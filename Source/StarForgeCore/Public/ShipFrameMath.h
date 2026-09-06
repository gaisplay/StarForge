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
}
