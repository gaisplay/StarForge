#pragma once

#include "CoreMinimal.h"

/**
 * A projectile in flight: where it is, where it is going, and how long it has been going.
 *
 * This is the first thing in the project that LEAVES its carrier's reference frame. A shot
 * inherits the ship's velocity at the muzzle and afterwards owns nothing of the ship: it is
 * a world-frame object, and the ship it came from may turn, roll or stop without it
 * noticing. Everything else aboard is carried; this is not, and that difference is the
 * reason a projectile was chosen over an instant trace.
 *
 * Length is unit-agnostic and follows whatever the caller passes, as in ShipFrameMath and
 * DockingMath; the game passes centimetres. Time is seconds, always, and the field says so
 * in its name -- a lifetime in frames is a lifetime that depends on the machine.
 *
 * No gravity and no drag. Not a simplification: the hull it was fired from has both turned
 * off explicitly (SetEnableGravity(false), SetLinearDamping(0.0)), and a shot that fell or
 * slowed would contradict the ship that fired it. Straight-line motion is also what makes
 * frame-rate independence provable rather than measurable: N steps of T/N land where one
 * step of T does up to floating-point accumulation and nothing else, and the tests' tolerance
 * sits about five orders of magnitude above that accumulation.
 */
namespace StarForge::Ballistics
{
	struct STARFORGECORE_API FProjectileState
	{
		/**
		 * Zeroed, not left to FVector's default constructor, which initialises nothing. A
		 * state built with only a velocity set would otherwise start from whatever the
		 * stack held.
		 */
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		double AgeSeconds = 0.0;
	};

	/** One step: the segment the caller sweeps this frame, and the state after it. */
	struct STARFORGECORE_API FProjectileStep
	{
		FVector SegmentStart = FVector::ZeroVector;
		FVector SegmentEnd = FVector::ZeroVector;
		FProjectileState NextState;
		bool bExpired = false;
	};

	/**
	 * The shot's world velocity as it leaves the muzzle: the carrier's velocity plus
	 * MuzzleSpeed along the barrel. Added, not substituted -- a shot fired astern from a
	 * ship faster than the muzzle speed still travels forward in the world.
	 *
	 * BarrelDirection is normalised here rather than trusted, so muzzle speed cannot depend
	 * on which way the turret points. A direction too short to normalise leaves the shot at
	 * the carrier's velocity.
	 */
	STARFORGECORE_API FVector ComputeMuzzleVelocity(
		const FVector& CarrierVelocity, const FVector& BarrelDirection, double MuzzleSpeed);

	/**
	 * Advances State by DeltaSeconds, clipped to what is left of MaxAgeSeconds.
	 *
	 * The clip is what makes range a design quantity: unclipped, range would be speed times
	 * the life plus up to one frame, longer at 60 fps than at 400. bExpired is set on the
	 * step that reaches the life, and that step still returns true so its segment is swept.
	 *
	 * Returns false, and leaves Out untouched, when DeltaSeconds or MaxAgeSeconds is not
	 * positive (NaN is not), when any part of State is not finite, or when State is already
	 * at or past its life.
	 */
	STARFORGECORE_API bool StepProjectile(
		const FProjectileState& State, double DeltaSeconds, double MaxAgeSeconds, FProjectileStep& Out);
}
