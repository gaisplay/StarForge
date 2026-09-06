#include "Player/SFShipboardMovementComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Ship/SFShipFrameComponent.h"

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
		DriftReferenceLocal = GetLocalLocation();
		LastLocalLocation = DriftReferenceLocal;
	}

	DriftCm = 0.0;
	PeakDriftCm = 0.0;
	PeakJitterCm = 0.0;
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

	UpdateDriftMeasurement();
}

void USFShipboardMovementComponent::UpdateDriftMeasurement()
{
	const FVector NowLocal = GetLocalLocation();

	const bool bMovingOnPurpose =
		!GetCurrentAcceleration().IsNearlyZero() || !Velocity.IsNearlyZero(1.0);

	if (bMovingOnPurpose)
	{
		// Walking legitimately changes the ship-local position, so re-baseline
		// instead of counting it as drift.
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
}
