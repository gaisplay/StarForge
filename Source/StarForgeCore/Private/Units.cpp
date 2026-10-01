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
		// Through the scalar conversions, component by component, so the factors live in one
		// place. Bit for bit what the vector form computed: FVector's operator*(double) is one
		// multiply per component, and (c * 100) * 100 is NewtonMetresToUu's own order.
		FWrench Result;
		Result.Force = FVector(NewtonsToUu(Si.Force.X), NewtonsToUu(Si.Force.Y), NewtonsToUu(Si.Force.Z));
		Result.Torque = FVector(
			NewtonMetresToUu(Si.Torque.X), NewtonMetresToUu(Si.Torque.Y), NewtonMetresToUu(Si.Torque.Z));
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
