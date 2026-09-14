#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Ship/SFStationOperator.h"
#include "TurretMath.h"
#include "SFPursuitOperatorComponent.generated.h"

class USFShipFrameComponent;
class USFThrusterArrayComponent;

/**
 * An operator that chases. The fourth implementation of ISFStationOperator, and the first
 * that decides where to go rather than being told.
 *
 * It sits in a pilot's post exactly as the console operator and the player do, and the post
 * cannot tell the difference -- which is the claim spec 4 rests on and the reason there is no
 * ASFPirateShip anywhere in this branch. A pirate is ASFTestShip with ESFShipCrewing::Automated.
 *
 * It needs no new maths. StarForge::Turret::ComputeBearing already answers "which way is that
 * point, in my frame", and a hull is a mount with different stops; reusing it keeps
 * StarForgeCore untouched by this task and keeps one answer to one question.
 *
 * Every threshold here is in metres and seconds. HoldDistanceMetres is where it wants to be;
 * BrakingDecelMetresPerSecSq is the deceleration its approach is shaped for, so it arrives at
 * rest rather than at speed; DeadBandMetres is the band around the hold inside which the
 * approach speed ramps to zero instead of following that curve; and AlignAngleRad is the angle
 * at which it asks for full stick on a turn -- which with spec 7's 120 deg/s is a time constant
 * of 30/120 = 0.25 s. Not one of them is a frame count.
 *
 * It holds a seat only on a hull that does not answer sf.* (see ESFShipCrewing). On the player's
 * ship it is inert: the console operator or a person flies that one.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFPursuitOperatorComponent : public UActorComponent, public ISFStationOperator
{
	GENERATED_BODY()

public:
	USFPursuitOperatorComponent();

	virtual void BeginPlay() override;

	virtual bool ProvideFlightIntent(StarForge::FFlightIntent& Out) override;
	virtual FString GetOperatorName() const override { return TEXT("pursuit"); }

	/** Where it wants to be, from the target's centre, in METRES. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Pursuit")
	double HoldDistanceMetres = 300.0;

	/** Band around the hold distance inside which the approach speed ramps linearly to zero, in METRES. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Pursuit")
	double DeadBandMetres = 50.0;

	/**
	 * The deceleration the approach is shaped for, in METRES PER SECOND SQUARED: half of the
	 * intact array's 16, so a ship that has lost a fore-and-aft thruster (12) still stops inside
	 * the distance the law assumes. The derivation is where the law is (ProvideFlightIntent).
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Pursuit")
	double BrakingDecelMetresPerSecSq = 8.0;

	/** Bearing at which a turn asks for full stick, in RADIANS. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Pursuit")
	double AlignAngleRad = UE_DOUBLE_PI / 6.0;

	/** What to chase. Its actor location is the point: on ASFTestShip, the centre of the hull. */
	void SetTargetActor(AActor* InTarget) { TargetActor = InTarget; }

	AActor* GetTargetActor() const { return TargetActor.Get(); }

	/**
	 * Distance and bearing to the target from this hull, as they stand now. A pure read of two
	 * transforms, independent of whether the post asked for an intent this frame -- so the log
	 * can say how far away the target is while the operator declines, which is exactly what the
	 * sf.PirateAI control is judged on. False with no target, or with the target on the pivot.
	 */
	bool MeasureTarget(double& OutDistanceMetres, StarForge::Turret::FTurretAim& OutBearing) const;

	/** True from the ask on which sf.PirateAI read 0 until the ask on which it reads anything else. */
	bool IsDeclining() const { return bDeclining; }

	/**
	 * Whether this instance answers the unnamed sf.* family. See ESFShipCrewing.
	 *
	 * Inverted in effect for this operator: a hull that answers the console is the player's, and
	 * this component leaves its pilot's seat alone.
	 */
	void SetConsoleControlled(bool bIn) { bConsoleControlled = bIn; }

private:
	bool bConsoleControlled = true;

	UPROPERTY()
	TObjectPtr<USFShipFrameComponent> Frame;

	/** Where the ship's own speed limit is read from; the stick is a fraction of it. */
	UPROPERTY()
	TObjectPtr<USFThrusterArrayComponent> Thrusters;

	TWeakObjectPtr<AActor> TargetActor;

	/** Last reading of sf.PirateAI as seen by an ask, so a turn of the switch is said once. */
	bool bDeclining = false;
};
