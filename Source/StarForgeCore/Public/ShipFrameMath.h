#pragma once

#include "CoreMinimal.h"

/**
 * Reference-frame conversions for occupants riding a moving ship, and what the ship's
 * motion does to a man standing in it.
 *
 * The transform helpers are unit-agnostic: they operate on whatever length unit the
 * caller passes in consistently. The game passes centimetres, matching Unreal. The ones
 * carrying a time -- accelerations, velocities, the grav-plate limit -- are SI like the
 * rest of StarForgeCore, and every one of those says so in its name.
 */
namespace StarForge::ShipFrame
{
	/** Occupant's transform expressed in the ship's frame. */
	STARFORGECORE_API FTransform WorldToLocal(const FTransform& ShipWorld, const FTransform& OccupantWorld);

	/** Occupant's ship-local transform expressed in world space. */
	STARFORGECORE_API FTransform LocalToWorld(const FTransform& ShipWorld, const FTransform& OccupantLocal);

	/**
	 * World transform an occupant must take so that its ship-local transform is
	 * unchanged after the ship moves from PrevShipWorld to CurrShipWorld.
	 */
	STARFORGECORE_API FTransform CarryOccupant(
		const FTransform& PrevShipWorld,
		const FTransform& CurrShipWorld,
		const FTransform& OccupantWorld);

	/**
	 * Ceiling of the ship's gravity-plate compensation, in m/s^2.
	 * Below it an occupant walks normally; above it he is knocked off his feet.
	 * Fixed at 2 g by the vertical slice design.
	 */
	inline constexpr double GravPlateLimitMetersPerSecSq = 19.62;

	/**
	 * Acceleration felt by an occupant standing at OccupantLocalOffset from the
	 * ship's pivot. Sums the ship's own linear acceleration with the centripetal
	 * term produced by its rotation.
	 *
	 * All three arguments and the result use metres and seconds.
	 */
	STARFORGECORE_API FVector ComputeFeltAcceleration(
		const FVector& ShipLinearAccel,
		const FVector& ShipAngularVelocityRadPerSec,
		const FVector& OccupantLocalOffset);

	/** True when the felt acceleration is beyond what the gravity plates absorb. */
	STARFORGECORE_API bool ExceedsGravPlateLimit(const FVector& FeltAccelMetersPerSecSq);

	/**
	 * What the occupant is left to feel once the plates have absorbed what they can.
	 *
	 * Zero up to and including the limit. Above it, the excess -- reversed, because
	 * FeltAccel is the acceleration of the deck under him and what he experiences is
	 * its opposite: a deck accelerating toward the spin axis is a man being thrown
	 * toward the wall. m/s^2 in and out, in the ship's frame.
	 */
	STARFORGECORE_API FVector ComputeUncompensatedAcceleration(const FVector& FeltAccelMetersPerSecSq);

	/**
	 * Whether an occupant is on his feet, with the delay that keeps a manoeuvre sitting
	 * at the limit from standing him up and knocking him down on alternate frames.
	 *
	 * Footing is lost the instant the limit is exceeded and regained only after
	 * RecoverySeconds continuously under it; any relapse restarts that wait.
	 */
	struct STARFORGECORE_API FFootingState
	{
		bool bDown = false;
		double SecondsUnderLimit = 0.0;

		/** Advances one step and returns whether he is down after it. */
		bool Update(bool bOverLimit, double DeltaSeconds, double RecoverySeconds);
	};

	/**
	 * One step of first-person look input, taken in the deck's axes rather than the
	 * world's.
	 *
	 * Yaw turns about the deck normal and pitch about the view's own right axis. That
	 * distinction is the whole point: an occupant standing on a deck turns on the spot
	 * and nods, and neither of those is a rotation about the world's vertical once the
	 * ship is rolled. Feeding the engine's AddYawInput and AddPitchInput instead adds
	 * to the world Euler components of the control rotation, so on a ship rolling at
	 * 180 deg/s the axis that "look up" moves along precesses continuously and the view
	 * sweeps a circle.
	 *
	 * PitchDegrees is positive looking up, toward DeckUp, and is limited to
	 * MaxPitchDegrees away from the deck plane so the view stops at the zenith instead
	 * of tumbling over it. That limit is measured against the deck and not against the
	 * world horizon, which is why it can coexist with the engine's own pitch clamp
	 * being deliberately lifted.
	 *
	 * Angles are degrees; DeckUp need not be normalised.
	 */
	STARFORGECORE_API FQuat ApplyLookStep(
		const FQuat& CurrentView,
		const FVector& DeckUp,
		double YawDegrees,
		double PitchDegrees,
		double MaxPitchDegrees = 89.0);

	/**
	 * A carrier's motion, as much of it as one point on that carrier needs: where the frame's
	 * origin is, how fast the origin travels, and how fast the frame turns about it.
	 *
	 * Metres, seconds and radians, and the field names say so. Unlike the transform helpers
	 * above this cannot be unit-agnostic: an angular velocity in radians per second crossed
	 * with an offset gives a speed, so fixing the time unit fixes the length unit with it. The
	 * game holds centimetres and converts at the call, through StarForge::Units.
	 *
	 * The frame's ORIENTATION is deliberately absent. The angular velocity and the offset are
	 * both world-axis vectors, so the rotation never enters the arithmetic, and a function
	 * taking the whole transform would be taking a quantity it must not use.
	 */
	struct STARFORGECORE_API FCarrierMotion
	{
		FVector OriginMetres = FVector::ZeroVector;
		FVector LinearVelocityMetresPerSec = FVector::ZeroVector;
		FVector AngularVelocityRadPerSec = FVector::ZeroVector;
	};

	/**
	 * How fast two carriers move relative to each other AT ONE POINT: the length of
	 * (vA + wA x rA) - (vB + wB x rB), where each r is the point's offset from that carrier's
	 * own origin.
	 *
	 * At a point and not between the origins, because a carrier that spins moves its deck at
	 * w x r. Two hulls whose origins are at rest with respect to each other can still slide
	 * past each other where a man stands, and it is the floor under him that is about to
	 * change. This is the arithmetic behind the interior volume's handover guard -- what keeps
	 * a man in his own frame when two hulls touch -- and the limit it is weighed against stays
	 * with the caller, which is where the decision belongs.
	 *
	 * Returns false and leaves Out untouched when any argument is not finite, the contract
	 * StepProjectile and the damage functions keep, so a velocity gone NaN refuses instead of
	 * reading as a speed.
	 */
	STARFORGECORE_API bool RelativeSpeedAtPointMetresPerSec(
		const FCarrierMotion& A,
		const FCarrierMotion& B,
		const FVector& PointMetres,
		double& OutSpeedMetresPerSec);
}
