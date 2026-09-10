#include "Player/SFShipboardMovementComponent.h"

#include "CoreGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFShipFrameComponent.h"
#include "StarForgeLog.h"
#include "Units.h"

namespace
{
	/**
	 * How long after the player stops commanding movement before drift measurement
	 * resumes, in seconds. Long enough for his own momentum to brake out, short
	 * enough that a one-minute standing test is nearly all measurement.
	 */
	constexpr double SettleSeconds = 0.5;

	/**
	 * How long continuously under the grav-plate limit before a knocked-down occupant
	 * gets his feet back. The same 0.5 the FFootingState tests use.
	 */
	constexpr double KnockdownRecoverySeconds = 0.5;

	/**
	 * Lateral friction while he slides, per second. ApplyVelocityBraking multiplies
	 * this by BrakingFrictionFactor, which the engine defaults to 2.0
	 * (CharacterMovementComponent.cpp 734, 4384-4385), so the coefficient that acts is
	 * 4/s and a steady push along the deck settles at a quarter of the excess: the
	 * spec's worst case, 19.86 m/s^2 four metres from the axis in a full roll, slides
	 * him at 5 m/s.
	 *
	 * It bounds the part of the push lying along the deck and nothing else. PhysFalling
	 * hands this friction to CalcVelocity for ProjectToGravityFloor(Velocity) alone
	 * (cpp 4896-4936), leaving the component along the ship's own down to the floor and
	 * the ceiling to stop. Measured under the scripted profile, whose push swings
	 * through every axis at 216 deg/s: a knocked-down occupant reached 358 m/s inside an
	 * eight-metre cabin, so this number is not what keeps him off the walls -- the walls
	 * are. Do not read it as a speed limit on a knockdown.
	 */
	constexpr float KnockdownSlideFriction = 2.0f;
}

static TAutoConsoleVariable<int32> CVarShipGravity(
	TEXT("sf.ShipGravity"),
	1,
	TEXT("1: align the occupant's gravity with the ship's own down, which is what keeps a rolling deck walkable.\n")
	TEXT("0: leave gravity pointing at world down, which detaches the occupant within about a quarter second of rolling.\n")
	TEXT("Set to 0 to run the rig as a negative control and confirm the drift gauge can report a failure at all."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarDriftDebug(
	TEXT("sf.DriftDebug"),
	0,
	TEXT("1: log how many times the carry runs per engine frame, the movement mode, the movement base and the ship-local position -- once a second, and immediately on any change of mode or base.\n")
	TEXT("Diagnostic only, and off by default: during a real measurement it is noise, and the summary line already says what a good run looks like."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarKnockdown(
	TEXT("sf.Knockdown"),
	1,
	TEXT("1: an occupant on his feet is knocked down when the felt acceleration exceeds the grav-plate limit (spec 6): falling, walking refused, still carried.\n")
	TEXT("0: he keeps his feet whatever the ship does. Needed to reproduce checkpoint 1's baseline, whose scripted profile is over the limit almost continuously, ")
	TEXT("and for the sf.ShipGravity 0 negative control, which the knockdown would otherwise mask by keeping the movement base."),
	ECVF_Default);

USFShipboardMovementComponent::USFShipboardMovementComponent()
{
	// This flag does nothing on the frame path. CarryWithFrame applies the ship's
	// rotation itself and never calls Super::UpdateBasedMovement, so the stock
	// yaw-follow that this flag suppresses cannot run there in the first place.
	// Where it does take effect is the fallback path, and there it silently drops
	// view-follow for a character riding an ordinary rotating platform. The rig has
	// exactly one moving base and it is the ship, so nothing in it reaches that
	// case; a game with lifts and turntables in it would not get off so lightly.
	bIgnoreBaseRotation = true;

	// The slide has to stop somewhere short of the far wall on its own. The default,
	// zero, would let a steady push accelerate him without limit; see
	// KnockdownSlideFriction for the number and the factor hidden in it.
	FallingLateralFriction = KnockdownSlideFriction;
}

void USFShipboardMovementComponent::SetShipFrame(USFShipFrameComponent* NewFrame)
{
	USFShipFrameComponent* OldFrame = ShipFrame.Get();

	// A handover is a change of carrier, not a rebinding to the same one. Walking back
	// into a volume he never left fires the overlap again, and treating that as a
	// handover would reset the drift instrument every time he crossed his own doorway.
	if (OldFrame && NewFrame && OldFrame != NewFrame)
	{
		// The angle between the two decks' down vectors. It is the number spec 8 means by
		// "the gravity direction changes only by the leftover tolerance": mated ports
		// hold both +Z parallel to within the up-angle limit, so a person walking through
		// the airlock should read a fraction of a degree here and never ten.
		const FVector OldDown = -OldFrame->GetFrameTransform().GetUnitAxis(EAxis::Z);
		const FVector NewDown = -NewFrame->GetFrameTransform().GetUnitAxis(EAxis::Z);

		PendingHandoverOldName = OldFrame->GetOwner()
			? OldFrame->GetOwner()->GetActorNameOrLabel() : TEXT("none");
		PendingHandoverNewName = NewFrame->GetOwner()
			? NewFrame->GetOwner()->GetActorNameOrLabel() : TEXT("none");
		PendingHandoverGravityTurnDeg = FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(FVector::DotProduct(OldDown, NewDown), -1.0, 1.0)));
		bHandoverPending = true;
	}

	ShipFrame = NewFrame;

	if (NewFrame)
	{
		LastFrameTransform = NewFrame->GetFrameTransform();
	}

	SecondsSinceCommandedInput = 0.0;
	bBaselineValid = false;

	LastCarryFrame = 0;
	SkippedCarryFrames = 0;
	bHasCarriedOnce = false;

	// A new frame is a new footing. The flag is the one that keeps the base through a
	// knockdown, and it must not outlive the knockdown it was set for.
	Footing = StarForge::ShipFrame::FFootingState();
	bStayBasedInAir = false;
	FeltAccelMetersPerSecSq = FVector::ZeroVector;
	PushMetersPerSecSq = FVector::ZeroVector;
	CurrentKnockdownSeconds = 0.0;
	LastFootingFrame = TNumericLimits<uint64>::Max();

	RestartMeasurementInterval();

	// The latched result belongs to whichever frame produced it. Binding to a new
	// (or the same) frame invalidates it, and this is the only place that does --
	// an ordinary interval restart must never touch it, or the operator's result
	// would vanish the instant he takes a step.
	BestCompletedSeconds = 0.0;
	BestCompletedPeakDriftCm = 0.0;
	BestCompletedPeakJitterCm = 0.0;
	BestCompletedKnockedDownSeconds = 0.0;

	// The cross-check belongs to the binding it was taken against.
	MismatchFrames = 0;
	LastCrossCheckFrame = TNumericLimits<uint64>::Max();
	SecondsSinceMismatchLog = 0.0;
}

FString USFShipboardMovementComponent::GetFrameName() const
{
	const USFShipFrameComponent* Frame = ShipFrame.Get();
	if (!Frame || !Frame->GetOwner())
	{
		return TEXT("none");
	}

	return Frame->GetOwner()->GetActorNameOrLabel();
}

int64 USFShipboardMovementComponent::GetFramesSinceLastCarry() const
{
	if (!bHasCarriedOnce)
	{
		return 0;
	}

	return static_cast<int64>(GFrameCounter) - static_cast<int64>(LastCarryFrame);
}

FVector USFShipboardMovementComponent::GetLocalLocation() const
{
	if (!ShipFrame.IsValid() || !UpdatedComponent)
	{
		return FVector::ZeroVector;
	}

	return ShipFrame->WorldToLocal(UpdatedComponent->GetComponentTransform()).GetLocation();
}

void USFShipboardMovementComponent::UpdateBasedMovement(float DeltaSeconds)
{
	if (!ShipFrame.IsValid() || !UpdatedComponent)
	{
		Super::UpdateBasedMovement(DeltaSeconds);
		return;
	}

	if (CVarDriftDebug.GetValueOnGameThread() != 0)
	{
		LogDiagnostics(DeltaSeconds);
	}

	DetectCarryGap();
	UpdateFrameCrossCheck(DeltaSeconds);

	// CMC decides what is walkable by comparing a floor normal against gravity, and
	// gravity points at world down until something says otherwise. A deck rolling at
	// the design's 180 deg/s crosses the 44.8 degree walkable limit in a quarter
	// second, after which the character falls, loses its base, and the engine stops
	// calling this function at all -- taking the carry and the whole measurement with
	// it. Pointing gravity along the ship's own down is what the spec's grav plates
	// mean in engine terms. Gated on sf.ShipGravity so the rig can be run as a
	// negative control: with it off, gravity is left at world down and the deck
	// detaches the occupant on its own, proving the drift gauge can report a
	// failure and is not just an instrument that has only ever read zero.
	if (CVarShipGravity.GetValueOnGameThread() != 0)
	{
		SetGravityDirection(-ShipFrame->GetFrameTransform().GetUnitAxis(EAxis::Z));
	}

	CarryWithFrame();

	// Footing before the measurement, so the figure taken this frame already carries
	// what this frame decided about him.
	UpdateFooting(DeltaSeconds);
	UpdateDriftMeasurement(DeltaSeconds);
}

void USFShipboardMovementComponent::LogDiagnostics(float DeltaSeconds)
{
	// Calls per engine frame. UpdateDriftMeasurement adds DeltaSeconds once per
	// call, so if this is ever above one the measured clock runs fast by exactly
	// that factor and every duration on the results form is wrong by it.
	const uint64 ThisFrame = GFrameCounter;
	if (ThisFrame == DebugLastFrame)
	{
		++DebugCallsThisFrame;
	}
	else
	{
		DebugCallsThisFrame = 1;
		DebugLastFrame = ThisFrame;
	}
	DebugCallsPerFramePeak = FMath::Max(DebugCallsPerFramePeak, DebugCallsThisFrame);

	const UObject* Base = GetMovementBaseObject();
	const FString State = FString::Printf(TEXT("%s|%s"),
		*GetMovementName(), Base ? *Base->GetName() : TEXT("none"));

	// Only on the frame's first call. This function runs twice per frame for the same
	// reason UpdateDriftMeasurement does, and accumulating on both ran this throttle at
	// exactly twice real time: a line every half second under a comment promising one a
	// second, so twenty-five seconds of run produced fifty lines and anyone counting
	// them to recover a duration got double. The doubled clock this project already
	// found once, relocated into the log's own pacing. The call counter above is
	// deliberately not guarded -- counting the calls is what it is for.
	if (DebugCallsThisFrame == 1)
	{
		DebugSecondsSinceLog += DeltaSeconds;
	}

	// One line a second, and one the instant the mode or the base changes. The
	// transitions are where the answer is, and a once-a-second sample steps straight
	// over the quarter second in which the occupant comes off a rolling deck.
	const bool bStateChanged = State != DebugLastState;
	if (!bStateChanged && DebugSecondsSinceLog < 1.0)
	{
		return;
	}

	const FVector Local = ShipFrame.IsValid() ? GetLocalLocation() : FVector::ZeroVector;

	UE_LOG(LogStarForge, Log,
		TEXT("Drift debug%s: calls/frame %d peak %d, dt %.4f, mode %s, base %s, ")
		TEXT("ship-local (%.0f %.0f %.0f) dist %.0f cm, settled %.1f s, measured %.1f s, ship gravity %d"),
		bStateChanged ? TEXT(" [CHANGED]") : TEXT(""),
		DebugCallsThisFrame, DebugCallsPerFramePeak, DeltaSeconds,
		*GetMovementName(),
		Base ? *Base->GetName() : TEXT("none"),
		Local.X, Local.Y, Local.Z, Local.Size(),
		SecondsSinceCommandedInput, MeasuredSeconds,
		CVarShipGravity.GetValueOnGameThread());

	DebugLastState = State;
	DebugSecondsSinceLog = 0.0;
}

void USFShipboardMovementComponent::DetectCarryGap()
{
	const uint64 ThisFrame = GFrameCounter;

	if (bHasCarriedOnce && ThisFrame > LastCarryFrame + 1)
	{
		// The engine stopped calling us for a stretch -- the occupant was not attached.
		// Carrying through a ship pose that old would map him through hundreds of metres
		// of stale motion, so re-seed instead and make the gap visible: a skipped carry
		// is the single most important thing this instrument can report.
		SkippedCarryFrames += static_cast<int64>(ThisFrame - LastCarryFrame - 1);
		LastFrameTransform = ShipFrame->GetFrameTransform();
		bBaselineValid = false;

		// He has just landed, and the velocity that came with it is not drift.
		// Restart the settle window too, or the next whole interval banks the
		// landing as a failure.
		SecondsSinceCommandedInput = 0.0;
	}

	LastCarryFrame = ThisFrame;
	bHasCarriedOnce = true;
}

void USFShipboardMovementComponent::UpdateFrameCrossCheck(float DeltaSeconds)
{
	const uint64 ThisFrame = GFrameCounter;
	if (ThisFrame == LastCrossCheckFrame)
	{
		return;
	}
	LastCrossCheckFrame = ThisFrame;

	// The engine already knows what he is standing on. The base is a component, its owner
	// is an actor, and that actor either carries the frame he rides or it does not.
	const UActorComponent* Base = Cast<UActorComponent>(GetMovementBaseObject());
	const AActor* BaseOwner = Base ? Base->GetOwner() : nullptr;
	const USFShipFrameComponent* BaseFrame =
		BaseOwner ? BaseOwner->FindComponentByClass<USFShipFrameComponent>() : nullptr;

	// No base, or a base whose owner carries no frame at all, is not a mismatch. The
	// first is a carry failure and has two instruments of its own; the second is a crate
	// or a drone, and standing on one says nothing about which ship you belong to.
	if (!BaseFrame || BaseFrame == ShipFrame.Get())
	{
		MismatchFrames = 0;
		SecondsSinceMismatchLog = 0.0;
		return;
	}

	++MismatchFrames;
	SecondsSinceMismatchLog += DeltaSeconds;

	// The first line at ten frames -- a sixth of a second, long enough that a single
	// frame of tick order cannot produce it -- and then one a second for as long as it
	// lasts. The same reason Carry NOT RUNNING repeats: a single line at frame ten does
	// not say that the disagreement has been going on for a minute.
	if (MismatchFrames == 10 || SecondsSinceMismatchLog >= 1.0)
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Standing on %s's floor while bound to %s's frame for %d frames. ")
			TEXT("One of the two is wrong, and the carry is following the frame."),
			BaseOwner ? *BaseOwner->GetActorNameOrLabel() : TEXT("nothing"),
			*GetFrameName(), MismatchFrames);
		SecondsSinceMismatchLog = 0.0;
	}
}

void USFShipboardMovementComponent::CarryWithFrame()
{
	const FTransform CurrentFrameTransform = ShipFrame->GetFrameTransform();
	const FTransform OccupantWorld = UpdatedComponent->GetComponentTransform();

	const FTransform Carried =
		ShipFrame->CarryOccupant(LastFrameTransform, OccupantWorld);

	const FVector Delta = Carried.GetLocation() - OccupantWorld.GetLocation();

	// bSweep is false deliberately. Relative to the ship this displacement is
	// zero, so there is nothing along the path that could legitimately stop it.
	MoveUpdatedComponent(
		Delta,
		Carried.GetRotation(),
		/*bSweep=*/false,
		/*OutHit=*/nullptr,
		ETeleportType::TeleportPhysics);

	// The instrument spec 8 asks for, and it measures the carry's own delta rather than
	// the difference between two positions. Between the binding and this call the man may
	// have taken a step of his own, and his step is his; the carry's delta is exactly the
	// part the handover caused.
	//
	// With both carriers docked it is required to be zero, because neither is moving with
	// respect to the other -- which is "visually unnoticeable" written as a number. It is
	// zero for a WRONG binding to a still carrier too, and that is why the negative
	// controls are judged on the cross-check and on the carry, not on this line.
	if (bHandoverPending)
	{
		LastHandoverJumpCm = Delta.Size();
		LastHandoverGravityTurnDeg = PendingHandoverGravityTurnDeg;
		LastHandoverText = FString::Printf(TEXT("%s -> %s, %.3f cm, %.1f deg"),
			*PendingHandoverOldName, *PendingHandoverNewName,
			LastHandoverJumpCm, LastHandoverGravityTurnDeg);

		UE_LOG(LogStarForge, Display,
			TEXT("Frame handover: %s -> %s, first carry moved him %.3f cm, gravity turned %.1f deg"),
			*PendingHandoverOldName, *PendingHandoverNewName,
			LastHandoverJumpCm, LastHandoverGravityTurnDeg);

		bHandoverPending = false;
	}

	const FQuat FrameDelta =
		CurrentFrameTransform.GetRotation() * LastFrameTransform.GetRotation().Inverse();

	// Velocity is frame-relative too, and CMC keeps it in world space. While the
	// ship turns under an occupant who is walking, his velocity goes on pointing
	// where the world used to be and his controls skew -- worse the faster he
	// moves. Position and view were already carried; this is the third of three.
	Velocity = FrameDelta.RotateVector(Velocity);

	// The view is stored ship-local: the player keeps looking at the same
	// bulkhead no matter what the ship does, and the stars sweep past outside.
	if (ACharacter* Owner = Cast<ACharacter>(GetOwner()))
	{
		if (AController* Controller = Owner->GetController())
		{
			const FQuat NewView = FrameDelta * Controller->GetControlRotation().Quaternion();
			Controller->SetControlRotation(NewView.Rotator());
		}
	}

	LastFrameTransform = CurrentFrameTransform;

	// Keep the engine's own base bookkeeping consistent even though the stock
	// carry path was bypassed.
	SaveBaseLocation();
}

void USFShipboardMovementComponent::UpdateDriftMeasurement(float DeltaSeconds)
{
	// Once a frame, whatever the engine does. UpdateBasedMovement runs twice per
	// frame, and this function used to add DeltaSeconds on each call, so the clock
	// ran at exactly twice real time: a reported minute was thirty seconds, and the
	// fifty-five second validity gate opened after twenty-eight. The ship does not
	// move between the two calls, so the second sample is identical to the first and
	// dropping it costs nothing.
	const uint64 ThisFrame = GFrameCounter;
	if (ThisFrame == LastMeasuredFrame)
	{
		return;
	}
	LastMeasuredFrame = ThisFrame;

	// Only commanded input counts as intent. Residual velocity must never excuse a
	// reading: uncommanded motion is precisely what this instrument exists to catch,
	// and gating on it let a gross failure suppress its own measurement.
	if (GetCurrentAcceleration().IsNearlyZero())
	{
		SecondsSinceCommandedInput += DeltaSeconds;
	}
	else
	{
		SecondsSinceCommandedInput = 0.0;
	}

	const FVector NowLocal = GetLocalLocation();

	// The ship's origin is a legal place to stand, so GetLocalLocation cannot signal
	// failure by returning zero. Refuse to baseline until there is a capsule to
	// measure, or the first reading would bank the occupant's whole offset from
	// origin into two peaks that never come back down.
	if (!bBaselineValid)
	{
		if (UpdatedComponent)
		{
			DriftReferenceLocal = NowLocal;
			LastLocalLocation = NowLocal;
			bBaselineValid = true;
			RestartMeasurementInterval();
		}
		return;
	}

	if (SecondsSinceCommandedInput < SettleSeconds)
	{
		// His own momentum is still bleeding off. Re-baseline rather than bank
		// walking as drift, and drop the interval with it: the peaks are read
		// beside the duration and judged against a per-minute budget, so the two
		// have to describe the same unbroken stretch of standing still.
		DriftReferenceLocal = NowLocal;
		LastLocalLocation = NowLocal;
		RestartMeasurementInterval();
		return;
	}

	DriftCm = FVector::Dist(NowLocal, DriftReferenceLocal);
	PeakDriftCm = FMath::Max(PeakDriftCm, DriftCm);

	// Jitter is what the eye notices even when the average position is perfect.
	const double JitterCm = FVector::Dist(NowLocal, LastLocalLocation);
	PeakJitterCm = FMath::Max(PeakJitterCm, JitterCm);

	LastLocalLocation = NowLocal;
	MeasuredSeconds += DeltaSeconds;

	// One line per ten seconds of accumulated measured time, so a run's outcome is
	// on disk even if nobody transcribes the HUD before it resets. Deliberately not
	// logged every tick: at 60 Hz that would spam the log without adding anything
	// the summary line does not already say.
	while (MeasuredSeconds >= NextPeriodicLogSeconds)
	{
		UE_LOG(LogStarForge, Log,
			TEXT("Shipboard drift: %.1f s settled, drift now %.3f cm, peak drift %.3f cm, ")
			TEXT("peak jitter %.3f cm/tick, skipped carries %lld, knocked down %.1f s -- %s"),
			MeasuredSeconds, DriftCm, PeakDriftCm, PeakJitterCm, SkippedCarryFrames,
			KnockedDownSeconds, *DescribeFooting());
		NextPeriodicLogSeconds += 10.0;
	}
}

void USFShipboardMovementComponent::RestartMeasurementInterval()
{
	// The interval about to be cleared is complete. Latch it if it beats the best
	// one seen so far, so the operator's result survives the very keypress that is
	// about to erase the live figures below. Compared by duration, not recency: a
	// stray two-second interval right after a good minute must not overwrite it.
	if (MeasuredSeconds > BestCompletedSeconds)
	{
		BestCompletedSeconds = MeasuredSeconds;
		BestCompletedPeakDriftCm = PeakDriftCm;
		BestCompletedPeakJitterCm = PeakJitterCm;
		BestCompletedKnockedDownSeconds = KnockedDownSeconds;
	}

	// A completed interval worth reading is one line on disk, so a run's outcome
	// can be checked after the fact even if the HUD's live figures were never
	// written down. Five seconds excludes the routine restarts every settle-and-step
	// produces, which would otherwise spam the log with nothing worth reading.
	if (MeasuredSeconds > 5.0)
	{
		UE_LOG(LogStarForge, Log,
			TEXT("Shipboard drift interval complete: %.1f s, peak drift %.3f cm, ")
			TEXT("peak jitter %.3f cm/tick, skipped carries %lld, knocked down %.1f s -- %s"),
			MeasuredSeconds, PeakDriftCm, PeakJitterCm, SkippedCarryFrames,
			KnockedDownSeconds, *DescribeFooting());
	}

	// Peaks and clock go together or not at all. Carrying peaks across a restart
	// would judge a thirty-second worst case against a minute's budget; carrying
	// the clock across one would let two short stands add up to a valid reading
	// that neither of them was.
	DriftCm = 0.0;
	PeakDriftCm = 0.0;
	PeakJitterCm = 0.0;
	MeasuredSeconds = 0.0;
	NextPeriodicLogSeconds = 10.0;
	KnockedDownSeconds = 0.0;
}

bool USFShipboardMovementComponent::IsKnockdownEnabled() const
{
	return CVarKnockdown.GetValueOnGameThread() != 0;
}

void USFShipboardMovementComponent::UpdateFooting(float DeltaSeconds)
{
	// Once a frame, for the reason UpdateDriftMeasurement gives: the engine calls
	// UpdateBasedMovement twice a frame, and a push added on both calls would be twice
	// the push the maths says -- the doubled clock all over again, in velocity. The
	// same guard makes the call from TickComponent free whenever the carry has already
	// settled his footing this frame.
	const uint64 ThisFrame = GFrameCounter;
	if (ThisFrame == LastFootingFrame)
	{
		return;
	}
	LastFootingFrame = ThisFrame;

	if (!ShipFrame.IsValid() || !UpdatedComponent)
	{
		return;
	}

	// Read once: the two questions below are asked about the same instant, and nothing
	// between them can change the answer.
	const bool bHasBase = GetMovementBaseObject() != nullptr;

	// The flag keeps the movement base through the fall it was set for, and nothing
	// else. With the base gone there is nothing left to keep, and a flag still on would
	// survive into the next walking-to-falling transition -- the one the negative
	// control depends on losing its base at.
	if (bStayBasedInAir && !bHasBase)
	{
		bStayBasedInAir = false;
	}

	// Everything in the ship's frame, in SI, from the one instrument. The frame
	// component samples in TG_PostPhysics, so on this pre-physics call these are last
	// frame's figures; a frame of lag against a half-second hysteresis is nothing.
	const FTransform FrameTransform = ShipFrame->GetFrameTransform();
	const FVector LocalOmega = FrameTransform.InverseTransformVectorNoScale(ShipFrame->GetAngularVelocityRadPerSec());
	const FVector LocalAccel = FrameTransform.InverseTransformVectorNoScale(ShipFrame->GetLinearAccelMetersPerSecSq());

	const FVector LocalCm = GetLocalLocation();
	const FVector LocalOffsetMetres(
		StarForge::Units::CmToMetres(LocalCm.X),
		StarForge::Units::CmToMetres(LocalCm.Y),
		StarForge::Units::CmToMetres(LocalCm.Z));

	FeltAccelMetersPerSecSq = StarForge::ShipFrame::ComputeFeltAcceleration(LocalAccel, LocalOmega, LocalOffsetMetres);

	// Strapped in, or with the mechanic off, nothing is over the limit as far as
	// footing is concerned. Routed through Update rather than skipped, so a man who is
	// down when the switch is thrown stands up through the same half second as
	// everyone else, not by a second path.
	//
	// No movement base is the third of those: a man with nothing under his feet has no
	// footing to lose, and the mechanic must not be the reason he never gets one. While
	// he is down IsValidLandingSpot refuses him a landing, landing is the only way to a
	// base, and the base is what the carry needs -- so a knockdown taken before he ever
	// had one strands him for the rest of the session. Measured, not argued: the rig's
	// occupant spawns a hand's breadth above the deck and falls to it over nine frames,
	// and with the scripted profile already over the limit he was knocked down on frame
	// three, never landed, was never carried once in twenty-five seconds, and the ship
	// left at 282 m/s without him. The state still advances here whether he is based or
	// not, which is what stands him up again after the deck has let go of him.
	const bool bOverLimit = IsKnockdownEnabled() && !bStrappedIn && bHasBase
		&& StarForge::ShipFrame::ExceedsGravPlateLimit(FeltAccelMetersPerSecSq);

	const bool bWasDown = Footing.bDown;
	Footing.Update(bOverLimit, DeltaSeconds, KnockdownRecoverySeconds);

	if (Footing.bDown && !bWasDown)
	{
		// Set BEFORE the mode change. OnMovementModeChanged reads it on entry to
		// falling and, with it on, skips both the imparting of base velocity and the
		// clearing of the base (CharacterMovementComponent.cpp 1454-1471). The base is
		// what MaybeUpdateBasedMovement checks before calling UpdateBasedMovement at
		// all; losing it stops the carry, and stopping the carry at 400 m/s is not
		// knocking a man down, it is deleting him.
		bStayBasedInAir = true;
		SetMovementMode(MOVE_Falling);
		CurrentKnockdownSeconds = 0.0;

		UE_LOG(LogStarForge, Display,
			TEXT("Footing LOST at t=%.2f s, ship-local (%.0f %.0f %.0f) cm: felt %.2f m/s2 over the %.2f limit, ")
			TEXT("ship turning %.1f deg/s; falling, walking refused, still carried."),
			GetWorld()->GetTimeSeconds(), LocalCm.X, LocalCm.Y, LocalCm.Z,
			FeltAccelMetersPerSecSq.Size(), StarForge::ShipFrame::GravPlateLimitMetersPerSecSq,
			FMath::RadiansToDegrees(LocalOmega.Size()));
	}

	if (Footing.bDown)
	{
		// What the plates could not absorb, reversed, as this frame's change of
		// velocity. Straight into Velocity rather than AddForce: PerformMovement applies
		// accumulated forces after this call and before StartNewPhysics (2840, 2871,
		// 3034), so either would land in this frame's PhysFalling, but AddForce also
		// divides by the character's Mass on the way and this is an acceleration already.
		PushMetersPerSecSq = StarForge::ShipFrame::ComputeUncompensatedAcceleration(FeltAccelMetersPerSecSq);

		const FVector PushWorld = FrameTransform.TransformVectorNoScale(PushMetersPerSecSq);
		Velocity += FVector(
			StarForge::Units::MetresToCm(PushWorld.X),
			StarForge::Units::MetresToCm(PushWorld.Y),
			StarForge::Units::MetresToCm(PushWorld.Z)) * DeltaSeconds;

		KnockedDownSeconds += DeltaSeconds;
		CurrentKnockdownSeconds += DeltaSeconds;
		return;
	}

	PushMetersPerSecSq = FVector::ZeroVector;

	if (bWasDown)
	{
		// Off the moment his footing returns. The only transition that reads the flag
		// is walking into falling, and until he lands there is none. Left on for good
		// it would hide the negative control: a deck made unwalkable by sf.ShipGravity 0
		// would keep its base instead of losing it, and the control would show nothing.
		bStayBasedInAir = false;

		UE_LOG(LogStarForge, Display,
			TEXT("Footing regained at t=%.2f s after %.1f s down, ship-local (%.0f %.0f %.0f) cm; landing is the engine's again."),
			GetWorld()->GetTimeSeconds(), CurrentKnockdownSeconds, LocalCm.X, LocalCm.Y, LocalCm.Z);

		CurrentKnockdownSeconds = 0.0;
	}
}

bool USFShipboardMovementComponent::IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const
{
	// PhysFalling asks this on every blocking hit and lands him the moment it says
	// yes (cpp 5050-5054): in the same frame, under the very acceleration that took
	// his feet. With no, the hit goes to HandleImpact, then ComputeSlideVector, and the
	// velocity is re-derived from the slide (5078, 5122, 5135): while he is down the
	// deck is something to slide along, not something to stand on.
	if (Footing.bDown)
	{
		return false;
	}

	return Super::IsValidLandingSpot(CapsuleLocation, Hit);
}

void USFShipboardMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	// The one landing the answer above cannot refuse: a character wedged between two
	// walls is landed by PhysFalling without asking (cpp 5221-5225). Landing is
	// SetMovementMode(MOVE_Walking), and walking takes its base from the floor under
	// him (1442-1452) -- a man pinned in a ceiling corner has none, and would come off
	// the ship. While he is down there is no landing by any path; the time is spent
	// falling, as the stock function would have spent it after landing.
	if (Footing.bDown)
	{
		StartNewPhysics(remainingTime, Iterations);
		return;
	}

	Super::ProcessLanded(Hit, remainingTime, Iterations);
}

void USFShipboardMovementComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The footing again, from the one path that runs whether or not he has a movement
	// base. With a base the carry has already decided it this frame and the frame guard
	// makes this free; without one the engine never calls UpdateBasedMovement at all
	// (cpp 2476-2487), and a man left down for want of a call would never walk or land
	// again, since IsValidLandingSpot refuses him a landing while he is down.
	UpdateFooting(DeltaTime);

	// After the engine's own tick, so "did the carry run this frame" has its answer.
	LogOpenCarryGap(DeltaTime);
}

void USFShipboardMovementComponent::LogOpenCarryGap(float DeltaSeconds)
{
	if (!ShipFrame.IsValid())
	{
		return;
	}

	// The HUD's threshold, for the HUD's reason: one frame is tick order, two is a gap.
	const int64 Frames = GetFramesSinceLastCarry();
	if (Frames <= 1)
	{
		SecondsSinceGapLog = 0.0;
		return;
	}

	// The first line the moment the gap is seen, then one a second. This is the log's
	// only word on a base loss that never ends: DetectCarryGap counts skipped carries
	// from inside the carry, and a carry the engine has stopped calling counts nothing.
	SecondsSinceGapLog += DeltaSeconds;
	if (Frames == 2 || SecondsSinceGapLog >= 1.0)
	{
		const UObject* Base = GetMovementBaseObject();
		UE_LOG(LogStarForge, Warning,
			TEXT("Carry NOT RUNNING: %lld frames since the occupant was last carried (mode %s, base %s). ")
			TEXT("He has come off the ship -- this is the breakage, not a knockdown."),
			Frames, *GetMovementName(), Base ? *Base->GetName() : TEXT("none"));
		SecondsSinceGapLog = 0.0;
	}
}

FString USFShipboardMovementComponent::DescribeFooting() const
{
	// In order of what invalidates what. Skipped carries are the defect this
	// instrument exists to find and outrank everything; the mechanic being off makes
	// the run a different rig; a knockdown explains a drift figure that is otherwise
	// over budget; only then is the figure the carry's own.
	if (SkippedCarryFrames > 0)
	{
		return TEXT("CARRY FAILED: the skipped carries are the defect");
	}
	if (!IsKnockdownEnabled())
	{
		return TEXT("knockdown off (sf.Knockdown 0)");
	}

	// The state as well as the interval's tally, because the tally alone cannot see a
	// man who is on the deck right now: RestartMeasurementInterval drops it with the
	// peaks on every frame of the settle window, so through that half second the
	// verdict would read "footing intact" beside a readout already saying he is down.
	if (Footing.bDown || KnockedDownSeconds > 0.0)
	{
		return TEXT("KNOCKED DOWN: the drift is his slide, not the carry");
	}

	return TEXT("footing intact");
}
