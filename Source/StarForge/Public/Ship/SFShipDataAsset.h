#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FlightController.h"
#include "ThrusterDef.h"
#include "SFShipDataAsset.generated.h"

/**
 * One thruster, in a form the editor can show.
 *
 * StarForgeCore has no UObjects and so no reflection, which is deliberate -- it is
 * what keeps the maths testable without an engine. This mirrors FThrusterDef for
 * authoring and converts across at load.
 */
USTRUCT(BlueprintType)
struct STARFORGE_API FSFThrusterSpec
{
	GENERATED_BODY()

	/** Position in the ship frame, in METRES. Not centimetres -- the maths is SI. */
	UPROPERTY(EditAnywhere, Category = "Thruster")
	FVector LocationMetres = FVector::ZeroVector;

	/** Direction this thruster pushes the ship. Normalised on load. */
	UPROPERTY(EditAnywhere, Category = "Thruster")
	FVector Direction = FVector::ForwardVector;

	/** Thrust at full throttle, in newtons. */
	UPROPERTY(EditAnywhere, Category = "Thruster")
	double MaxThrustNewtons = 0.0;

	/** Seconds to spool from zero to full. */
	UPROPERTY(EditAnywhere, Category = "Thruster")
	double ResponseTimeSeconds = 0.05;
};

/**
 * A ship as data: what it weighs, what pushes it, and how hard it may be flown.
 *
 * Handling is not written here, it follows from the thruster layout. That is the
 * whole point of describing thrusters by geometry: change where they sit and the
 * ship flies differently without anyone touching the flight code.
 *
 * A layout that cannot produce all six axes is rejected rather than flown, because
 * a ship that silently refuses to turn is the hardest kind of bug to find. Note
 * that six axes needs opposed pairs: an array pushing only along +X, +Y and +Z can
 * never turn without also shoving the ship, since a throttle cannot go negative.
 */
UCLASS(BlueprintType)
class STARFORGE_API USFShipDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Kilograms. Declared here; Chaos derives inertia and centre of mass from it. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Ship")
	double MassKg = 50000.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Ship")
	TArray<FSFThrusterSpec> Thrusters;

	/** Manoeuvring ceiling from spec 7, metres per second. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Limits")
	double MaxSpeed = 200.0;

	/** Boost ceiling from spec 7, metres per second. Consumed by plan 2b. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Limits")
	double BoostSpeed = 400.0;

	/**
	 * Angular ceilings in degrees per second, as an angular velocity vector:
	 * X roll, Y pitch, Z yaw. Spec 7 gives 180 for roll and 120 for the other two,
	 * which is three numbers and must stay three.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Limits")
	FVector MaxAngularSpeedDegPerSec = FVector(180.0, 120.0, 120.0);

	UPROPERTY(EditAnywhere, Category = "StarForge|Limits")
	double LinearGain = 2.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Limits")
	double AngularGain = 3.0;

	/**
	 * Replaces Thrusters with the standard symmetric test layout: four corners, six
	 * thrusters each, one along every signed axis. Twenty-four records of four fields.
	 *
	 * It exists because hand-entering those twenty-four records invites a sign typo,
	 * and a sign typo does not announce itself -- it produces a ship that flies subtly
	 * wrong while looking correctly authored in the details panel.
	 *
	 * The opposed pairs are not decoration. A throttle is confined to [0, 1] because a
	 * thruster cannot run backwards, so an array pushing only along +X, +Y and +Z
	 * cannot produce torque without also shoving the ship: zeroing the net force zeroes
	 * every contributing throttle and takes the moment with it. Turning in place needs
	 * a force couple, and a couple needs two thrusters facing opposite ways.
	 * FThrustAllocator::Build rejects an array that cannot span all six axes, so a
	 * hand-authored layout that misses this does not fly at all.
	 *
	 * This is the same layout the headless solver tests are built on, so a rig filled
	 * by this button flies the arrangement the maths was verified against.
	 *
	 * MassKg and the limits are left alone -- the button fills the array and nothing else.
	 */
	UFUNCTION(CallInEditor, Category = "StarForge|Ship")
	void PopulateSymmetricLayout();

	/** The thruster set in the form the solver wants: SI, normalised directions. */
	TArray<StarForge::FThrusterDef> BuildThrusters() const;

	/** The flight envelope in the form the control loop wants: SI, radians. */
	StarForge::FFlightLimits BuildLimits() const;
};
