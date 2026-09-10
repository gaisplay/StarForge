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

	FVector ComputeUncompensatedAcceleration(const FVector& FeltAccelMetersPerSecSq)
	{
		const FVector Absorbed = FeltAccelMetersPerSecSq.GetClampedToMaxSize(GravPlateLimitMetersPerSecSq);
		return -(FeltAccelMetersPerSecSq - Absorbed);
	}

	bool FFootingState::Update(bool bOverLimit, double DeltaSeconds, double RecoverySeconds)
	{
		if (bOverLimit)
		{
			bDown = true;
			SecondsUnderLimit = 0.0;
			return bDown;
		}

		if (bDown)
		{
			SecondsUnderLimit += FMath::Max(0.0, DeltaSeconds);
			if (SecondsUnderLimit >= RecoverySeconds)
			{
				bDown = false;
				SecondsUnderLimit = 0.0;
			}
		}

		return bDown;
	}
}

namespace StarForge::ShipFrame
{
	FQuat ApplyLookStep(
		const FQuat& CurrentView,
		const FVector& DeckUp,
		double YawDegrees,
		double PitchDegrees,
		double MaxPitchDegrees)
	{
		const FVector Up = DeckUp.GetSafeNormal();
		if (Up.IsNearlyZero())
		{
			// No deck to stand on means no axes to turn about; leaving the view alone
			// is the only honest answer.
			return CurrentView;
		}

		// Yaw about the deck normal, in world space, so turning on the spot follows the
		// floor rather than the world's vertical.
		FQuat View = FQuat(Up, FMath::DegreesToRadians(YawDegrees)) * CurrentView;

		// Pitch about the view's own right axis, recomputed after the yaw so the two
		// halves of one step do not fight each other. It is projected back into the
		// deck plane first: that is what stops a pitch step from leaking roll, and it
		// re-squares the axis every frame instead of letting error accumulate.
		FVector Right = FVector::VectorPlaneProject(View.GetRightVector(), Up).GetSafeNormal();
		if (Right.IsNearlyZero())
		{
			Right = View.GetRightVector();
		}

		// How far above the deck plane the view already points, and how far it may go.
		// Clamping the destination rather than the step means holding the key parks the
		// view at the zenith instead of walking it over the top.
		const double CurrentPitch = FMath::RadiansToDegrees(
			FMath::Asin(FMath::Clamp(FVector::DotProduct(View.GetForwardVector(), Up), -1.0, 1.0)));

		const double AllowedPitch =
			FMath::Clamp(CurrentPitch + PitchDegrees, -MaxPitchDegrees, MaxPitchDegrees) - CurrentPitch;

		// Negative because a positive rotation about the right axis pitches the nose
		// down, while a positive PitchDegrees means looking up.
		View = FQuat(Right, -FMath::DegreesToRadians(AllowedPitch)) * View;

		return View.GetNormalized();
	}
}
