#include "Ship/SFTurretComponent.h"

#include "GameFramework/Actor.h"
#include "StarForgeLog.h"

USFTurretComponent::USFTurretComponent()
{
	// Driven by the post that owns it, not by its own tick.
	PrimaryComponentTick.bCanEverTick = false;
}

void USFTurretComponent::BeginPlay()
{
	Super::BeginPlay();

	// Once, so a run's log says what turret it was flying, the same way the ship says
	// its physics settings. Relative to the parent, which on the rig is the hull, so
	// these are ship-local centimetres.
	const FVector Rel = GetRelativeLocation();
	UE_LOG(LogStarForge, Display,
		TEXT("Turret mount on [%s] at ship-local (%.0f %.0f %.0f) cm: yaw +/-%.0f, pitch %.0f..%.0f deg, ")
		TEXT("traverse %.0f deg/s, on target within %.1f deg, %s."),
		*GetNameSafe(GetOwner()),
		Rel.X, Rel.Y, Rel.Z, MaxYawDegrees, MinPitchDegrees, MaxPitchDegrees,
		TraverseRateDegPerSec, OnTargetToleranceDegrees,
		Barrel ? TEXT("barrel attached") : TEXT("no barrel (numbers only)"));
}

void USFTurretComponent::SetBarrel(USceneComponent* InBarrel)
{
	Barrel = InBarrel;
	RotateBarrel();
}

StarForge::Turret::FTurretLimits USFTurretComponent::BuildLimits() const
{
	// The one place degrees become radians for this component.
	StarForge::Turret::FTurretLimits Limits;
	Limits.MaxYawRad = FMath::DegreesToRadians(MaxYawDegrees);
	Limits.MinPitchRad = FMath::DegreesToRadians(MinPitchDegrees);
	Limits.MaxPitchRad = FMath::DegreesToRadians(MaxPitchDegrees);
	Limits.TraverseRateRadPerSec = FMath::DegreesToRadians(TraverseRateDegPerSec);
	Limits.OnTargetToleranceRad = FMath::DegreesToRadians(OnTargetToleranceDegrees);
	return Limits;
}

void USFTurretComponent::ApplyBearing(
	const StarForge::Turret::FTurretAim& Bearing, bool bInFiring, double DeltaSeconds)
{
	// Cleared rather than merely ignored, as the thruster array does: a locked turret that kept
	// the last bearing and trigger would print them as a live command.
	if (bLocked)
	{
		ClearCommand();
		return;
	}

	LastBearing = Bearing;
	bHasBearing = true;
	bFiring = bInFiring;

	const StarForge::Turret::FAimStep Step =
		StarForge::Turret::StepAim(Aim, Bearing, BuildLimits(), DeltaSeconds);
	Aim = Step.Aim;

	switch (Step.State)
	{
	case StarForge::Turret::EAimState::OnTarget:
		State = ESFTurretState::OnTarget;
		break;
	case StarForge::Turret::EAimState::CannotBear:
		State = ESFTurretState::CannotBear;
		break;
	default:
		State = ESFTurretState::Slewing;
		break;
	}

	RotateBarrel();
}

void USFTurretComponent::ApplyNoTarget(bool bInFiring)
{
	if (bLocked)
	{
		ClearCommand();
		return;
	}

	// The barrel holds where it is. Swinging it home would read as movement on a
	// turret that has nothing to move for.
	bHasBearing = false;
	bFiring = bInFiring;
	State = ESFTurretState::NoTarget;
}

void USFTurretComponent::ClearCommand()
{
	bHasBearing = false;
	bFiring = false;
	State = ESFTurretState::Uncommanded;
}

void USFTurretComponent::SetLocked(bool bInLocked)
{
	bLocked = bInLocked;
}

void USFTurretComponent::RotateBarrel()
{
	if (Barrel)
	{
		// The same conversion the tests round-trip: whatever this rotator does to the
		// barrel, ComputeBearing reads back as the aim, so the picture and the numbers
		// cannot disagree about which way the barrel points.
		Barrel->SetRelativeRotation(StarForge::Turret::AimToRotator(Aim));
	}
}

const TCHAR* USFTurretComponent::StateName(ESFTurretState InState)
{
	switch (InState)
	{
	case ESFTurretState::Uncommanded: return TEXT("UNCOMMANDED");
	case ESFTurretState::NoTarget: return TEXT("NO TARGET");
	case ESFTurretState::Slewing: return TEXT("slewing");
	case ESFTurretState::OnTarget: return TEXT("ON TARGET");
	case ESFTurretState::CannotBear: return TEXT("CANNOT BEAR");
	}
	return TEXT("?");
}
