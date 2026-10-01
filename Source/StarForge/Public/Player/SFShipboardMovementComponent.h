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
	 * Ticks of this component on which the carry could not run because the occupant had
	 * no movement base, since the frame was last bound. Anything but zero means the
	 * instrument stopped instrumenting for a stretch, and is the single most important
	 * thing it can report.
	 *
	 * This component's ticks, not engine frames. GFrameCounter goes on counting while the
	 * world is paused (LaunchEngineLoop.cpp 6131) and through a HighResShot's run-up
	 * (UnrealClient.cpp 1604), and nothing ticks in either: counted in engine frames, ten
	 * seconds of `pause` at 60 fps banked 600 skipped carries and turned every later verdict
	 * to CARRY FAILED for a carry that never failed. A component that ticks without being
	 * carried is still counted, so a base genuinely lost is counted exactly as before.
	 *
	 * Counted internally as uint64 but reported as int64, because Blueprint has no
	 * unsigned 64-bit type and this figure has to reach the readout.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	int64 GetSkippedCarryFrames() const { return SkippedCarryFrames; }

	/**
	 * The same count over the whole session. No binding resets it: a handover used to erase
	 * the only record of a carry failure, because the count above belongs to one binding and
	 * walking through the hatch starts a new one. The readout's red line reads this.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	int64 GetTotalSkippedCarryFrames() const { return TotalSkippedCarryFrames; }

	/**
	 * This component's ticks since the carry last ran, or zero before the first one.
	 *
	 * The counter above cannot report a base loss that never ends, because it is
	 * incremented inside the very function the engine stops calling once the
	 * occupant comes off the deck. This is read from outside, every frame, so it can.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	int64 GetFramesSinceLastCarry() const;

	/**
	 * Seconds since the last binding with no carry at all, zero once he has been carried.
	 *
	 * The count above is blind here by construction: it measures from the last carry, and
	 * after a binding there has not been one. A man bound in mid-air who never reaches a
	 * deck is the case it cannot see -- recorded on the spike rig under full thrust along
	 * -Z, where the floor outruns his spawn fall: never carried in eighty seconds at 400 fps,
	 * and at 60 not before a brief contact 6.0-6.3 s in, whose end the record had read as
	 * the moment he came off. Every instrument stayed calm until then.
	 */
	double GetSecondsBoundUncarried() const { return bHasCarriedOnce ? 0.0 : SecondsBoundUncarried; }

	/** Whether the carry has run at all since the frame was last bound. */
	bool HasBeenCarriedSinceBinding() const { return bHasCarriedOnce; }

	/**
	 * How long a binding may go without a first carry before it is a finding.
	 *
	 * Every rig places the man with the foot of his capsule 2 cm above the deck -- its centre
	 * 100 cm above the floor plate's mid-plane, a half-height of 88, and the plate's top face
	 * 10 cm above its mid-plane -- and the fall takes sqrt(2 x 0.02 / 9.81) = 64 ms. Measured
	 * as the first carry after the binding: 5 frames at 60 fps (83 ms) and 27 at 400
	 * (67.5 ms).
	 *
	 * The bound is not the fall alone. He lands during one world step and, on a simulating
	 * hull, the first carry is the post-physics one of the step after it, which runs after
	 * this clock is read; and a world step is clamped at 0.4 s (MaxUndilatedFrameTime,
	 * BaseGame.ini 199), the length an uncapped run's first frames after the map load come
	 * close to. So a man who lands has been carried within 0.064 + 2 x 0.4 = 0.864 s at the
	 * very worst, at any frame rate. A second clears that by 136 ms. In a second under one g
	 * a man falls 4.9 m, more than half the cabin: one still uncarried then is not finishing
	 * a 2 cm fall.
	 */
	static constexpr double NeverCarriedThresholdSeconds = 1.0;

	/**
	 * Whether the carry is reported as not running: two ticks since the last carry, or a
	 * binding older than NeverCarriedThresholdSeconds with no carry in it. The readout's
	 * red line and the log's Warning are both decided here, so the two cannot disagree.
	 */
	bool IsCarryNotRunning() const;

	/**
	 * Bound to a frame and standing on nothing. The knockdown cannot take a man's footing
	 * when he has none, so without this the verdict fell through to "footing intact" for a
	 * man drifting in the cabin with no carry at all.
	 */
	bool HasNoFloor() const;

	/** The carrier he was bound to when it was destroyed under him; empty otherwise. */
	const FString& GetLostFrameName() const { return LostFrameName; }

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
	double GetFrameMismatchSeconds() const { return MismatchSeconds; }

	/**
	 * True once the disagreement has lasted long enough to be reported. The readout
	 * colours its frame line on this and nothing else, so the panel and the log agree on
	 * what counts as a mismatch: the same threshold, in the same unit.
	 */
	bool IsFrameMismatchReported() const { return bMismatchReported; }

	/**
	 * How long the floor and the frame may disagree before it is a finding: a sixth of a
	 * second, which is the ten frames at 60 fps the instrument was designed around.
	 *
	 * A TIME, not a frame count, and the difference was measured. Walking through the
	 * hatch binds the occupant to the new carrier when the edge of his capsule touches its
	 * volume, while the floor under him switches when the capsule's centre crosses the
	 * panel boundary: 34 cm on the way out and 24 cm on the way back, 57 and 40 ms at
	 * walking speed. At 60 fps that is three frames and two, under any threshold; at the
	 * 320 fps of the first manual run it is eighteen and thirteen, and a threshold of ten
	 * frames warned on every single crossing -- twelve false findings in one session. A
	 * frame count is a frame rate, which is the shape of the Chaos sleep defect again.
	 * A sixth of a second clears the seam at any rate and still reports a wrong binding
	 * within it -- and a man who stops on the seam itself is reported, as he should be:
	 * if the ship leaves then, the seam leaves without him.
	 */
	static constexpr double FrameMismatchThresholdSeconds = 1.0 / 6.0;

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
	 * Why the last footing decision could not knock him down, whatever the felt figure:
	 * "strapped in", "sf.Knockdown 0", "no floor under him", or several of them joined.
	 * Empty when nothing exempted him.
	 *
	 * Taken from the decision itself, not re-derived for the readout. The readout used to
	 * name two of the three and recompute the rest on its own, so a man falling with no
	 * base under an over-limit manoeuvre was told he was strapped in or the mechanic was
	 * off, and neither was true.
	 */
	FString DescribeKnockdownExemptions() const;

	/**
	 * What the drift figure means right now, in words, for the log and the HUD alike:
	 * the carry failed, or his carrier is gone, or he has no floor under him, or he was
	 * knocked down, or none of those. The number alone cannot say, and it used to mean
	 * only the first.
	 */
	FString DescribeFooting() const;

	/**
	 * One step of look input, turned in the deck's axes by StarForge::ShipFrame::ApplyLookStep
	 * and stored in the ship's. Degrees; yaw positive to the right, pitch positive up.
	 */
	void TurnView(double YawDegrees, double PitchDegrees);

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
	 * and complains when the two disagree for FrameMismatchThresholdSeconds running.
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
	 *
	 * Called by every carry with bFromCarry, and once a tick from TickComponent without it,
	 * before the engine's own tick. The carry alone could never print the line it was
	 * written for: the engine stops calling UpdateBasedMovement the moment the base is gone,
	 * so "base none" was never logged and the diagnostic simply went quiet. From the tick
	 * it prints a change of state and nothing else; the once-a-second line stays the
	 * carry's.
	 */
	void LogDiagnostics(float DeltaSeconds, bool bFromCarry);

	/**
	 * Clears a binding whose carrier no longer exists, once, and says so. A destroyed hull
	 * invalidates the weak pointer below in the same frame, and that used to look exactly
	 * like never having been bound: the carry fell through to the stock path, the footing
	 * froze, the strap stayed on and Carry NOT RUNNING went quiet at the very moment the
	 * carry had stopped for good. Measured on the KillZ control at 60 fps: 22 s of a man
	 * still in the world with not one line about him.
	 */
	void ReleaseLostFrame();

	/** The controller the view is written to, or null for a pawn nobody possesses. */
	AController* GetOwnerController() const;

	/**
	 * Takes the controller's view as the truth if anyone but this component has moved it --
	 * the game mode at a restart, a possession -- or if there is no ship-local view yet.
	 *
	 * Compared against what the controller held after this component's own last write, not
	 * against what it was asked for: SetControlRotation declines a change within 1e-3 deg
	 * (Controller.cpp 140), so a declined write would read as somebody else's and re-seed
	 * the view from the stale value -- exactly the loss this exists to stop.
	 */
	void SyncViewFromController(const AController& Controller);

	/** Keeps the view where it is in the world while the pose it is stored against changes. */
	void ReexpressView(const FTransform& NewReference);

	/** Writes ship rotation x LocalView to the controller and records what it then holds. */
	void WriteView(AController& Controller, const FQuat& FrameRotation);

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

	/**
	 * Where he is looking, in the frame of LastFrameTransform: the ship's, once the carry has
	 * run. The control rotation is written from it, never composed onto.
	 *
	 * The view used to be carried by increments, each frame's turn of the ship composed onto
	 * the control rotation and written back, and the controller loses part of what it is
	 * given. It declines any change within 1e-3 deg (Controller.cpp 140), and FQuat::Rotator
	 * answers exactly +/-90 of pitch within 0.081 deg of the vertical (UnrealMath.cpp
	 * 698-708). Both are angles per frame, so the turn rate they swallow grows with the frame
	 * rate -- under 0.06 deg/s at 60 fps, under 0.4 at 400 -- and what an increment loses is
	 * lost for good, since the next one starts from the pose already moved on. Measured on the
	 * spike rig at 400 fps, flying forward and yawing at 0.24 deg/s: every write was declined,
	 * the view stood still in the world while the cabin turned past it, 2.8 deg off after
	 * twelve seconds. Held whole, it trails by the one frame's 0.0006 deg and no more.
	 *
	 * A hull that only turns does not show it, which is worth knowing before testing it so.
	 * The physics sync moves the hull's component only once its pose differs from the body's
	 * by 1e-4 in some quaternion component (EqualsNoScale, PrimitiveComponentPhysics.cpp 949),
	 * which is 0.0115 deg, so a hull turning in place turns in steps the controller accepts.
	 * One that is also moving clears the test on its translation every frame and turns in
	 * steps of a frame's worth.
	 */
	FQuat LocalView = FQuat::Identity;
	bool bLocalViewValid = false;

	/** What the controller held right after this component last wrote to it. */
	FRotator LastLeftControlRotation = FRotator::ZeroRotator;

	/** See GetLostFrameName. */
	FString LostFrameName;

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

	/**
	 * Diagnostic state for LogDiagnostics; meaningless unless sf.DriftDebug is on.
	 *
	 * The line prints the calls of the last whole frame, not the running count of the frame
	 * it is printed in. The running count is 1 on the first call of every frame, and the
	 * once-a-second line is always printed on a first call, so every periodic line read
	 * "calls/frame 1" on a hull the carry runs on twice a frame -- the doubled call the field
	 * exists to show, hidden by when it was read.
	 */
	uint64 DebugLastFrame = 0;
	int32 DebugCallsThisFrame = 0;
	int32 DebugCallsLastFrame = 0;
	int32 DebugCallsPerFramePeak = 0;
	uint64 DebugThrottleFrame = 0;
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

	/**
	 * This component's ticks, counted at the top of TickComponent and so before either of the
	 * engine's carry calls in the frame. What the carry gap is measured in; see
	 * GetSkippedCarryFrames for why it is not GFrameCounter.
	 */
	uint64 OwnTickCount = 0;

	/** OwnTickCount at the carry's last run, used to spot gaps in it. */
	uint64 LastCarryTick = 0;

	/** Carry ticks missed because the occupant was not attached to the ship, this binding. */
	int64 SkippedCarryFrames = 0;

	/** The same, over the session; no binding resets it. */
	int64 TotalSkippedCarryFrames = 0;

	/** See GetSecondsBoundUncarried. Advanced in LogOpenCarryGap, so a paused world adds nothing. */
	double SecondsBoundUncarried = 0.0;
	bool bNeverCarriedReported = false;

	/**
	 * Whether a frame was bound, and by what name, so a binding whose carrier has since been
	 * destroyed can be told from no binding at all. The weak pointer cannot: it reads null
	 * either way.
	 */
	bool bBoundToFrame = false;
	FString BoundFrameName;

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

	/** The exemptions behind the last footing decision; see DescribeKnockdownExemptions. */
	bool bLastDecisionKnockdownOff = false;
	bool bLastDecisionStrappedIn = false;
	bool bLastDecisionNoBase = false;

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

	/**
	 * The frame the handover was armed on, and whether the carry stopped before it was
	 * consumed. Both exist because "the first carry moved him X cm" is a figure over an
	 * interval the line did not name: the consuming carry can be one frame after the
	 * binding or twenty-two, and a reader cannot tell a measurement from an artefact
	 * without the count. A carry gap in between is worse than a long interval -- the gap
	 * re-seeds the transform the carry measures against, so the figure would be a zero
	 * taken over nothing at all, and zero is the verdict this instrument exists to issue.
	 */
	uint64 PendingHandoverFrame = 0;
	bool bPendingHandoverCarryGap = false;

	double LastHandoverJumpCm = 0.0;
	double LastHandoverGravityTurnDeg = 0.0;
	FString LastHandoverText;

	/** Cross-check state; see UpdateFrameCrossCheck and FrameMismatchThresholdSeconds. */
	int32 MismatchFrames = 0;
	double MismatchSeconds = 0.0;
	bool bMismatchReported = false;
	uint64 LastCrossCheckFrame = TNumericLimits<uint64>::Max();
	double SecondsSinceMismatchLog = 0.0;
};
