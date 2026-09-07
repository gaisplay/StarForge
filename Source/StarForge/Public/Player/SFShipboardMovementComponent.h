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

	/**
	 * Worst drift seen during the current unbroken settled interval, in centimetres.
	 * It is deliberately not a lifetime worst: the budget it is judged against is
	 * per minute, so the figure has to describe the same stretch of time as
	 * GetMeasuredSeconds and no other.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetPeakDriftCm() const { return PeakDriftCm; }

	/**
	 * Largest single-tick jump of the ship-local position during the current
	 * settled interval, in centimetres. Drift and jitter fail differently: a rig
	 * can hold its average position perfectly and still shake hard enough to be
	 * unusable.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetPeakJitterCm() const { return PeakJitterCm; }

	/**
	 * Length of the current unbroken stretch of settled, uncommanded time, in
	 * seconds. A drift of zero means nothing unless this is large: it separates
	 * "held position for a minute" from "never took a reading".
	 *
	 * One continuous interval, not a sum of several. Summing would let two
	 * thirty-second stands with a step between them pass a fifty-five-second
	 * validity gate that neither stand actually met, while each peak beside it
	 * was only ever a thirty-second figure.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetMeasuredSeconds() const { return MeasuredSeconds; }

	/**
	 * Duration of the longest settled interval that has completed since the frame was
	 * bound, in seconds. Unlike GetMeasuredSeconds, a restart never touches this: it
	 * survives the operator moving around after a good stand, so the result he came
	 * for does not vanish the instant he touches a key.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetBestCompletedSeconds() const { return BestCompletedSeconds; }

	/** Peak drift belonging to the interval reported by GetBestCompletedSeconds. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetBestCompletedPeakDriftCm() const { return BestCompletedPeakDriftCm; }

	/** Peak jitter belonging to the interval reported by GetBestCompletedSeconds. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetBestCompletedPeakJitterCm() const { return BestCompletedPeakJitterCm; }

	/**
	 * Engine ticks on which the carry could not run because the occupant had no
	 * movement base. Anything but zero means the instrument stopped instrumenting
	 * for a stretch, and is the single most important thing it can report.
	 *
	 * Counted internally as uint64 but reported as int64, because Blueprint has no
	 * unsigned 64-bit type and this figure has to reach the readout.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	int64 GetSkippedCarryFrames() const { return SkippedCarryFrames; }

	/**
	 * Engine frames since the carry last ran, or zero before the first one.
	 *
	 * The counter above cannot report a base loss that never ends, because it is
	 * incremented inside the very function the engine stops calling once the
	 * occupant comes off the deck. This is read from outside, every frame, so it can.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	int64 GetFramesSinceLastCarry() const;

	/** Current position in the ship's coordinates, in centimetres. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FVector GetLocalLocation() const;

	virtual void UpdateBasedMovement(float DeltaSeconds) override;

private:
	void CarryWithFrame();
	void UpdateDriftMeasurement(float DeltaSeconds);

	/**
	 * Drops the drift figures and the clock together, so the duration reported and
	 * the peaks reported always describe the same unbroken stretch of settled time.
	 */
	void RestartMeasurementInterval();

	/**
	 * Notices that the engine stopped calling the carry for a while and re-seeds
	 * from the ship's present pose. Returns nothing; the count it keeps is the
	 * report.
	 */
	void DetectCarryGap();

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

	/** Length of the current unbroken settled interval. */
	double MeasuredSeconds = 0.0;

	/**
	 * Next MeasuredSeconds threshold at which the periodic log line fires. Advances
	 * by ten seconds each time it fires and is reset alongside MeasuredSeconds, so
	 * the log gets one line per ten seconds of accumulated measured time rather than
	 * one per tick.
	 */
	double NextPeriodicLogSeconds = 10.0;

	/**
	 * Figures for the longest settled interval completed since the frame was last set.
	 * A restart of the live interval never touches these -- only SetShipFrame does --
	 * so a good result outlives the keypress that ends the interval that produced it.
	 */
	double BestCompletedSeconds = 0.0;
	double BestCompletedPeakDriftCm = 0.0;
	double BestCompletedPeakJitterCm = 0.0;

	/** Engine frame the carry last ran on, used to spot gaps in it. */
	uint64 LastCarryFrame = 0;

	/** Total carry ticks missed because the occupant was not attached to the ship. */
	int64 SkippedCarryFrames = 0;

	/** False until a baseline has been taken against a live capsule. */
	bool bBaselineValid = false;

	/** False until the carry has run once, so the very first tick is not read as a gap. */
	bool bHasCarriedOnce = false;
};
