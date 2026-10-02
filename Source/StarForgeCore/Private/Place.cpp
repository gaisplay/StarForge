#include "Place.h"

namespace StarForge::Place
{
	namespace
	{
		bool IsFiniteVector(const FVector& V)
		{
			// Written through IsFinite rather than ContainsNaN because the name ContainsNaN
			// promises less than is needed here: an infinite coordinate is just as unusable as
			// a NaN one, and every comparison with either is false, so a guard that misses one
			// hands back a confident EngineReach over data that is simply broken.
			return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
		}
	}

	FPlaceFit MeasurePlace(
		const FVector& OriginFromWorldOriginMetres, double RadiusMetres, const FPlaceLimits& Limits)
	{
		FPlaceFit Fit;

		// The limits first, and nothing else filled in yet: if they are unusable, every figure
		// derived from them would be a number the caller could print and nobody could trust.
		const bool bLimitsUsable =
			FMath::IsFinite(Limits.EngineRadiusMetres) && Limits.EngineRadiusMetres > 0.0
			&& FMath::IsFinite(Limits.SlowestHullSpeedMetresPerSec) && Limits.SlowestHullSpeedMetresPerSec > 0.0
			&& FMath::IsFinite(Limits.CrossingBudgetSeconds) && Limits.CrossingBudgetSeconds > 0.0;

		if (!bLimitsUsable)
		{
			Fit.Refusal = EPlaceRefusal::LimitsNotUsable;
			return Fit;
		}

		Fit.EngineCeilingMetres = Limits.EngineRadiusMetres;
		Fit.CrossingCeilingMetres = Limits.SlowestHullSpeedMetresPerSec * Limits.CrossingBudgetSeconds;

		if (!IsFiniteVector(OriginFromWorldOriginMetres) || !FMath::IsFinite(RadiusMetres))
		{
			Fit.Refusal = EPlaceRefusal::NotFinite;
			return Fit;
		}

		if (RadiusMetres <= 0.0)
		{
			// Spec 5 calls a place a continuous volume. A radius of zero clears both ceilings
			// with the largest margins in the file and describes nothing anyone can fly in,
			// which makes it the one authoring mistake this predicate would otherwise bless.
			Fit.Refusal = EPlaceRefusal::RadiusNotPositive;
			return Fit;
		}

		// Everything measured before anything is judged, because the line is printed either way.
		Fit.ReachMetres = OriginFromWorldOriginMetres.Size() + RadiusMetres;
		Fit.EngineMarginMetres = Fit.EngineCeilingMetres - Fit.ReachMetres;
		Fit.DiameterMetres = 2.0 * RadiusMetres;
		Fit.CrossingMarginMetres = Fit.CrossingCeilingMetres - Fit.DiameterMetres;
		Fit.CrossingSeconds = Fit.DiameterMetres / Limits.SlowestHullSpeedMetresPerSec;

		if (Fit.EngineMarginMetres < 0.0)
		{
			Fit.Refusal = EPlaceRefusal::EngineReach;
			return Fit;
		}

		if (Fit.CrossingMarginMetres < 0.0)
		{
			Fit.Refusal = EPlaceRefusal::CrossingTime;
			return Fit;
		}

		Fit.Refusal = EPlaceRefusal::None;
		return Fit;
	}

	const TCHAR* RefusalName(EPlaceRefusal Refusal)
	{
		switch (Refusal)
		{
		case EPlaceRefusal::None:              return TEXT("fits");
		case EPlaceRefusal::LimitsNotUsable:   return TEXT("limits not usable");
		case EPlaceRefusal::NotFinite:         return TEXT("not a finite place");
		case EPlaceRefusal::RadiusNotPositive: return TEXT("radius not positive");
		case EPlaceRefusal::EngineReach:       return TEXT("engine reach");
		case EPlaceRefusal::CrossingTime:      return TEXT("crossing time");
		}

		return TEXT("unknown refusal");
	}
}
