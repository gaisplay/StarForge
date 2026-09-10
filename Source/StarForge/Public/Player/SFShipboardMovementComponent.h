#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ShipFrameMath.h"
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

	/** The bound frame's owner, by name, or "none". What the readout prints. */
	FString GetFrameName() const;

	/** How far the first carry after the last handover moved him, in centimetres. */
	double GetLastHandoverJumpCm() const { return LastHandoverJumpCm; }

	/** By how much the deck's down turned across that handover, in degrees. */
	double GetLastHandoverGravityTurnDeg() const { return LastHandoverGravityTurnDeg; }

	/** The whole handover as one phrase, empty before the first one. */
	const FString& GetLastHandoverText() const { return LastHandoverText; }

	/**
	 * Consecutive frames on which the floor under him belonged to a different carrier
	 * than the frame he is bound to. Zero is the only healthy value.
	 *
	 * An instrument that costs one line, because the engine already knows what he is
	 * standing on: the movement base has an owner, and that owner either carries the
	 * frame he rides or it does not.
	 */
	int32 GetFrameMismatchFrames() const { return MismatchFrames; }

	virtual void UpdateBasedMovement(float DeltaSeconds) override;

	/**
	 * Whether the occupant is knocked off his feet (spec 6): the movement mode is
	 * falling, walking is refused, and the carry and the gravity alignment go on
	 * exactly as before. Standing up again takes half a second continuously under
	 * the grav-plate limit.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	bool IsKnockedDown() const { return Footing.bDown; }

	/** Whether sf.Knockdown is on. Off is a different rig, and every readout says so. */
	bool IsKnockdownEnabled() const;

	/**
	 * A seated occupant is strapped in and never loses his footing, whatever the ship
	 * does. Set by the character on taking and leaving a seat.
	 */
	void SetStrappedIn(bool bInStrappedIn) { bStrappedIn = bInStrappedIn; }
	bool IsStrappedIn() const { return bStrappedIn; }

	/** Seconds spent knocked down during the current settled interval. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetKnockedDownSeconds() const { return KnockedDownSeconds; }

	/**
	 * How long the knockdown in progress has lasted, in seconds; zero on his feet.
	 *
	 * This is the figure to print beside the word "down", and the one above is not:
	 * the interval's count is dropped with the peaks on every frame of the settle
	 * window, so a man lying on the deck reads as zero seconds down for the whole half
	 * second in which the instrument keeps restarting.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetCurrentKnockdownSeconds() const { return CurrentKnockdownSeconds; }

	/** Knocked-down seconds belonging to the interval reported by GetBestCompletedSeconds. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	double GetBestCompletedKnockedDownSeconds() const { return BestCompletedKnockedDownSeconds; }

	/** The felt acceleration the last footing decision was taken on, ship frame, m/s^2. */
	FVector GetFeltAccelerationMetersPerSecSq() const { return FeltAccelMetersPerSecSq; }

	/** What pushes him while he is down, ship frame, m/s^2. Zero on his feet. */
	FVector GetKnockdownPushMetersPerSecSq() const { return PushMetersPerSecSq; }

	/**
	 * What the drift figure means right now, in words, for the log and the HUD alike:
	 * the carry failed, or the man was knocked down, or neither. The number alone
	 * cannot say, and it used to mean only the first.
	 */
	FString DescribeFooting() const;

	/** False while knocked down: a man who is down does not land. See the cpp. */
	virtual bool IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const override;

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** No landing while knocked down, whichever path asks. See the cpp. */
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

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

	/**
	 * Compares the owner of the floor he is standing on against the frame he is bound to,
	 * and complains when the two disagree for ten frames running.
	 *
	 * Latched to one call per frame like the drift measurement, and for a related reason:
	 * the engine calls UpdateBasedMovement once a frame for a base that is not simulating
	 * and twice for one that is (CharacterMovementComponent.cpp 2476-2510). Unlatched,
	 * "ten frames" would mean ten frames docked and five flying -- an instrument whose
	 * threshold moves with the state of the ship.
	 */
	void UpdateFrameCrossCheck(float DeltaSeconds);

	/**
	 * Diagnostic only, behind sf.DriftDebug. Reports how many times the carry runs
	 * per engine frame, what the occupant is standing on, and where he is in the
	 * ship's coordinates.
	 *
	 * It exists because two questions cannot be answered from the drift summary
	 * alone: the measured-time clock advances once per call, so a call that runs
	 * twice a frame makes it run at twice real time and every duration on the
	 * results form is half what it claims; and a negative control that used to fall
	 * silent now goes on measuring, which means something is still calling this
	 * after the occupant has left the deck.
	 */
	void LogDiagnostics(float DeltaSeconds);

	/**
	 * Decides whether he keeps his feet this frame and acts on it: enters falling with
	 * the base kept, pushes him with what the plates could not absorb, stands him up
	 * again. Once per frame, like UpdateDriftMeasurement, and for the same reason.
	 *
	 * Called from the carry while there is a movement base and from TickComponent when
	 * there is not. The engine calls UpdateBasedMovement only while a base is set
	 * (CharacterMovementComponent.cpp 2476-2487), and a man who lost his base while
	 * down would otherwise stay down for the rest of the session: no walking, and no
	 * landing either, since IsValidLandingSpot refuses one while he is down.
	 */
	void UpdateFooting(float DeltaSeconds);

	/**
	 * Reports, from outside the carry, a carry gap that is still open. The counter in
	 * DetectCarryGap cannot: it lives inside the function the engine has stopped
	 * calling. Once a second, for as long as the gap lasts.
	 */
	void LogOpenCarryGap(float DeltaSeconds);

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

	/**
	 * Engine frame the drift measurement last ran on.
	 *
	 * The engine calls UpdateBasedMovement twice per frame, so without this the
	 * clock accumulates two frame deltas per frame and runs at exactly twice real
	 * time -- which made every duration this instrument has ever reported, including
	 * the one its own validity gate is read against, double what actually elapsed.
	 */
	uint64 LastMeasuredFrame = TNumericLimits<uint64>::Max();

	/** Diagnostic state for LogDiagnostics; meaningless unless sf.DriftDebug is on. */
	uint64 DebugLastFrame = 0;
	int32 DebugCallsThisFrame = 0;
	int32 DebugCallsPerFramePeak = 0;
	double DebugSecondsSinceLog = 0.0;
	FString DebugLastState;

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

	StarForge::ShipFrame::FFootingState Footing;

	/** Ship-frame figures behind the last footing decision, for the readouts. */
	FVector FeltAccelMetersPerSecSq = FVector::ZeroVector;
	FVector PushMetersPerSecSq = FVector::ZeroVector;

	/** Set while he holds a seat. Strapped in, he is never knocked down. */
	bool bStrappedIn = false;

	/** Knocked-down time in the current settled interval; reset with the peaks. */
	double KnockedDownSeconds = 0.0;

	/** Length of the knockdown in progress, for the line that says how long he was down. */
	double CurrentKnockdownSeconds = 0.0;

	double BestCompletedKnockedDownSeconds = 0.0;

	/** Same guard as LastMeasuredFrame, for the same reason: two calls a frame would push twice. */
	uint64 LastFootingFrame = TNumericLimits<uint64>::Max();

	/** Throttle for LogOpenCarryGap. */
	double SecondsSinceGapLog = 0.0;

	/** Set by SetShipFrame, consumed by the first carry that follows it. */
	bool bHandoverPending = false;
	FString PendingHandoverOldName;
	FString PendingHandoverNewName;
	double PendingHandoverGravityTurnDeg = 0.0;

	double LastHandoverJumpCm = 0.0;
	double LastHandoverGravityTurnDeg = 0.0;
	FString LastHandoverText;

	/** Cross-check state; see UpdateFrameCrossCheck. */
	int32 MismatchFrames = 0;
	uint64 LastCrossCheckFrame = TNumericLimits<uint64>::Max();
	double SecondsSinceMismatchLog = 0.0;
};
