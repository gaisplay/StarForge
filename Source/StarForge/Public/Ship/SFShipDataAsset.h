#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FlightController.h"
#include "ShipDamage.h"
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
	 * Metres per second along the barrel, added to the carrier's own velocity. Not set in
	 * the spec; this figure is the stage-4 plan's.
	 *
	 * 2.5 times spec 7's 400 m/s boost ceiling, so a ship cannot overtake its own shot. And
	 * at 60 fps a shot covers 1000 / 60 = 16.67 m a frame, twice the length of the eight-metre
	 * hull, which is what makes sf.ProjectileSweep 0 a control that can fail rather than one
	 * that passes by being too slow to tunnel.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Weapon")
	double MuzzleSpeedMetresPerSec = 1000.0;

	/**
	 * Seconds between shots. Not set in the spec; this figure is the stage-4 plan's.
	 *
	 * Five shots a second: twelve frames apart at 60 fps and eighty at 400, so shots are told
	 * apart in the log at both rates, and the count over a window has to agree between them --
	 * which is the check that the cadence is written in seconds and not in frames.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Weapon")
	double SecondsBetweenShots = 0.2;

	/**
	 * A shot's life in seconds. Not set in the spec; this figure is the stage-4 plan's.
	 *
	 * 1000 m/s x 5.0 s = 5000 m, five times the combat rig's one-kilometre separation, so a
	 * miss expires far outside the scene rather than hanging in it.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Weapon")
	double ProjectileLifeSeconds = 5.0;

	/**
	 * What one hit carries. Not set in the spec; this figure is the stage-4 plan's.
	 *
	 * A round hundred, so events can be counted in shots without a calculator: a wrong
	 * multiplier shows up as a wrong count rather than as a plausible-looking number.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Weapon")
	double ProjectileDamage = 100.0;

	/**
	 * What one shield face holds. Not set in the spec; this figure is the stage-4 plan's. Spec 9
	 * names hull and shield strength among what this asset describes, so it lives here and not
	 * in the ship's constructor.
	 *
	 * Exactly ten hits of ProjectileDamage take a face down, which is 2.0 s of a held trigger at
	 * SecondsBetweenShots: a face that falls in nine or eleven is a wrong count, not a plausible
	 * number.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Damage")
	double ShieldFaceCapacity = 1000.0;

	/**
	 * Charge a face regains per second once its delay has run. Not set in the spec; this figure
	 * is the stage-4 plan's. A face taken all the way down is full again 1000 / 50 = 20 s later.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Damage")
	double ShieldRegenPerSecond = 50.0;

	/**
	 * Seconds since the last hit on THAT face before it starts to regenerate. Not set in the spec;
	 * this figure is the stage-4 plan's. In seconds, and that is load-bearing rather than style:
	 * a delay counted in frames would be a shield that comes back sooner on a faster machine.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Damage")
	double ShieldRegenDelaySeconds = 3.0;

	/**
	 * The hull's strength. Not set in the spec; this figure is the stage-4 plan's. Twenty more hits
	 * after a face is down, 4.0 s at the weapon's cadence; a whole face and the hull together are
	 * 6.0 s of unbroken hits on one place. At zero the ship powers down rather than being destroyed.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Damage")
	double HullStrength = 2000.0;

	/**
	 * How far from a hull hit, in METRES, a thruster is destroyed by it (spec 5: a hit near a
	 * thruster destroys it). Not set in the spec; this figure is the stage-4 plan's.
	 *
	 * Metres because it is compared with FThrusterDef::Location, which is. On the standard layout
	 * the nearest point of the outer skin to any thruster is 2.1 m: a thruster at (2, 2, 0) and
	 * the +X face at 4.1, so 4.1 - 2.0. Below 2.1 no hit on the skin could ever reach one, and the
	 * mechanism would be dead code that looks alive. 2.5 clears that by 0.4 m and stays under
	 * 4.1 -- the distance from every thruster to the deck and the ceiling, since all twenty-four
	 * lie on z = 0 -- so a shot into the roof can never destroy one. That is a negative control
	 * the geometry gives for free, and a radius above 4.1 would throw it away. On the +X face the
	 * window around each corner is |y -+ 2| <= sqrt(2.5^2 - 2.1^2) = 1.356 m; the centre of the
	 * face, 2.90 m from both corners, destroys nothing.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Damage")
	double ThrusterKillRadiusMetres = 2.5;

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

	/** The three shield figures in the form the shield maths wants: the same for every face. */
	StarForge::Damage::FShieldLimits BuildShieldLimits() const;
};
