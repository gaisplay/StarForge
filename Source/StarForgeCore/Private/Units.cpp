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

	double ApplyDeadZone(double Raw, double DeadZone)
	{
		const double Zone = FMath::Clamp(DeadZone, 0.0, 1.0);
		const double Magnitude = FMath::Abs(Raw);

		// A zone of one covers the whole travel, so the stick is ignored, over-range input
		// included -- and the rescale below is never reached with nothing left to divide by.
		// A zone clamped short of one instead, to 0.99 say, stretches the last hundredth of the
		// travel over the whole range: a stick told to be ignored reads 0.2 at 0.992 and full
		// thrust at its stop.
		if (Magnitude <= Zone || Zone >= 1.0)
		{
			return 0.0;
		}

		// Rescale what is left of the travel back onto [0, 1], then put the sign back.
		const double Rescaled = (Magnitude - Zone) / (1.0 - Zone);
		return FMath::Sign(Raw) * FMath::Min(Rescaled, 1.0);
	}
}
