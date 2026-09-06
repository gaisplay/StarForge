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

	FVector ComputeFeltAcceleration(
		const FVector& ShipLinearAccel,
		const FVector& ShipAngularVelocityRadPerSec,
		const FVector& OccupantLocalOffset)
	{
		const FVector& Omega = ShipAngularVelocityRadPerSec;
		const FVector Centripetal =
			FVector::CrossProduct(Omega, FVector::CrossProduct(Omega, OccupantLocalOffset));

		return ShipLinearAccel + Centripetal;
	}

	bool ExceedsGravPlateLimit(const FVector& FeltAccelMetersPerSecSq)
	{
		return FeltAccelMetersPerSecSq.Size() > GravPlateLimitMetersPerSecSq;
	}
}
