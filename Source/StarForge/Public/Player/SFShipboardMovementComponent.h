#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SFShipboardMovementComponent.generated.h"

class USFShipFrameComponent;

/**
 * Character movement that survives standing inside a fast, hard-turning ship.
 *
 * A character's motion in one frame is two unrelated things. Being carried by
 * the ship is zero motion relative to the ship, so nothing along that path can
 * legitimately block it -- stock UE sweeps it anyway and collides with the
 * ship's own hull, which at 400 m/s is a six-metre line through a wall. That
 * part is done as a teleport here. Walking is a few centimetres against
 * momentarily static geometry, and is left to the stock swept path untouched.
 */
UCLASS()
class STARFORGE_API USFShipboardMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	USFShipboardMovementComponent();

	/** Starts riding a frame, or stops when passed null. */
	UFUNCTION(BlueprintCallable, Category = "StarForge|Frame")
	void SetShipFrame(USFShipFrameComponent* NewFrame);

	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	USFShipFrameComponent* GetShipFrame() const { return ShipFrame.Get(); }

	/** Ship-local displacement accumulated while standing still, in centimetres. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetDriftCm() const { return DriftCm; }

	/** Worst drift seen since the frame was set, in centimetres. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetPeakDriftCm() const { return PeakDriftCm; }

	/**
	 * Largest single-tick jump of the ship-local position while standing still,
	 * in centimetres. Drift and jitter fail differently: a rig can hold its
	 * average position perfectly and still shake hard enough to be unusable.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetPeakJitterCm() const { return PeakJitterCm; }

	/**
	 * Seconds of settled, uncommanded time actually folded into the drift figures.
	 * A drift of zero means nothing unless this is large: it separates "held
	 * position for a minute" from "never took a reading".
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetMeasuredSeconds() const { return MeasuredSeconds; }

	/** Current position in the ship's coordinates, in centimetres. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FVector GetLocalLocation() const;

	virtual void UpdateBasedMovement(float DeltaSeconds) override;

private:
	void CarryWithFrame();
	void UpdateDriftMeasurement(float DeltaSeconds);

	TWeakObjectPtr<USFShipFrameComponent> ShipFrame;

	/** This occupant's own view of where the ship was last tick. */
	FTransform LastFrameTransform = FTransform::Identity;

	/** Baseline the drift is measured against, re-taken whenever the player moves. */
	FVector DriftReferenceLocal = FVector::ZeroVector;

	/** Previous tick's ship-local position, for the per-tick jitter figure. */
	FVector LastLocalLocation = FVector::ZeroVector;

	double DriftCm = 0.0;
	double PeakDriftCm = 0.0;
	double PeakJitterCm = 0.0;

	/** Seconds since the player last commanded movement. */
	double SecondsSinceCommandedInput = 0.0;

	/** Settled time folded into the drift figures so far. */
	double MeasuredSeconds = 0.0;

	/** False until a baseline has been taken against a live capsule. */
	bool bBaselineValid = false;
};
