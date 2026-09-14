#include "Ship/SFGunnerStationComponent.h"

#include "GameFramework/Actor.h"
#include "Ship/SFTurretComponent.h"
#include "Ship/SFWeaponComponent.h"
#include "StarForgeLog.h"

USFGunnerStationComponent::USFGunnerStationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// The same group as the pilot station, and nothing orders the two. Nothing needs
	// to: this post turns a barrel and the pilot's post pushes the hull, and neither
	// reads in its tick anything the other writes in its own. The mount transform read
	// here is the hull's pre-physics pose whichever of them runs first. Should a post
	// ever read what another writes -- a sensor post feeding this one, say -- that is
	// the moment for AddTickPrerequisiteComponent, not for a comment claiming an order.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	StationName = TEXT("gunner");
}

void USFGunnerStationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Turret = Owner->FindComponentByClass<USFTurretComponent>();
		Weapon = Owner->FindComponentByClass<USFWeaponComponent>();
	}

	if (!Turret)
	{
		// Loudly: a gunner station with nothing to command looks exactly like a turret
		// that will not move, and the readout cannot tell the two apart on its own.
		UE_LOG(LogStarForge, Error,
			TEXT("Gunner station on [%s] found no turret; nothing will be aimed."), *GetNameSafe(GetOwner()));
	}

	if (!Weapon)
	{
		// A warning, not an error: a gunner's post without a turret is useless, and one
		// without a muzzle still aims.
		UE_LOG(LogStarForge, Warning,
			TEXT("Gunner station on [%s] found no weapon; the turret will aim and nothing will fire."),
			*GetNameSafe(GetOwner()));
	}
}

void USFGunnerStationComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The weapon is told the trigger on every path out of this tick, and told it is released
	// exactly where the turret is told nobody commanded it. Without that, a post that empties
	// leaves the trigger held from its last tick and the weapon keeps firing on a stale
	// reading -- the same failure that cost the thruster array a whole function (ClearDemand)
	// to remove.
	if (!Turret)
	{
		LastIntent = StarForge::FTurretIntent();
		bOperatorGunning = false;
		if (Weapon) { Weapon->ApplyTrigger(false, DeltaTime); }
		return;
	}

	if (!IsOccupied())
	{
		LastIntent = StarForge::FTurretIntent();
		bOperatorGunning = false;
		Turret->ClearCommand();
		if (Weapon) { Weapon->ApplyTrigger(false, DeltaTime); }
		return;
	}

	// Not checked against sf.ShipProfile, unlike the pilot station. The scripted
	// profile writes the hull's velocity and a wrench would fight it; this post writes
	// a barrel's rotation, and the two never meet. A gunner works while the rig flies
	// itself, and that is correct rather than an omission.

	StarForge::FTurretIntent Intent;
	if (!Operator->ProvideTurretIntent(Intent))
	{
		// An operator who is not gunning says so. The turret is told nobody commanded
		// it, which is the truth, rather than being left holding last tick's state.
		LastIntent = StarForge::FTurretIntent();
		bOperatorGunning = false;
		Turret->ClearCommand();
		if (Weapon) { Weapon->ApplyTrigger(false, DeltaTime); }
		return;
	}

	LastIntent = Intent;
	bOperatorGunning = true;

	// With no target the trigger still goes through as the operator holds it, the same value
	// the turret records: the weapon fires on the trigger, not on the aim.
	if (!Intent.bHasTarget)
	{
		Turret->ApplyNoTarget(Intent.bFiring);
		if (Weapon) { Weapon->ApplyTrigger(Intent.bFiring, DeltaTime); }
		return;
	}

	// The demand, in the executor's own frame: the same division of labour as the
	// pilot station computing a wrench for the array to apply.
	StarForge::Turret::FTurretAim Bearing;
	if (!StarForge::Turret::ComputeBearing(Turret->GetMountTransform(), Intent.TargetWorldLocation, Bearing))
	{
		// A target on the mount itself is not a bearing. Reported as no target rather
		// than aimed at with a made-up angle.
		Turret->ApplyNoTarget(Intent.bFiring);
		if (Weapon) { Weapon->ApplyTrigger(Intent.bFiring, DeltaTime); }
		return;
	}

	// After the bearing, so the muzzle a shot leaves from is where this tick's slew put it.
	Turret->ApplyBearing(Bearing, Intent.bFiring, DeltaTime);
	if (Weapon) { Weapon->ApplyTrigger(Intent.bFiring, DeltaTime); }
}
