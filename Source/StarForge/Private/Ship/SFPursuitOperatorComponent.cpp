#include "Ship/SFPursuitOperatorComponent.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFPilotStationComponent.h"
#include "Ship/SFShipDataAsset.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFThrusterArrayComponent.h"
#include "StarForgeLog.h"
#include "Units.h"

namespace
{
	/**
	 * The pursuit's negative control, and a NAMED EXCEPTION to ESFShipCrewing's rule that an
	 * Automated hull answers no sf.* variable -- the other one is sf.Shields. Here the automated
	 * hull is the ONLY one that reads it, because this operator holds a seat only there. The rule
	 * exists so that a command addressed to the player's ship does not also move the pirate;
	 * sf.PirateAI is addressed to something the player's ship does not have at all, so it cannot
	 * reach the wrong hull.
	 */
	TAutoConsoleVariable<int32> CVarPirateAI(TEXT("sf.PirateAI"), 1,
		TEXT("1: the pursuit operator flies the automated ship's pilot post. 0: it declines the post -- no zeroed\n")
		TEXT("intent, which with assist on would brake the ship -- and the ship coasts. Read by automated hulls only."),
		ECVF_Default);
}

USFPursuitOperatorComponent::USFPursuitOperatorComponent()
{
	// Nothing to do per frame of its own: the post asks it, in the post's tick. The seat is taken
	// once at BeginPlay, and nothing in this stage vacates it.
	PrimaryComponentTick.bCanEverTick = false;
}

void USFPursuitOperatorComponent::BeginPlay()
{
	Super::BeginPlay();

	// Silent on the player's ship. Both existing rigs spawn only that one, and a line from a
	// component that does nothing there would be a new line in every recorded run.
	if (bConsoleControlled)
	{
		return;
	}

	AActor* Owner = GetOwner();
	Frame = Owner ? Owner->FindComponentByClass<USFShipFrameComponent>() : nullptr;
	Thrusters = Owner ? Owner->FindComponentByClass<USFThrusterArrayComponent>() : nullptr;
	USFPilotStationComponent* Pilot = Owner ? Owner->FindComponentByClass<USFPilotStationComponent>() : nullptr;

	if (!Frame)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Pursuit operator on [%s] found no frame component, so it has no bearing to anything and will not fly."),
			*GetNameSafe(Owner));
	}

	if (!Pilot)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Pursuit operator on [%s] found no pilot station; nobody will fly this automated ship."),
			*GetNameSafe(Owner));
		return;
	}

	// Without asking any variable, as the automation takes the gunner's seat on this hull: its
	// posts are held by automation by definition. TryOccupy says who took it, or who refused.
	Pilot->TryOccupy(this);
}

bool USFPursuitOperatorComponent::MeasureTarget(double& OutDistanceMetres, StarForge::Turret::FTurretAim& OutBearing) const
{
	const AActor* Target = TargetActor.Get();
	if (!Frame || !Target)
	{
		return false;
	}

	// Through the frame facade, the one thing that knows where this hull is in the world.
	const FTransform Hull = Frame->GetFrameTransform();
	const FVector TargetCm = Target->GetActorLocation();
	if (!StarForge::Turret::ComputeBearing(Hull, TargetCm, OutBearing))
	{
		return false;
	}

	OutDistanceMetres = StarForge::Units::CmToMetres(FVector::Dist(Hull.GetLocation(), TargetCm));
	return true;
}

bool USFPursuitOperatorComponent::IsDeclining() const
{
	// Read live, not remembered from the last ask. The post stops asking whenever it stands aside --
	// a powered-down or locked hull -- and a remembered answer would then go on describing a switch
	// that has since been turned.
	return CVarPirateAI.GetValueOnGameThread() == 0;
}

void USFPursuitOperatorComponent::RefuseOnce(ERefusal Cause, const TCHAR* Why)
{
	const uint8 Bit = static_cast<uint8>(1 << static_cast<uint8>(Cause));
	if (ReportedRefusals & Bit)
	{
		return;
	}
	ReportedRefusals |= Bit;

	// Loud, because a pursuit operator that quietly declines looks exactly like a pirate that is
	// powered down or locked, and those are states of the ship where this is a fault of its setup.
	UE_LOG(LogStarForge, Error, TEXT("Pursuit: [%s] declines the pilot's post: %s"), *GetNameSafe(GetOwner()), Why);
}

bool USFPursuitOperatorComponent::ProvideFlightIntent(StarForge::FFlightIntent& Out)
{
	const bool bDeclineNow = IsDeclining();
	if (bDeclineNow != bDeclinedLastAsk)
	{
		bDeclinedLastAsk = bDeclineNow;
		UE_LOG(LogStarForge, Display, TEXT("Pursuit: [%s] %s"), *GetNameSafe(GetOwner()),
			bDeclinedLastAsk
				? TEXT("sf.PirateAI 0 -- the pursuit operator declines the pilot's post; nobody flies this ship and it coasts.")
				: TEXT("sf.PirateAI 1 -- the pursuit operator flies the pilot's post again."));
	}

	if (bDeclinedLastAsk)
	{
		// Declines rather than returning a zeroed intent. A zeroed FFlightIntent is hands
		// resting on the stick, and with assist on the ship would brake to a stop believing
		// it had been told to -- which looks exactly like a parked pirate and is not one.
		// The interface's default returns false for this reason; this honours it.
		// Said once per turn of the switch, not once a frame.
		return false;
	}

	// Nothing to chase is not a command to stop, for the reason above. Not logged here: the
	// Pursuit line under sf.FlightLog says "no target" once a second, from outside this call.
	double DistanceMetres = 0.0;
	StarForge::Turret::FTurretAim Bearing;
	if (!MeasureTarget(DistanceMetres, Bearing))
	{
		return false;
	}

	// The stick is a speed with assist on, and it is scaled by the ship's own limit, not a literal.
	if (!Thrusters)
	{
		RefuseOnce(ERefusal::NoThrusterArray, TEXT("it found no thruster array, so it has no speed limit to fly to."));
		return false;
	}
	if (!Thrusters->ShipData)
	{
		RefuseOnce(ERefusal::NoShipData, TEXT("the thruster array has no ship data asset, so there is no speed limit to fly to."));
		return false;
	}
	const double MaxSpeedMetresPerSec = Thrusters->ShipData->BuildLimits().MaxSpeed;
	if (!(MaxSpeedMetresPerSec > 0.0) || !FMath::IsFinite(MaxSpeedMetresPerSec))
	{
		RefuseOnce(ERefusal::BadMaxSpeed, TEXT("the ship data's MaxSpeed is not a finite positive number, and the stick is a fraction of it."));
		return false;
	}

	// Both are divisors or under a root below. ClampMin keeps an editor from typing zero; this keeps
	// anything else from getting one through.
	if (!(AlignAngleRad > 0.0) || !FMath::IsFinite(AlignAngleRad))
	{
		RefuseOnce(ERefusal::BadAlignAngle, TEXT("AlignAngleRad is not a finite positive angle, and the turn is divided by it."));
		return false;
	}
	if (!(BrakingDecelMetresPerSecSq > 0.0) || !FMath::IsFinite(BrakingDecelMetresPerSecSq))
	{
		RefuseOnce(ERefusal::BadBrakingDecel, TEXT("BrakingDecelMetresPerSecSq is not a finite positive deceleration, and the approach is shaped by it."));
		return false;
	}

	// Linear: ask for the speed from which a constant deceleration a stops the ship exactly at the
	// hold distance. Closing at v, it needs v^2 / 2a to stop, so asking for v = sqrt(2 a |e|), with
	// e the distance past the hold, keeps the gap left equal to the stopping distance all the way
	// in, and the ship arrives at 300 m at rest instead of at the speed it built up. The law this
	// replaced asked for full stick until 350 m; the pirate was measured there at 143 m/s, which
	// needs 640 m to stop at 16 m/s^2, and it flew through its target.
	//
	// a = 8 m/s^2, half of what the intact array gives: 800 kN on 50 t is 16. The half is the
	// margin the assist spends catching up to a speed it is always slightly behind, and it is what
	// keeps a damaged ship on the curve: one lost fore-and-aft thruster leaves 600 kN, 12 m/s^2,
	// which stops from v in v^2 / 24 -- still inside the v^2 / 16 the law assumes.
	//
	// The square root has an infinite slope at zero, which with the stick a speed demand is an
	// infinite gain at the hold distance. Inside DeadBandMetres the demand therefore falls
	// linearly from the curve's value at the band's edge, sqrt(2 x 8 x 50) = 28.3 m/s, to zero:
	// continuous at the edge, and finite at the hold.
	//
	// Every threshold is metres or seconds: 300 m, 50 m and 8 m/s^2.
	//
	// Scaled by how much of the hull's +X points at the target: max(0, cos of the off-axis angle),
	// which from the bearing is cos(pitch) cos(yaw). The stick moves the hull along its own X, and
	// only that share of the motion changes the distance. With the target behind, cos is negative
	// and forward thrust opens the gap it was asked to close -- the pirate used to accelerate away
	// for the whole of a turn -- so it gets nothing, and with assist on the ship holds still along
	// X while the angular stick brings the nose round. Backing off (e < 0) keeps the same rule for
	// the same reason with the sign turned over: reverse moves the hull along -X, which opens the
	// gap only while the target is ahead; with it behind, reversing would close the gap it is meant
	// to open, and zero is again the only demand whose sign cannot be wrong.
	//
	// Every other axis is zero: no roll and no strafe, which with assist on holds both still.
	Out = StarForge::FFlightIntent();
	const double PastHoldMetres = DistanceMetres - HoldDistanceMetres;
	const double WantMetresPerSec = FMath::Abs(PastHoldMetres) >= DeadBandMetres
		? FMath::Sign(PastHoldMetres) * FMath::Sqrt(2.0 * BrakingDecelMetresPerSecSq * FMath::Abs(PastHoldMetres))
		: (PastHoldMetres / DeadBandMetres) * FMath::Sqrt(2.0 * BrakingDecelMetresPerSecSq * DeadBandMetres);
	const double OnAxis = FMath::Max(0.0, FMath::Cos(Bearing.PitchRad) * FMath::Cos(Bearing.YawRad));
	Out.LinearAxis.X = FMath::Clamp(WantMetresPerSec / MaxSpeedMetresPerSec, -1.0, 1.0) * OnAxis;

	// Angular: proportional on the bearing, saturating at AlignAngleRad. With assist on, the
	// stick asks for a RATE, so this is a first-order approach with a time constant of
	// AlignAngleRad / MaxAngularRate -- a quarter second at spec 7's numbers.
	// NEGATED, and the sign is the whole line. The bearing's pitch is elevation, positive
	// toward +Z. An angular velocity about +Y turns +X toward -Z -- nose DOWN -- by the same
	// quaternion algebra FQuat::RotateVector uses, so feeding the bearing straight in would
	// pitch the nose away from a target above. Yaw keeps its sign: about +Z, +X turns toward
	// +Y, which is the bearing's own convention.
	Out.AngularAxis.Y = FMath::Clamp(-Bearing.PitchRad / AlignAngleRad, -1.0, 1.0);
	Out.AngularAxis.Z = FMath::Clamp(Bearing.YawRad / AlignAngleRad, -1.0, 1.0);
	Out.bFlightAssistEnabled = true;

	return true;
}
