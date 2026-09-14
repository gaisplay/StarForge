#include "Ship/SFWeaponComponent.h"

#include "Ballistics.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFProjectile.h"
#include "Ship/SFShipDataAsset.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFTurretComponent.h"
#include "StarForgeLog.h"
#include "Units.h"

namespace
{
	/**
	 * -1: no override, the trigger is whatever the post's operator asks for. 0: hold fire.
	 * 1: hold the trigger down -- the operator's trigger, never the operator; see ApplyTrigger.
	 *
	 * A latched variable rather than a held key, and that is what makes the manual controls
	 * possible at all: checkpoint 6 found a step that asked an operator to hold thrust and
	 * type at the console at the same time, which two hands cannot do. A latched trigger
	 * leaves both hands free.
	 *
	 * It is ignored by an Automated hull (see ESFShipCrewing), so a person latching his own
	 * trigger does not also silence the pirate.
	 */
	TAutoConsoleVariable<int32> CVarFire(TEXT("sf.Fire"), -1,
		TEXT("-1: the trigger is whatever the gunner post's operator asks for, on a weapons-free hull.\n")
		TEXT("0: hold fire. 1: hold the trigger down while an operator holds the post. Ignored by an automated hull."),
		ECVF_Default);

	/**
	 * One line per shot. Off by default because at five a second it is a stream; hits are
	 * events and are logged whatever this says. Not addressed to a hull: it moves nothing,
	 * and every line it writes carries the name of the ship that fired.
	 */
	TAutoConsoleVariable<int32> CVarWeaponDebug(TEXT("sf.WeaponDebug"), 0,
		TEXT("1: log every shot -- time, muzzle, velocity and the turret's state when it left."),
		ECVF_Default);

namespace WeaponCadence
{
	/**
	 * How far short of a full interval a shot still counts as due, in seconds.
	 *
	 * The engine hands a tick its dt as a float, and a float frame is not the frame the
	 * command line asked for: 0.0025f is 0.00249999994, and eighty of them sum to
	 * 0.1999999955, short of 0.2. Compared exactly, the shot after a press waited 81 frames at
	 * 400 fps instead of 80 -- 2.5 ms late -- while at 60 fps 1/60f lies above 1/60 and twelve
	 * frames landed on time. A microsecond is some two hundred times that shortfall and 2500
	 * times shorter than a frame at 400 fps, so it absorbs the rounding without ever letting a
	 * shot out a frame early.
	 */
	constexpr double ShotDueToleranceSeconds = 1.0e-6;
}
}

USFWeaponComponent::USFWeaponComponent()
{
	// Driven by the gunner post, not by its own tick.
	PrimaryComponentTick.bCanEverTick = false;
}

void USFWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	const AActor* Owner = GetOwner();
	Frame = Owner ? Owner->FindComponentByClass<USFShipFrameComponent>() : nullptr;

	for (USceneComponent* Parent = GetAttachParent(); Parent && !Mount; Parent = Parent->GetAttachParent())
	{
		Mount = Cast<USFTurretComponent>(Parent);
	}

	if (!ShipData)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Weapon: [%s] has no ship data asset, so no muzzle speed and no cadence; it will not fire."),
			*GetNameSafe(Owner));
		return;
	}

	if (!(ShipData->SecondsBetweenShots > 0.0))
	{
		// Refused rather than clamped: the trigger loop below subtracts this figure until the
		// accumulator is under it, and a figure that is not positive never gets there.
		UE_LOG(LogStarForge, Error,
			TEXT("Weapon: [%s] SecondsBetweenShots is %.3f, which is not a cadence; it will not fire."),
			*GetNameSafe(Owner), ShipData->SecondsBetweenShots);
		return;
	}

	SecondsSinceShot = ShipData->SecondsBetweenShots;

	if (!Frame)
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Weapon: [%s] found no frame component; its shots will leave at muzzle speed alone, ")
			TEXT("without the carrier's velocity."),
			*GetNameSafe(Owner));
	}

	// Once, so a run's log says what this weapon was and whether it could fire -- the line
	// that tells a silent weapon on a measurement rig from a broken one.
	const FVector Rel = GetRelativeLocation();
	UE_LOG(LogStarForge, Display,
		TEXT("Weapon: [%s] muzzle at (%.0f %.0f %.0f) cm on %s: %.0f m/s, a shot every %.2f s, ")
		TEXT("life %.2f s (%.0f m), damage %.1f; weapons %s, %s."),
		*GetNameSafe(Owner), Rel.X, Rel.Y, Rel.Z, *GetNameSafe(GetAttachParent()),
		ShipData->MuzzleSpeedMetresPerSec, ShipData->SecondsBetweenShots,
		ShipData->ProjectileLifeSeconds, ShipData->MuzzleSpeedMetresPerSec * ShipData->ProjectileLifeSeconds,
		ShipData->ProjectileDamage,
		bWeaponsFree ? TEXT("free") : TEXT("hold"),
		bConsoleControlled ? TEXT("answers sf.Fire") : TEXT("deaf to sf.Fire"));
}

void USFWeaponComponent::ApplyTrigger(bool bTriggerHeld, double DeltaSeconds)
{
	// The one place sf.Fire is applied. See the declaration for why it is here and not in
	// ReleaseTrigger.
	const int32 Override = bConsoleControlled ? CVarFire.GetValueOnGameThread() : -1;
	const bool bTriggerDown =
		Override == 1 ? true
		: Override == 0 ? false
		: (bWeaponsFree && bTriggerHeld);

	TickCadence(bTriggerDown, DeltaSeconds);
}

void USFWeaponComponent::ReleaseTrigger(double DeltaSeconds)
{
	TickCadence(false, DeltaSeconds);
}

void USFWeaponComponent::TickCadence(bool bTriggerDown, double DeltaSeconds)
{
	if (!ShipData || !(ShipData->SecondsBetweenShots > 0.0))
	{
		return;
	}
	const double SecondsBetweenShots = ShipData->SecondsBetweenShots;

	// By time, with the interval subtracted rather than the accumulator zeroed: zeroing would
	// throw away each frame's remainder and make the rate depend on dt.
	SecondsSinceShot += DeltaSeconds;
	if (!bTriggerDown)
	{
		bTriggerWasHeld = false;
		return;
	}
	if (!bTriggerWasHeld)
	{
		// Capped to one interval on the tick the trigger goes down, for two reasons. Left to
		// grow while the trigger was up, the accumulator would fire fifteen shots in this one
		// frame after a three-second pause -- a burst whose size is the length of the pause.
		// And capped only while up, this tick's own dt would ride on top of the full interval
		// as a remainder, bringing the second shot one frame early: 17 ms at 60 fps, 2.5 at 400.
		SecondsSinceShot = FMath::Min(SecondsSinceShot, SecondsBetweenShots);
		bTriggerWasHeld = true;
	}
	while (SecondsSinceShot >= SecondsBetweenShots - WeaponCadence::ShotDueToleranceSeconds)
	{
		SecondsSinceShot -= SecondsBetweenShots;
		FireOne();
	}
}

void USFWeaponComponent::FireOne()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Muzzle = GetComponentLocation();
	const FVector Barrel = GetComponentQuat().GetForwardVector();

	// From the frame facade, not from Chaos. A docked hull is kinematic and its physics
	// velocity is zero while the hull is genuinely moving with the station; the frame derives
	// its velocity by differencing transforms and is right in both cases. One instrument.
	FVector CarrierCms = FVector::ZeroVector;
	if (Frame)
	{
		const FVector CarrierMetres = Frame->GetLinearVelocityMetersPerSec();
		CarrierCms = FVector(
			StarForge::Units::MetresToCm(CarrierMetres.X),
			StarForge::Units::MetresToCm(CarrierMetres.Y),
			StarForge::Units::MetresToCm(CarrierMetres.Z));
	}

	const FVector VelocityCms = StarForge::Ballistics::ComputeMuzzleVelocity(
		CarrierCms, Barrel, StarForge::Units::MetresToCm(ShipData->MuzzleSpeedMetresPerSec));

	// Read after the decision to fire has been made, never before it, and only to be written
	// down: which shots may be compared depends on the barrel's state when each one left.
	const FString TurretState = Mount
		? FString::Printf(TEXT("%s (aim yaw %.2f pitch %.2f deg)"),
			USFTurretComponent::StateName(Mount->GetState()),
			FMath::RadiansToDegrees(Mount->GetAim().YawRad),
			FMath::RadiansToDegrees(Mount->GetAim().PitchRad))
		: FString(TEXT("(no turret)"));

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASFProjectile* Shot = World->SpawnActor<ASFProjectile>(ASFProjectile::StaticClass(), FTransform(Muzzle), Params);
	if (!Shot)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Weapon: [%s] a projectile failed to spawn; the shot was not fired."), *GetNameSafe(GetOwner()));
		return;
	}

	++ShotsFired;
	Shot->SetShotRecord(ShotsFired, TurretState);
	Shot->Launch(GetOwner(), Muzzle, VelocityCms, ShipData->ProjectileDamage, ShipData->ProjectileLifeSeconds);

	if (CVarWeaponDebug.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogStarForge, Display,
			TEXT("Weapon: [%s] shot %d at t %.3f s from muzzle (%.3f %.3f %.3f) cm, velocity (%.1f %.1f %.1f) m/s, ")
			TEXT("turret %s."),
			*GetNameSafe(GetOwner()), ShotsFired, World->GetTimeSeconds(),
			Muzzle.X, Muzzle.Y, Muzzle.Z,
			StarForge::Units::CmToMetres(VelocityCms.X),
			StarForge::Units::CmToMetres(VelocityCms.Y),
			StarForge::Units::CmToMetres(VelocityCms.Z),
			*TurretState);
	}
}
