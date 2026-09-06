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
}
