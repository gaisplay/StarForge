#include "ViewMath.h"

namespace StarForge::View
{
	double VerticalFov(double HorizontalFov, double AspectRatio)
	{
		return 2.0 * FMath::Atan(FMath::Tan(0.5 * HorizontalFov) / AspectRatio);
	}

	double FitSphereDistance(double Radius, double HorizontalFov, double AspectRatio)
	{
		// Each guard is the negation of "valid", so a NaN fails it instead of slipping through.
		if (!(Radius > 0.0) || !FMath::IsFinite(Radius)) { return 0.0; }
		if (!(HorizontalFov > 0.0 && HorizontalFov < UE_DOUBLE_PI)) { return 0.0; }
		if (!(AspectRatio > 0.0)) { return 0.0; }

		const double NarrowerFov = FMath::Min(HorizontalFov, VerticalFov(HorizontalFov, AspectRatio));

		// An infinite aspect passes the guard above and leaves a vertical field of exactly zero,
		// which is a field outside (0, pi) arrived at by the other road.
		if (!(NarrowerFov > 0.0)) { return 0.0; }

		return Radius / FMath::Sin(0.5 * NarrowerFov);
	}

	FTransform ChaseCameraPose(
		const FVector& Centre,
		double Radius,
		double HorizontalFov,
		double AspectRatio,
		double Elevation)
	{
		const double Distance = FitSphereDistance(Radius, HorizontalFov, AspectRatio);
		if (Distance <= 0.0)
		{
			return FTransform::Identity;
		}

		const FVector Location =
			Centre + Distance * FVector(-FMath::Cos(Elevation), 0.0, FMath::Sin(Elevation));

		// A rotation about +Y by +e is FRotator(-e, 0, 0): it takes +X onto (cos e, 0, -sin e),
		// which is the nose pitched down by e -- the same sign ShipFrame::ApplyLookStep states for
		// the right axis. Built from the axis and the angle in radians rather than from a rotator
		// in degrees, so the pose carries no degree round trip into the 1e-9 the test holds it to.
		const FQuat Rotation(FVector::YAxisVector, Elevation);

		return FTransform(Rotation, Location);
	}
}
