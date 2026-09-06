#include "Player/SFShipboardMovementComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Ship/SFShipFrameComponent.h"

namespace
{
	/**
	 * How long after the player stops commanding movement before drift measurement
	 * resumes, in seconds. Long enough for his own momentum to brake out, short
	 * enough that a one-minute standing test is nearly all measurement.
	 */
	constexpr double SettleSeconds = 0.5;
}

USFShipboardMovementComponent::USFShipboardMovementComponent()
{
	// The ship rotates the occupant explicitly in CarryWithFrame; letting the
	// stock yaw-only path run as well would apply the rotation twice.
	bIgnoreBaseRotation = true;
}

void USFShipboardMovementComponent::SetShipFrame(USFShipFrameComponent* NewFrame)
{
	ShipFrame = NewFrame;

	if (NewFrame)
	{
		LastFrameTransform = NewFrame->GetFrameTransform();
	}

	DriftCm = 0.0;
	PeakDriftCm = 0.0;
	PeakJitterCm = 0.0;
	MeasuredSeconds = 0.0;
	SecondsSinceCommandedInput = 0.0;
	bBaselineValid = false;
}

FVector USFShipboardMovementComponent::GetLocalLocation() const
{
	if (!ShipFrame.IsValid() || !UpdatedComponent)
	{
		return FVector::ZeroVector;
	}

	return ShipFrame->WorldToLocal(UpdatedComponent->GetComponentTransform()).GetLocation();
}

void USFShipboardMovementComponent::UpdateBasedMovement(float DeltaSeconds)
{
	if (!ShipFrame.IsValid() || !UpdatedComponent)
	{
		Super::UpdateBasedMovement(DeltaSeconds);
		return;
	}

	CarryWithFrame();
	UpdateDriftMeasurement(DeltaSeconds);
}

void USFShipboardMovementComponent::CarryWithFrame()
{
	const FTransform CurrentFrameTransform = ShipFrame->GetFrameTransform();
	const FTransform OccupantWorld = UpdatedComponent->GetComponentTransform();

	const FTransform Carried =
		ShipFrame->CarryOccupant(LastFrameTransform, OccupantWorld);

	const FVector Delta = Carried.GetLocation() - OccupantWorld.GetLocation();

	// bSweep is false deliberately. Relative to the ship this displacement is
	// zero, so there is nothing along the path that could legitimately stop it.
	MoveUpdatedComponent(
		Delta,
		Carried.GetRotation(),
		/*bSweep=*/false,
		/*OutHit=*/nullptr,
		ETeleportType::TeleportPhysics);

	// The view is stored ship-local: the player keeps looking at the same
	// bulkhead no matter what the ship does, and the stars sweep past outside.
	if (ACharacter* Owner = Cast<ACharacter>(GetOwner()))
	{
		if (AController* Controller = Owner->GetController())
		{
			const FQuat FrameDelta =
				CurrentFrameTransform.GetRotation() * LastFrameTransform.GetRotation().Inverse();

			const FQuat NewView = FrameDelta * Controller->GetControlRotation().Quaternion();
			Controller->SetControlRotation(NewView.Rotator());
		}
	}

	LastFrameTransform = CurrentFrameTransform;

	// Keep the engine's own base bookkeeping consistent even though the stock
	// carry path was bypassed.
	SaveBaseLocation();
}

void USFShipboardMovementComponent::UpdateDriftMeasurement(float DeltaSeconds)
{
	// Only commanded input counts as intent. Residual velocity must never excuse a
	// reading: uncommanded motion is precisely what this instrument exists to catch,
	// and gating on it let a gross failure suppress its own measurement.
	if (GetCurrentAcceleration().IsNearlyZero())
	{
		SecondsSinceCommandedInput += DeltaSeconds;
	}
	else
	{
		SecondsSinceCommandedInput = 0.0;
	}

	const FVector NowLocal = GetLocalLocation();

	// The ship's origin is a legal place to stand, so GetLocalLocation cannot signal
	// failure by returning zero. Refuse to baseline until there is a capsule to
	// measure, or the first reading would bank the occupant's whole offset from
	// origin into two peaks that never come back down.
	if (!bBaselineValid)
	{
		if (UpdatedComponent)
		{
			DriftReferenceLocal = NowLocal;
			LastLocalLocation = NowLocal;
			bBaselineValid = true;
		}
		return;
	}

	if (SecondsSinceCommandedInput < SettleSeconds)
	{
		// His own momentum is still bleeding off. Re-baseline rather than bank
		// walking as drift, but do not count this tick as measured either.
		DriftReferenceLocal = NowLocal;
		LastLocalLocation = NowLocal;
		DriftCm = 0.0;
		return;
	}

	DriftCm = FVector::Dist(NowLocal, DriftReferenceLocal);
	PeakDriftCm = FMath::Max(PeakDriftCm, DriftCm);

	// Jitter is what the eye notices even when the average position is perfect.
	const double JitterCm = FVector::Dist(NowLocal, LastLocalLocation);
	PeakJitterCm = FMath::Max(PeakJitterCm, JitterCm);

	LastLocalLocation = NowLocal;
	MeasuredSeconds += DeltaSeconds;
}
