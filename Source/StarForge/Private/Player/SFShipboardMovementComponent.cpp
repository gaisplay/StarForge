#include "Player/SFShipboardMovementComponent.h"

#include "CoreGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Ship/SFShipFrameComponent.h"

namespace
{
	/**
	 * How long after the player stops commanding movement before drift measurement
	 * resumes, in seconds. Long enough for his own momentum to brake out, short
	 * enough that a one-minute standing test is nearly all measurement.
	 */
	constexpr double SettleSeconds = 0.5;
}

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
}

void USFShipboardMovementComponent::SetShipFrame(USFShipFrameComponent* NewFrame)
{
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

	RestartMeasurementInterval();
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

	DetectCarryGap();

	// CMC decides what is walkable by comparing a floor normal against gravity, and
	// gravity points at world down until something says otherwise. A deck rolling at
	// the design's 180 deg/s crosses the 44.8 degree walkable limit in a quarter
	// second, after which the character falls, loses its base, and the engine stops
	// calling this function at all -- taking the carry and the whole measurement with
	// it. Pointing gravity along the ship's own down is what the spec's grav plates
	// mean in engine terms.
	SetGravityDirection(-ShipFrame->GetFrameTransform().GetUnitAxis(EAxis::Z));

	CarryWithFrame();
	UpdateDriftMeasurement(DeltaSeconds);
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

	// The view is stored ship-local: the player keeps looking at the same
	// bulkhead no matter what the ship does, and the stars sweep past outside.
	if (ACharacter* Owner = Cast<ACharacter>(GetOwner()))
	{
		if (AController* Controller = Owner->GetController())
		{
			const FQuat FrameDelta =
				CurrentFrameTransform.GetRotation() * LastFrameTransform.GetRotation().Inverse();

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
}

void USFShipboardMovementComponent::RestartMeasurementInterval()
{
	// Peaks and clock go together or not at all. Carrying peaks across a restart
	// would judge a thirty-second worst case against a minute's budget; carrying
	// the clock across one would let two short stands add up to a valid reading
	// that neither of them was.
	DriftCm = 0.0;
	PeakDriftCm = 0.0;
	PeakJitterCm = 0.0;
	MeasuredSeconds = 0.0;
}
