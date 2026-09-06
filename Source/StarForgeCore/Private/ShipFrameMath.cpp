#include "ShipFrameMath.h"

namespace StarForge::ShipFrame
{
	FTransform WorldToLocal(const FTransform& ShipWorld, const FTransform& OccupantWorld)
	{
		return OccupantWorld.GetRelativeTransform(ShipWorld);
	}

	FTransform LocalToWorld(const FTransform& ShipWorld, const FTransform& OccupantLocal)
	{
		return OccupantLocal * ShipWorld;
	}

	FTransform CarryOccupant(
		const FTransform& PrevShipWorld,
		const FTransform& CurrShipWorld,
		const FTransform& OccupantWorld)
	{
		return LocalToWorld(CurrShipWorld, WorldToLocal(PrevShipWorld, OccupantWorld));
	}
}
