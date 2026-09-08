#pragma once

#include "CoreMinimal.h"

/**
 * Reference-frame conversions for occupants riding a moving ship.
 *
 * These functions are unit-agnostic: they operate on whatever length unit the
 * caller passes in consistently. The game passes centimetres, matching Unreal.
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
}
