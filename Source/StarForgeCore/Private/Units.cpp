#include "Units.h"

namespace StarForge::Units
{
	double MetresToCm(double Metres)
	{
		return Metres * CmPerMetre;
	}

	double CmToMetres(double Cm)
	{
		return Cm / CmPerMetre;
	}

	double NewtonsToUu(double Newtons)
	{
		return Newtons * CmPerMetre;
	}

	double NewtonMetresToUu(double NewtonMetres)
	{
		return NewtonMetres * CmPerMetre * CmPerMetre;
	}

	FVector InertiaUuToSi(const FVector& InertiaUu)
	{
		return InertiaUu / (CmPerMetre * CmPerMetre);
	}

	FWrench WrenchToUu(const FWrench& Si)
	{
		FWrench Result;
		Result.Force = Si.Force * CmPerMetre;
		Result.Torque = Si.Torque * CmPerMetre * CmPerMetre;
		return Result;
	}
}
