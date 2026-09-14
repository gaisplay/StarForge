#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SFWeaponComponent.generated.h"

class USFShipDataAsset;
class USFShipFrameComponent;
class USFTurretComponent;

/**
 * The muzzle: the place a shot leaves from, and the cadence it leaves at.
 *
 * A scene component rather than an actor because a muzzle is a PLACE. It hangs off the
 * barrel the turret turns, so wherever the picture says the barrel points is where the
 * shot goes, by construction rather than by a second calculation that could disagree.
 *
 * It does not tick. The gunner's post drives it, as spec 4 has a post drive the systems it
 * commands, and it does not know who pulled the trigger.
 *
 * Its figures come from the ship data asset (spec 9): the ship actor carries the data out,
 * it does not hold it.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFWeaponComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USFWeaponComponent();

	virtual void BeginPlay() override;

	/** Muzzle speed, cadence, life and damage. Without it the weapon does not fire, and says so. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Weapon")
	TObjectPtr<USFShipDataAsset> ShipData;

	/**
	 * One tick of the trigger. Fires while bTriggerHeld, no faster than one shot per
	 * SecondsBetweenShots -- seconds, so the shot count over a second is the same at 60 and
	 * at 400 fps. A cadence counted in frames would be a weapon whose rate of fire is the
	 * machine's.
	 *
	 * It does NOT ask the turret whether the barrel is on target. Firing only when on target
	 * would hide aiming defects: a mount that never finishes slewing would simply stop
	 * shooting, and that reads as "the weapon is broken" rather than "the turret does not
	 * aim". Whether to hold the trigger is the operator's judgement and stays where it is.
	 */
	void ApplyTrigger(bool bTriggerHeld, double DeltaSeconds);

	FVector GetMuzzleWorldLocation() const { return GetComponentLocation(); }

	int32 GetShotsFired() const { return ShotsFired; }

	/** Whether this instance answers sf.Fire. See ESFShipCrewing. */
	void SetConsoleControlled(bool bIn) { bConsoleControlled = bIn; }

	/**
	 * Whether the operator's trigger reaches the muzzle at all. See ASFTestShip::bWeaponsFree:
	 * off, only sf.Fire 1 on a hull that answers the console makes this weapon fire.
	 */
	void SetWeaponsFree(bool bIn) { bWeaponsFree = bIn; }

private:
	void FireOne();

	UPROPERTY()
	TObjectPtr<USFShipFrameComponent> Frame;

	/** The mount this muzzle hangs from, read only to write its state into the shot's record. */
	UPROPERTY()
	TObjectPtr<USFTurretComponent> Mount;

	/**
	 * Time toward the next shot. BeginPlay sets it to a full interval, so a weapon that has
	 * not fired fires on the first tick its trigger is held.
	 */
	double SecondsSinceShot = 0.0;

	/** Whether the trigger was down on the last tick, so the tick it goes down can be told apart. */
	bool bTriggerWasHeld = false;

	int32 ShotsFired = 0;

	bool bConsoleControlled = true;
	bool bWeaponsFree = false;
};
