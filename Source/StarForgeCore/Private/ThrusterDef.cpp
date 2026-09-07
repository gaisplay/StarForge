#include "ThrusterDef.h"

namespace StarForge
{
	FWrench ComputeThrusterWrench(const FThrusterDef& Thruster, const FVector& CentreOfMass)
	{
		FWrench Result;
		Result.Force = Thruster.Direction.GetSafeNormal() * Thruster.MaxThrust;
		Result.Torque = FVector::CrossProduct(Thruster.Location - CentreOfMass, Result.Force);
		return Result;
	}
}
