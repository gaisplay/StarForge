#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "TurretMath.h"
#include "SFTurretComponent.generated.h"

/**
 * Five states, where the maths has three. The maths only ever sees a bearing: on it,
 * slewing to it, or unable to reach it. The executor also knows two cases the maths
 * never sees, and both have to be named: a barrel standing still because nobody
 * commanded it looks exactly like one standing still because it was told to hold.
 */
UENUM()
enum class ESFTurretState : uint8
{
	/** No post commanded this turret on the last tick. */
	Uncommanded,
	/** Commanded, but the operator had nothing to point at. */
	NoTarget,
	Slewing,
	OnTarget,
	CannotBear
};

/**
 * The executor the gunner post commands: a mount with stops and a slew rate, and a
 * barrel it turns.
 *
 * It does not know who is aiming or why. It is handed a bearing in its own frame,
 * moves toward it as far as its limits and this tick's time allow, and remembers what
 * state that left it in. That separation is the same one the thruster array keeps
 * from the pilot: a player, a crewman and the ship's automation share this one path.
 *
 * It aims; it does not shoot. FTurretIntent carries the trigger and this records it,
 * so the whole path from operator to executor is exercised, and stage 4 adds a muzzle.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFTurretComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USFTurretComponent();

	virtual void BeginPlay() override;

	/** Yaw travel each way from straight ahead. Degrees here; radians below the boundary. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Turret")
	double MaxYawDegrees = 120.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Turret")
	double MinPitchDegrees = -30.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Turret")
	double MaxPitchDegrees = 60.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Turret")
	double TraverseRateDegPerSec = 90.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Turret")
	double OnTargetToleranceDegrees = 0.5;

	/** The visible barrel this mount turns. Optional: the maths and the readout need none. */
	void SetBarrel(USceneComponent* InBarrel);

	/** The limits in the form the maths wants: radians. */
	StarForge::Turret::FTurretLimits BuildLimits() const;

	/** Where the mount is in the world right now; the post takes the bearing from here. */
	FTransform GetMountTransform() const { return GetComponentTransform(); }

	/** One tick toward a bearing: slews within the limits, turns the barrel, records the state. */
	void ApplyBearing(const StarForge::Turret::FTurretAim& Bearing, bool bInFiring, double DeltaSeconds);

	/** Commanded with nothing to point at: the barrel holds, and the state says why. */
	void ApplyNoTarget(bool bInFiring);

	/** Nobody commanded this tick. */
	void ClearCommand();

	/**
	 * Locks the turret: ApplyBearing and ApplyNoTarget become no-ops that also clear the
	 * command, so the readout says nobody commanded it.
	 *
	 * The same shape as the thruster array's lock and for the same reason: the turret refuses,
	 * not the post and not the operator. The post asks whether its turret is locked, and neither
	 * it nor whoever holds it has to know why. A powered-down ship locks it. Docking does not:
	 * a docked ship still lays its gun, and the loop rig's figures were taken with it doing so.
	 */
	void SetLocked(bool bInLocked);

	bool IsLocked() const { return bLocked; }

	StarForge::Turret::FTurretAim GetAim() const { return Aim; }
	StarForge::Turret::FTurretAim GetLastBearing() const { return LastBearing; }
	bool HasBearing() const { return bHasBearing; }
	ESFTurretState GetState() const { return State; }
	bool IsFiring() const { return bFiring; }

	/** Upper case for the states a reader should notice; the calm one is lower case. */
	static const TCHAR* StateName(ESFTurretState InState);

private:
	void RotateBarrel();

	UPROPERTY()
	TObjectPtr<USceneComponent> Barrel;

	StarForge::Turret::FTurretAim Aim;
	StarForge::Turret::FTurretAim LastBearing;
	bool bHasBearing = false;
	ESFTurretState State = ESFTurretState::Uncommanded;
	bool bFiring = false;

	/** True while something has locked the turret. See SetLocked. */
	bool bLocked = false;
};
