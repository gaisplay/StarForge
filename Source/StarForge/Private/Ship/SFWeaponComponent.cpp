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
	 * How far short of its due time a shot still counts as due: half the step, so a shot goes
	 * out on the frame NEAREST its due time rather than on the first frame at or after it.
	 *
	 * The schedule in TickCadence is absolute -- shot n is due n intervals after the press, and
	 * no shot's time is derived from the one before it -- so the only error left is the frame
	 * grid, and half a step is the smallest error that grid admits.
	 *
	 * No fixed tolerance can do that, and the microsecond that stood here could not. The engine
	 * hands a tick its dt as a float, and float(1/400) is 0.0024999999441, short of 1/400 by
	 * 5.59e-11: the game's own clock runs 22 ns slow per second at 400 fps, so 0.2 s of it is
	 * 80.0000018 frames and the due time leaves the frame grid by 4.47e-9 an interval. A fixed
	 * margin is therefore a budget and not a bound -- a microsecond's worth is spent after 224
	 * intervals, 44.7 s of unbroken fire, after which one interval takes 81 frames. Simulated in
	 * double over a 60 s run with a float dt: with a microsecond the 224th gap is 81 frames,
	 * with half a step every gap is 80, and 60 fps is 12 frames throughout either way because
	 * float(1/60) lies above 1/60.
	 *
	 * Capped at half an interval, so a step longer than the cadence cannot turn the lookahead
	 * into a second shot on the tick the trigger goes down.
	 */
	double ShotDueToleranceSeconds(double DeltaSeconds, double SecondsBetweenShots)
	{
		return 0.5 * FMath::Min(DeltaSeconds, SecondsBetweenShots);
	}
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
		// Refused rather than clamped: the trigger loop below fires while the clock has reached
		// the next multiple of this figure, and a figure that is not positive is reached by
		// every multiple at once.
		UE_LOG(LogStarForge, Error,
			TEXT("Weapon: [%s] SecondsBetweenShots is %.3f, which is not a cadence; it will not fire."),
			*GetNameSafe(Owner), ShipData->SecondsBetweenShots);
		return;
	}

	SecondsSinceTrigger = ShipData->SecondsBetweenShots;

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

	// The hulls' clock, which is the shot's (ASFProjectile::PhysicsStepSeconds). Kept on the frame,
	// one 0.3 s frame would put two shots out of the muzzle in the same tick, from the same pose at
	// the same velocity, where the cadence wants them 200 m apart.
	const double StepSeconds = ASFProjectile::PhysicsStepSeconds(DeltaSeconds);

	// One clock since the press, and the count of shots taken from it. Shot n is due n intervals
	// after the press, and that due time is computed rather than carried: no shot's timing
	// descends from the one before it, so a whole run has the residual of a single interval.
	// An accumulator with the interval subtracted off at each shot is the same arithmetic
	// written backwards and was simulated frame-for-frame identical to this, down to the same
	// 81-frame gap at shot 224; it reads, wrongly, as though the remainder were what kept time.
	SecondsSinceTrigger += StepSeconds;
	if (!bTriggerDown)
	{
		bTriggerWasHeld = false;
		return;
	}
	if (!bTriggerWasHeld)
	{
		// The press starts a new schedule, from the time since the last SHOT rather than from
		// zero, capped at one interval. Both halves carry load. Uncapped, a three-second pause
		// would fire fifteen shots in this one frame -- a burst whose size is the length of the
		// pause. Started from zero, releasing and pressing again would be a way to fire faster
		// than the cadence, and this tick's own dt would ride on top of the full interval,
		// bringing the second shot one frame early: 17 ms at 60 fps, 2.5 at 400.
		SecondsSinceTrigger = FMath::Min(
			SecondsSinceTrigger - ShotsSincePress * SecondsBetweenShots, SecondsBetweenShots);
		ShotsSincePress = 0;
		bTriggerWasHeld = true;
	}

	const double DueTolerance = WeaponCadence::ShotDueToleranceSeconds(StepSeconds, SecondsBetweenShots);
	while (SecondsSinceTrigger >= (ShotsSincePress + 1) * SecondsBetweenShots - DueTolerance)
	{
		++ShotsSincePress;
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
