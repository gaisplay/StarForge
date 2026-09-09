#include "TurretMath.h"

namespace StarForge::Turret
{
	bool ComputeBearing(const FTransform& MountWorld, const FVector& TargetWorld, FTurretAim& OutBearing)
	{
		const FVector Local = MountWorld.InverseTransformPosition(TargetWorld);

		// A target on the mount has no direction. The threshold is tiny and unitless on
		// purpose: it catches the degenerate case and is not a minimum range.
		if (Local.IsNearlyZero(1e-6))
		{
			return false;
		}

		// atan2 gives (-pi, pi] with the sign the engine's yaw has: +X toward +Y.
		OutBearing.YawRad = FMath::Atan2(Local.Y, Local.X);
		OutBearing.PitchRad = FMath::Atan2(Local.Z, FMath::Sqrt(Local.X * Local.X + Local.Y * Local.Y));
		return true;
	}

	bool CanBear(const FTurretAim& Bearing, const FTurretLimits& Limits)
	{
		return FMath::Abs(Bearing.YawRad) <= Limits.MaxYawRad
			&& Bearing.PitchRad >= Limits.MinPitchRad
			&& Bearing.PitchRad <= Limits.MaxPitchRad;
	}

	FAimStep StepAim(
		const FTurretAim& Current,
		const FTurretAim& Bearing,
		const FTurretLimits& Limits,
		double DeltaSeconds)
	{
		// Where the barrel is going: the bearing, or the nearest point on the stops.
		FTurretAim Goal;
		Goal.YawRad = FMath::Clamp(Bearing.YawRad, -Limits.MaxYawRad, Limits.MaxYawRad);
		Goal.PitchRad = FMath::Clamp(Bearing.PitchRad, Limits.MinPitchRad, Limits.MaxPitchRad);

		const double MaxStep = FMath::Max(0.0, Limits.TraverseRateRadPerSec * DeltaSeconds);

		FAimStep Result;
		Result.Aim.YawRad = Current.YawRad + FMath::Clamp(Goal.YawRad - Current.YawRad, -MaxStep, MaxStep);
		Result.Aim.PitchRad = Current.PitchRad + FMath::Clamp(Goal.PitchRad - Current.PitchRad, -MaxStep, MaxStep);

		// Never outside the stops, even if Current arrived from outside them: a limits
		// edit while the barrel was deployed must pull it in, not leave it stranded.
		Result.Aim.YawRad = FMath::Clamp(Result.Aim.YawRad, -Limits.MaxYawRad, Limits.MaxYawRad);
		Result.Aim.PitchRad = FMath::Clamp(Result.Aim.PitchRad, Limits.MinPitchRad, Limits.MaxPitchRad);

		if (!CanBear(Bearing, Limits))
		{
			Result.State = EAimState::CannotBear;
		}
		else if (FMath::Abs(Result.Aim.YawRad - Bearing.YawRad) <= Limits.OnTargetToleranceRad
			&& FMath::Abs(Result.Aim.PitchRad - Bearing.PitchRad) <= Limits.OnTargetToleranceRad)
		{
			Result.State = EAimState::OnTarget;
		}
		else
		{
			Result.State = EAimState::Slewing;
		}

		return Result;
	}

	FRotator AimToRotator(const FTurretAim& Aim)
	{
		return FRotator(
			FMath::RadiansToDegrees(Aim.PitchRad),
			FMath::RadiansToDegrees(Aim.YawRad),
			0.0);
	}
}
