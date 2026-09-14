#include "Ship/SFPursuitOperatorComponent.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFPilotStationComponent.h"
#include "Ship/SFShipFrameComponent.h"
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

bool USFPursuitOperatorComponent::ProvideFlightIntent(StarForge::FFlightIntent& Out)
{
	const bool bDeclineNow = CVarPirateAI.GetValueOnGameThread() == 0;
	if (bDeclineNow != bDeclining)
	{
		bDeclining = bDeclineNow;
		UE_LOG(LogStarForge, Display, TEXT("Pursuit: [%s] %s"), *GetNameSafe(GetOwner()),
			bDeclining
				? TEXT("sf.PirateAI 0 -- the pursuit operator declines the pilot's post; nobody flies this ship and it coasts.")
				: TEXT("sf.PirateAI 1 -- the pursuit operator flies the pilot's post again."));
	}

	if (bDeclining)
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

	// Linear: full stick beyond hold + dead band, full reverse inside hold - dead band, and in
	// proportion between. With assist on the stick asks for a speed, so between the two it asks
	// for a speed in proportion rather than coasting, and at the hold distance itself to stop.
	// 300 m and 50 m, so full stick past 350 and full reverse inside 250.
	// Every other axis is zero: no roll and no strafe, which with assist on holds both still.
	Out = StarForge::FFlightIntent();
	Out.LinearAxis.X = FMath::Clamp((DistanceMetres - HoldDistanceMetres) / DeadBandMetres, -1.0, 1.0);

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
