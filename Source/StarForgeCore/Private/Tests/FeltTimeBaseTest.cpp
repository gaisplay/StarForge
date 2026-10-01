#include "Misc/AutomationTest.h"
#include "ShipFrameMath.h"

/**
 * Finding F4: the frame component's second difference of position, on the time base of whatever
 * moved the carrier. StarForge::ShipFrame::EstimateStepVelocity and EstimateStepAcceleration say
 * what that is; these tests pin it against the two failures it was written for and against the
 * two it must not introduce -- a blind instrument and a moved figure.
 *
 * Frame times are floats, because that is what the engine hands TickComponent, and the form
 * before 2026-09-27 is written out below as the component wrote it, so the tests that measure
 * against it measure against the real thing.
 */
namespace
{
	using StarForge::ShipFrame::FFrameStep;
	using StarForge::ShipFrame::FFrameVelocity;

	constexpr double GravPlateLimit = StarForge::ShipFrame::GravPlateLimitMetersPerSecSq;

	/** The loop rig's release standoff and USFDockingComponent::CaptureSeconds. */
	constexpr double ReleaseMetres = 2.93;
	constexpr double CaptureSeconds = 1.0;

	/** The spike rig's full forward thrust: 800 kN on 50 t. */
	constexpr double SpikeThrust = 16.0;

	/** UPhysicsSettings::MaxPhysicsDeltaTime's default (PhysicsSettings.cpp 22), as the float it is. */
	const double MaxPhysicsStepSeconds = static_cast<double>(1.0f / 30.0f);

	/**
	 * Frames alternating 1.74:1 about 1/420 s, the rate F4 was felt at by hand. 1.74 is the ratio at
	 * which dividing by the frame, an over-read of (1 + 1.74) / 2 = 1.37, carries the release's 17.58
	 * to 24.1 m/s2 -- the size of the 20.2-23.7 the owner was knocked down at. Mean 1/420 s: the
	 * short frame is 2 / (2.74 * 420) = 1.738 ms and the long one 3.024 ms.
	 */
	const float JitterShort = static_cast<float>(2.0 / (2.74 * 420.0));
	const float JitterLong = static_cast<float>(1.74 * 2.0 / (2.74 * 420.0));

	FFrameStep MakeStep(const FVector& DisplacementMetres, double FrameSeconds, bool bAtStart, bool bAtEnd)
	{
		FFrameStep Step;
		Step.DisplacementMetres = DisplacementMetres;
		Step.FrameSeconds = FrameSeconds;
		Step.bSimulatingAtStart = bAtStart;
		Step.bSimulatingAtEnd = bAtEnd;
		return Step;
	}

	/**
	 * A kinematic move as USFDockingComponent::AdvanceRelease writes one: smoothstep of the time
	 * accumulated in double, and exactly the target on the frame the time runs out. StillFrames
	 * docked frames before it and as many at rest after, so the reading starts and ends from a
	 * carrier that is not moving. Frame times cycle through Frames.
	 */
	TArray<FFrameStep> KinematicSmoothstepSteps(
		const FVector& StartMetres,
		const FVector& TargetMetres,
		double Seconds,
		const TArray<float>& Frames,
		int32 StillFrames)
	{
		TArray<FFrameStep> Steps;
		int32 FrameIndex = 0;
		const auto NextFrame = [&Frames, &FrameIndex]()
		{
			return static_cast<double>(Frames[FrameIndex++ % Frames.Num()]);
		};

		for (int32 i = 0; i < StillFrames; ++i)
		{
			Steps.Add(MakeStep(FVector::ZeroVector, NextFrame(), false, false));
		}

		double Elapsed = 0.0;
		FVector Pose = StartMetres;
		bool bArrived = false;
		while (!bArrived)
		{
			const double FrameSeconds = NextFrame();
			Elapsed += FrameSeconds;
			const double Alpha = Elapsed / Seconds;
			bArrived = Alpha >= 1.0;

			const double Eased = Alpha * Alpha * (3.0 - 2.0 * Alpha);
			const FVector NewPose = bArrived ? TargetMetres : FMath::Lerp(StartMetres, TargetMetres, Eased);
			Steps.Add(MakeStep(NewPose - Pose, FrameSeconds, false, false));
			Pose = NewPose;
		}

		for (int32 i = 0; i < StillFrames; ++i)
		{
			Steps.Add(MakeStep(FVector::ZeroVector, NextFrame(), false, false));
		}

		return Steps;
	}

	/**
	 * A body Chaos integrates, semi-implicit Euler as Chaos does it: v += a h, then x += v h, with
	 * h the frame clamped to the physics maximum. Returns the steps its position makes, each
	 * simulated at both ends. AccelAt gives the acceleration for step i.
	 */
	template <typename FAccelAt>
	TArray<FFrameStep> SimulatedSteps(
		const FVector& StartMetres,
		const FVector& StartVelocity,
		const TArray<float>& Frames,
		int32 Count,
		FAccelAt AccelAt)
	{
		TArray<FFrameStep> Steps;
		FVector Position = StartMetres;
		FVector Velocity = StartVelocity;
		for (int32 i = 0; i < Count; ++i)
		{
			const double FrameSeconds = static_cast<double>(Frames[i % Frames.Num()]);
			const double H = FMath::Min(FrameSeconds, MaxPhysicsStepSeconds);
			Velocity += AccelAt(i) * H;
			const FVector NewPosition = Position + Velocity * H;
			Steps.Add(MakeStep(NewPosition - Position, FrameSeconds, true, true));
			Position = NewPosition;
		}
		return Steps;
	}

	/** One reading per step: the velocity from the first step on, the acceleration from the second. */
	struct FReadings
	{
		TArray<FVector> Velocities;
		TArray<FVector> Accelerations;
	};

	/**
	 * The frame component's loop over the estimator. A refused acceleration keeps the last one, as
	 * the component does, so the two arrays stay aligned with the steps.
	 */
	FReadings ReadOnTimeBase(const TArray<FFrameStep>& Steps, double MaxStepSeconds)
	{
		FReadings Out;
		FFrameVelocity Previous;
		FVector Held = FVector::ZeroVector;
		for (int32 i = 0; i < Steps.Num(); ++i)
		{
			FFrameVelocity Current;
			StarForge::ShipFrame::EstimateStepVelocity(Steps[i], MaxStepSeconds, Current);
			Out.Velocities.Add(Current.VelocityMetresPerSec);
			if (i > 0)
			{
				StarForge::ShipFrame::EstimateStepAcceleration(Previous, Current, Steps[i].FrameSeconds, Held);
				Out.Accelerations.Add(Held);
			}
			Previous = Current;
		}
		return Out;
	}

	/**
	 * The form before 2026-09-27, character for character what USFShipFrameComponent::TickComponent
	 * computed: the displacement divided by the float frame component by component, the difference
	 * of two of those divided by the same float through FVector's operator/. It is the mutant
	 * several tests below measure against -- dividing by dt_n.
	 */
	FReadings ReadByFrame(const TArray<FFrameStep>& Steps)
	{
		FReadings Out;
		FVector LinearVelocityMetersPerSec = FVector::ZeroVector;
		for (int32 i = 0; i < Steps.Num(); ++i)
		{
			const float DeltaTime = static_cast<float>(Steps[i].FrameSeconds);
			const FVector& Metres = Steps[i].DisplacementMetres;
			const FVector VelocityMetersPerSec(Metres.X / DeltaTime, Metres.Y / DeltaTime, Metres.Z / DeltaTime);
			Out.Velocities.Add(VelocityMetersPerSec);
			if (i > 0)
			{
				Out.Accelerations.Add((VelocityMetersPerSec - LinearVelocityMetersPerSec) / DeltaTime);
			}
			LinearVelocityMetersPerSec = VelocityMetersPerSec;
		}
		return Out;
	}

	double LargestX(const TArray<FVector>& Values)
	{
		double Largest = -TNumericLimits<double>::Max();
		for (const FVector& Value : Values) { Largest = FMath::Max(Largest, Value.X); }
		return Largest;
	}

	double SmallestX(const TArray<FVector>& Values)
	{
		double Smallest = TNumericLimits<double>::Max();
		for (const FVector& Value : Values) { Smallest = FMath::Min(Smallest, Value.X); }
		return Smallest;
	}

	double LargestSize(const TArray<FVector>& Values)
	{
		double Largest = 0.0;
		for (const FVector& Value : Values) { Largest = FMath::Max(Largest, Value.Size()); }
		return Largest;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFeltTimeBaseReleaseUnderJitterTest,
	"StarForge.Core.ShipFrame.TimeBase.ReleaseUnderJitterStaysUnderItsPeak",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFeltTimeBaseReleaseUnderJitterTest::RunTest(const FString& Parameters)
{
	// F4 itself: the release, 2.93 m over 1.0 s, peaks at 6d/T^2 = 17.58 m/s2 at both ends, under
	// the 19.62 limit, so a man standing through it must stay on his feet at any frame sequence.
	const double Peak = 6.0 * ReleaseMetres / (CaptureSeconds * CaptureSeconds);

	// Why the peak is a hard ceiling and not a figure to be near: between the middles of two frames
	// the velocity difference is the true acceleration integrated against a tent that rises across
	// the earlier frame and falls across the later one, never negative, whose area is exactly the
	// spacing (dt_n + dt_{n-1}) / 2. The reading is therefore a weighted mean of the acceleration and
	// cannot exceed its largest value. What is left is rounding: poses of at most 2.93 m carry a few
	// ulps, about 3e-15 m, over frames of 1.7 ms twice -- about 2e-9 m/s2. The tolerance is 1e-6.
	const double Tolerance = 1e-6;

	// And the reading must not sit far under it either, or the ceiling is passed by an instrument
	// that has gone blind: the acceleration falls from the peak at 12d/T^3 = 35.2 m/s3, and the first
	// reading wholly inside the release is centred within two long frames, 6.05 ms, of its start --
	// at most 0.21 under the peak.
	const double Blindness = 12.0 * ReleaseMetres / (CaptureSeconds * CaptureSeconds * CaptureSeconds)
		* 2.0 * static_cast<double>(JitterLong);

	// Both phases of the alternation, because which frame the release starts on decides which of the
	// two over-reads lands on its peak.
	const TArray<TArray<float>> Phases = { { JitterLong, JitterShort }, { JitterShort, JitterLong } };
	for (const TArray<float>& Frames : Phases)
	{
		const TArray<FFrameStep> Steps = KinematicSmoothstepSteps(
			FVector::ZeroVector, FVector(ReleaseMetres, 0.0, 0.0), CaptureSeconds, Frames, 5);

		const double OnTimeBase = LargestSize(ReadOnTimeBase(Steps, MaxPhysicsStepSeconds).Accelerations);
		const double ByFrame = LargestSize(ReadByFrame(Steps).Accelerations);

		TestTrue(FString::Printf(TEXT("never over the release's own %.2f m/s2 (read %.6f)"), Peak, OnTimeBase),
			OnTimeBase <= Peak + Tolerance);
		TestTrue(FString::Printf(TEXT("and not blind to it (read %.4f, allowed down to %.4f)"), OnTimeBase, Peak - Blindness),
			OnTimeBase >= Peak - Blindness);

		// The fixture's own check: divided by the frame, the same poses knock him down. Without this
		// the two lines above could pass on a sequence that never exposed F4 at all.
		TestTrue(FString::Printf(TEXT("where dividing by the frame reads %.2f, over the %.2f limit"), ByFrame, GravPlateLimit),
			ByFrame > GravPlateLimit);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFeltTimeBaseSimulatedUnderJitterTest,
	"StarForge.Core.ShipFrame.TimeBase.SimulatedThrustUnderJitterReadsItsAcceleration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFeltTimeBaseSimulatedUnderJitterTest::RunTest(const FString& Parameters)
{
	// The spike rig's 800 kN on 50 t, 16 m/s2, under the same jittered frames. Semi-implicit Euler
	// makes dx / h the velocity at each step's end, so the difference of two over the frame between
	// those ends is 16 exactly, whatever the frames do. Half a second from rest: 2 m at most, a few
	// ulps of which over 1.7 ms twice is about 3e-10 m/s2, under the 1e-9 asked.
	const TArray<float> Frames = { JitterLong, JitterShort };
	const TArray<FFrameStep> Steps = SimulatedSteps(
		FVector::ZeroVector, FVector::ZeroVector, Frames, 210,
		[](int32) { return FVector(SpikeThrust, 0.0, 0.0); });

	const FReadings Readings = ReadOnTimeBase(Steps, MaxPhysicsStepSeconds);

	double WorstError = 0.0;
	for (const FVector& Accel : Readings.Accelerations)
	{
		WorstError = FMath::Max(WorstError, FMath::Abs(Accel.X - SpikeThrust));
	}
	TestTrue(FString::Printf(TEXT("16 m/s2 of thrust reads 16 on every frame (worst error %.3g)"), WorstError),
		WorstError <= 1e-9);

	// The fixture's check that a simulated step must NOT be spaced like a kinematic one: over the
	// middles, (dt_n + dt_{n-1}) / 2, the same velocities read 16 * 2 / (1 + r) with r = dt_{n-1} / dt_n
	// -- 32 / (1 + 1/1.74) = 20.32 on a long frame after a short one and 32 / 2.74 = 11.68 on a short
	// after a long, over the limit under thrust that is under it.
	double MidpointLargest = 0.0;
	for (int32 i = 1; i < Steps.Num(); ++i)
	{
		const double Spacing = 0.5 * (Steps[i].FrameSeconds + Steps[i - 1].FrameSeconds);
		MidpointLargest = FMath::Max(MidpointLargest,
			(Readings.Velocities[i].X - Readings.Velocities[i - 1].X) / Spacing);
	}
	TestTrue(FString::Printf(TEXT("where midpoint spacing would read %.2f, over the limit"), MidpointLargest),
		MidpointLargest > GravPlateLimit);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFeltTimeBaseLetGoIsKinematicTest,
	"StarForge.Core.ShipFrame.TimeBase.TheLetGoStepIsKinematic",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFeltTimeBaseLetGoIsKinematicTest::RunTest(const FString& Parameters)
{
	// The end of a release as USFDockingComponent writes it since 2026-09-28: eased kinematic
	// frames; then the arrival, the frame that runs the release's time out, which writes the
	// target exactly and is kinematic at both ends; then, one tick later, the let-go, which
	// switches simulation on and moves nothing. That step began kinematic and ends simulating,
	// and its travel is what Chaos integrated from rest -- nothing, in sf.FeltDebug's record of a
	// release (2026-09-28-letgo-feltdebug-release-EXTRACT.log, frame 1809: simulating 0 -> 1,
	// dx 0). Judged by both ends it is kinematic, stamped at its middle; then the body under
	// thrust.
	//
	// The arrival is a 40 ms hitch, so it carries the last 40 ms of the move, 13.6 mm, rather
	// than the micrometres an ordinary frame leaves. Those are the poses the one-tick let-go at
	// the end of this test snaps across, so the two sequences differ in one thing: which tick
	// the snap is on.
	constexpr float ArrivalFrame = 0.040f;
	const TArray<float> Frames = { JitterLong, JitterShort };

	TArray<FFrameStep> Steps;
	double Elapsed = 0.0;
	double Pose = 0.0;
	int32 FrameIndex = 0;
	while (Elapsed + static_cast<double>(ArrivalFrame) < CaptureSeconds)
	{
		const double FrameSeconds = static_cast<double>(Frames[FrameIndex++ % Frames.Num()]);
		Elapsed += FrameSeconds;
		const double Alpha = Elapsed / CaptureSeconds;
		const double NewPose = Alpha * Alpha * (3.0 - 2.0 * Alpha) * ReleaseMetres;
		Steps.Add(MakeStep(FVector(NewPose - Pose, 0.0, 0.0), FrameSeconds, false, false));
		Pose = NewPose;
	}
	const TArray<FFrameStep> Eased = Steps;

	const double SnapMetres = ReleaseMetres - Pose;
	TestTrue(TEXT("the fixture's arrival is a real distance"), SnapMetres > 0.01);
	const int32 ArrivalIndex = Steps.Add(MakeStep(FVector(SnapMetres, 0.0, 0.0), ArrivalFrame, false, false));

	const double LetGoFrame = static_cast<double>(Frames[FrameIndex % Frames.Num()]);
	const int32 LetGoIndex = Steps.Add(MakeStep(FVector::ZeroVector, LetGoFrame, false, true));

	// Then the hull under the spike rig's thrust, from rest at the target.
	const TArray<FFrameStep> Thrust = SimulatedSteps(FVector(ReleaseMetres, 0.0, 0.0), FVector::ZeroVector, Frames, 4,
		[](int32) { return FVector(SpikeThrust, 0.0, 0.0); });
	Steps.Append(Thrust);

	FFrameVelocity Arrival;
	StarForge::ShipFrame::EstimateStepVelocity(Steps[ArrivalIndex], MaxPhysicsStepSeconds, Arrival);

	FFrameVelocity LetGo;
	TestTrue(TEXT("the let-go step has a velocity"),
		StarForge::ShipFrame::EstimateStepVelocity(Steps[LetGoIndex], MaxPhysicsStepSeconds, LetGo));
	TestTrue(TEXT("of nothing, because nothing moved it"), LetGo.VelocityMetresPerSec.X == 0.0);

	// With no travel the divisor cannot show; the stamp is what the rule decides. Judged by its
	// end alone the step would be stamped at its end, half a frame later.
	TestTrue(TEXT("and it is stamped at its middle"),
		LetGo.StampOffsetSeconds == -0.5 * LetGoFrame);

	// So the let-go frame reads the arrival's mean velocity going to zero between the two
	// middles: about -16.3 m/s2 here, and -8.7 at a fixed 60 fps, where the arrival carries the
	// last 2.4 mm.
	FVector LetGoAccel = FVector::ZeroVector;
	TestTrue(TEXT("the let-go step gives an acceleration"),
		StarForge::ShipFrame::EstimateStepAcceleration(Arrival, LetGo, LetGoFrame, LetGoAccel));
	const double ExpectedLetGo = (0.0 - Arrival.VelocityMetresPerSec.X)
		/ (0.5 * LetGoFrame + 0.5 * static_cast<double>(ArrivalFrame));
	TestTrue(FString::Printf(TEXT("spaced between the two middles (read %.6f, expected %.6f)"), LetGoAccel.X, ExpectedLetGo),
		FMath::IsNearlyEqual(LetGoAccel.X, ExpectedLetGo, 1e-9));

	// And the first simulated step after it is spaced from that middle: 16 h over h + dt / 2.
	const FFrameStep& First = Steps[LetGoIndex + 1];
	FFrameVelocity FirstVelocity;
	StarForge::ShipFrame::EstimateStepVelocity(First, MaxPhysicsStepSeconds, FirstVelocity);

	FVector Accel = FVector::ZeroVector;
	TestTrue(TEXT("the step after it gives an acceleration"),
		StarForge::ShipFrame::EstimateStepAcceleration(LetGo, FirstVelocity, First.FrameSeconds, Accel));
	const double Expected = SpikeThrust * First.FrameSeconds / (First.FrameSeconds + 0.5 * LetGoFrame);
	TestTrue(FString::Printf(TEXT("spaced from the let-go step's middle (read %.6f, expected %.6f)"), Accel.X, Expected),
		FMath::IsNearlyEqual(Accel.X, Expected, 1e-9));

	// What it is for. Every pair of estimates is a weighted mean of the true acceleration, and the
	// true one here never leaves [-17.58, +16]: the release, rest, then thrust. So no frame reads
	// more than the release's peak. Tolerance as in ReleaseUnderJitterStaysUnderItsPeak.
	const double Peak = 6.0 * ReleaseMetres / (CaptureSeconds * CaptureSeconds);
	const double Largest = LargestSize(ReadOnTimeBase(Steps, MaxPhysicsStepSeconds).Accelerations);
	TestTrue(FString::Printf(TEXT("nothing through the let-go reads over %.2f (read %.2f)"), Peak, Largest),
		Largest <= Peak + 1e-6);

	// What the rule still guards: a move written into the let-go tick itself, which is how the
	// release let go before 2026-09-28 -- the same snap, on a step that began kinematic and ends
	// simulating, with nothing added for the physics step inside it. Its travel is a teleport, so
	// it is kinematic: over the whole 40 ms and stamped 20 ms before the frame's end. Judged by its
	// end alone it would be divided by the 33.3 ms physics step, 20% higher, and stamped at the
	// end. Both compared exactly, because each is the same division the estimator makes.
	TArray<FFrameStep> OneTick = Eased;
	const int32 SnapIndex = OneTick.Add(MakeStep(FVector(SnapMetres, 0.0, 0.0), ArrivalFrame, false, true));
	OneTick.Append(Thrust);

	FFrameVelocity Snap;
	StarForge::ShipFrame::EstimateStepVelocity(OneTick[SnapIndex], MaxPhysicsStepSeconds, Snap);
	TestTrue(TEXT("a snap on the switching step is divided by its frame, not by the physics step"),
		Snap.VelocityMetresPerSec.X == SnapMetres / static_cast<double>(ArrivalFrame));
	TestTrue(TEXT("and stamped at its middle"),
		Snap.StampOffsetSeconds == -0.5 * static_cast<double>(ArrivalFrame));

	const double OneTickLargest = LargestSize(ReadOnTimeBase(OneTick, MaxPhysicsStepSeconds).Accelerations);
	TestTrue(FString::Printf(TEXT("and nothing through it reads over %.2f either (read %.2f)"), Peak, OneTickLargest),
		OneTickLargest <= Peak + 1e-6);

	// The fixture's own check, with the mutant written in: the one-tick snap judged by its end
	// alone. Divided by h its velocity is 13.6 mm / 33.3 ms = 0.407 m/s, and spaced from the
	// next step by that step's 3.02 ms alone, the next frame reads (0.048 - 0.407) / 0.00302 --
	// about -119 m/s2, a knockdown nobody deserved. On the two-tick let-go above the same mutant
	// only moves the stamps of a step with no travel and reads under the peak, so this is the
	// sequence that can show it.
	TArray<FFrameStep> JudgedByItsEnd = OneTick;
	JudgedByItsEnd[SnapIndex].bSimulatingAtStart = true;
	const double MutantLargest = LargestSize(ReadOnTimeBase(JudgedByItsEnd, MaxPhysicsStepSeconds).Accelerations);
	TestTrue(FString::Printf(TEXT("where judging the step by its end reads %.1f, over the limit"), MutantLargest),
		MutantLargest > GravPlateLimit);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFeltTimeBaseCaptureIsKinematicTest,
	"StarForge.Core.ShipFrame.TimeBase.TheCaptureStepIsKinematic",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFeltTimeBaseCaptureIsKinematicTest::RunTest(const FString& Parameters)
{
	// The other half of "simulating at both samples", the let-go test's mirror: the start of a
	// capture as USFDockingComponent writes it for sf.Dock and for sf.Dock 2's re-dock. The hull
	// simulates at rest; then one pre-physics tick stops the simulation and, in the same tick, makes
	// the run-in's first eased step (TickComponent polls the command and then advances the capture,
	// SFDockingComponent.cpp 763-775). That step began simulating and ends kinematic, and its
	// travel is the eased step, so it is kinematic: stamped at its middle. Four frames at rest
	// before it are what Chaos integrates for a body at rest -- no travel -- so they are the
	// kinematic fixture's still frames marked simulating.
	const double Peak = 6.0 * ReleaseMetres / (CaptureSeconds * CaptureSeconds);
	constexpr int32 StillFrames = 4;

	const TArray<TArray<float>> Sequences = { { 1.0f / 60.0f }, { JitterLong, JitterShort } };
	for (const TArray<float>& Frames : Sequences)
	{
		TArray<FFrameStep> Steps = KinematicSmoothstepSteps(
			FVector::ZeroVector, FVector(ReleaseMetres, 0.0, 0.0), CaptureSeconds, Frames, StillFrames);
		for (int32 i = 0; i < StillFrames; ++i)
		{
			Steps[i].bSimulatingAtStart = true;
			Steps[i].bSimulatingAtEnd = true;
		}
		const int32 CaptureIndex = StillFrames;
		Steps[CaptureIndex].bSimulatingAtStart = true;

		FFrameVelocity Capture;
		StarForge::ShipFrame::EstimateStepVelocity(Steps[CaptureIndex], MaxPhysicsStepSeconds, Capture);
		TestTrue(TEXT("the capture's first step is divided by its frame"),
			Capture.VelocityMetresPerSec.X == Steps[CaptureIndex].DisplacementMetres.X / Steps[CaptureIndex].FrameSeconds);
		TestTrue(TEXT("and stamped at its middle"),
			Capture.StampOffsetSeconds == -0.5 * Steps[CaptureIndex].FrameSeconds);

		// Judged correctly, the first reading is the mean of the first eased step over half its
		// frame. At 1/60 s: 2.93 * (3/60^2 - 2/60^3) = 2.4145 mm, 0.14487 m/s, over 1/120 s is 17.38,
		// under the 17.58 peak as the weighted mean must be; the rest of the run-in is the release's
		// shape played forwards, bounded the same way. Tolerance as in
		// ReleaseUnderJitterStaysUnderItsPeak.
		const double Largest = LargestSize(ReadOnTimeBase(Steps, MaxPhysicsStepSeconds).Accelerations);
		TestTrue(FString::Printf(TEXT("nothing through the capture reads over %.2f (read %.4f)"), Peak, Largest),
			Largest <= Peak + 1e-6);

		// The fixture's own check, with the mutant written in: the first step judged by its start
		// alone, which is the same as marking it simulating at its end too. Stamped at its end, it
		// is half a frame from the next kinematic middle, and the next reading is a whole frame's
		// velocity change over half a frame. At 1/60 s the second step's mean is
		// 2.93 * (s(2/60) - s(1/60)) * 60 = 0.42811 m/s, 0.28324 above the first, over 1/120 s --
		// 33.99, a knockdown on every capture from rest.
		TArray<FFrameStep> JudgedByItsStart = Steps;
		JudgedByItsStart[CaptureIndex].bSimulatingAtEnd = true;
		const double MutantLargest = LargestSize(ReadOnTimeBase(JudgedByItsStart, MaxPhysicsStepSeconds).Accelerations);
		TestTrue(FString::Printf(TEXT("where judging the step by its start reads %.2f, over the limit"), MutantLargest),
			MutantLargest > GravPlateLimit);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFeltTimeBaseLongFrameTest,
	"StarForge.Core.ShipFrame.TimeBase.ALongFrameAtCruiseReadsNoAcceleration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFeltTimeBaseLongFrameTest::RunTest(const FString& Parameters)
{
	// Case (2) of F4, predicted and never measured: a hull cruising at 200 m/s at 400 fps, and one
	// 40 ms frame. Physics advances only 1/30 s of it (ChaosScene.cpp 344), so the hull covers 6.67 m
	// in a frame 40 ms long. It is not accelerating, and must read zero on that frame and the next.
	//
	// Rounding: positions under 15 m, a few ulps over 2.5 ms twice, about 1e-9 m/s2; tolerance 1e-6.
	constexpr double Cruise = 200.0;
	constexpr float NormalFrame = 1.0f / 400.0f;
	constexpr float LongFrame = 0.040f;

	TArray<float> Frames;
	for (int32 i = 0; i < 10; ++i) { Frames.Add(NormalFrame); }
	Frames.Add(LongFrame);
	for (int32 i = 0; i < 5; ++i) { Frames.Add(NormalFrame); }
	const int32 LongAt = 10;

	const TArray<FFrameStep> Steps = SimulatedSteps(
		FVector::ZeroVector, FVector(Cruise, 0.0, 0.0), Frames, Frames.Num(),
		[](int32) { return FVector::ZeroVector; });

	const FReadings OnTimeBase = ReadOnTimeBase(Steps, MaxPhysicsStepSeconds);
	const FReadings ByFrame = ReadByFrame(Steps);

	// Acceleration i belongs to step i + 1.
	const double OnTheLongFrame = OnTimeBase.Accelerations[LongAt - 1].X;
	const double OnTheNext = OnTimeBase.Accelerations[LongAt].X;
	TestTrue(FString::Printf(TEXT("the long frame reads no acceleration (read %.3g)"), OnTheLongFrame),
		FMath::Abs(OnTheLongFrame) <= 1e-6);
	TestTrue(FString::Printf(TEXT("nor does the frame after it (read %.3g)"), OnTheNext),
		FMath::Abs(OnTheNext) <= 1e-6);
	TestTrue(TEXT("and the velocity is the cruise throughout"),
		FMath::IsNearlyEqual(OnTimeBase.Velocities[LongAt].X, Cruise, 1e-9));

	// What dividing by the frame reads there -- which is also what h = dt reads, since a simulated
	// step divided by its frame is that form. The long frame reports 200 * (1/30) / 0.04 = 166.67 m/s,
	// so (166.67 - 200) / 0.04 = -833.3, and the recovery (200 - 166.67) / 0.0025 = +13 333.3.
	// Within 0.5 because the frame and the clamp are floats: -833.333 and +13 333.329 exactly.
	const double TodayOnTheLongFrame = ByFrame.Accelerations[LongAt - 1].X;
	const double TodayOnTheNext = ByFrame.Accelerations[LongAt].X;
	TestTrue(FString::Printf(TEXT("dividing by the frame reads -833.3 on it (read %.3f)"), TodayOnTheLongFrame),
		FMath::IsNearlyEqual(TodayOnTheLongFrame, -833.3, 0.5));
	TestTrue(FString::Printf(TEXT("and +13 333.3 on the next (read %.3f)"), TodayOnTheNext),
		FMath::IsNearlyEqual(TodayOnTheNext, 13333.3, 0.5));

	// sf.FeltTimeBase 0 is this estimator handed a physics maximum of 0 and every step simulated at
	// both ends, and it is the negative control only if that is the form above exactly -- the long
	// frame divided by its whole 40 ms, not by the clamp and not by min(dt, 0) = 0, which would put
	// an infinity into the control. Bit for bit, because it is the same two divisions.
	const FReadings Unclamped = ReadOnTimeBase(Steps, 0.0);
	TestTrue(TEXT("with no physics maximum the long frame is divided by its whole 40 ms"),
		Unclamped.Velocities[LongAt].X == Steps[LongAt].DisplacementMetres.X / static_cast<double>(LongFrame));

	int32 Mismatches = 0;
	for (int32 i = 0; i < Unclamped.Velocities.Num(); ++i)
	{
		Mismatches += (Unclamped.Velocities[i] == ByFrame.Velocities[i]) ? 0 : 1;
	}
	for (int32 i = 0; i < Unclamped.Accelerations.Num(); ++i)
	{
		Mismatches += (Unclamped.Accelerations[i] == ByFrame.Accelerations[i]) ? 0 : 1;
	}
	TestEqual(TEXT("and every figure is dividing by the frame, bit for bit, the long frame included"), Mismatches, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFeltTimeBaseOverLimitMoveTest,
	"StarForge.Core.ShipFrame.TimeBase.AnOverLimitKinematicMoveStillReadsOverTheLimit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFeltTimeBaseOverLimitMoveTest::RunTest(const FString& Parameters)
{
	// The fix must not blind the instrument. The same 2.93 m over 0.8 s peaks at
	// 6 * 2.93 / 0.64 = 27.47 m/s2, genuinely over the limit, and a man standing through it must go
	// down near both ends. The acceleration falls from each end at 12d/T^3 = 68.7 m/s3; the reading
	// nearest an end is centred about one frame in, so at 1/60 s it reads 27.47 - 68.7 / 60 = 26.3,
	// and under the jitter about 27.3 -- both well over 19.62.
	constexpr double Seconds = 0.8;
	const TArray<TArray<float>> Sequences = { { 1.0f / 60.0f }, { JitterLong, JitterShort } };

	for (const TArray<float>& Frames : Sequences)
	{
		const TArray<FFrameStep> Steps = KinematicSmoothstepSteps(
			FVector::ZeroVector, FVector(ReleaseMetres, 0.0, 0.0), Seconds, Frames, 5);
		const TArray<FVector> Accelerations = ReadOnTimeBase(Steps, MaxPhysicsStepSeconds).Accelerations;

		const double Start = LargestX(Accelerations);
		const double End = SmallestX(Accelerations);
		TestTrue(FString::Printf(TEXT("over the limit as it sets off (read %.2f)"), Start), Start > GravPlateLimit);
		TestTrue(FString::Printf(TEXT("and as it comes to rest (read %.2f)"), End), End < -GravPlateLimit);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFeltTimeBaseFixedStepIdentityTest,
	"StarForge.Core.ShipFrame.TimeBase.AFixedStepReadsBitForBitAsBefore",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFeltTimeBaseFixedStepIdentityTest::RunTest(const FString& Parameters)
{
	// Every rig figure recorded before 2026-09-27 was taken at a fixed step, and it stands only if a
	// run of one kind at such a step reads exactly what dividing by the frame read -- ==, not nearly:
	// a last-bit difference in the acceleration is a different knockdown on the frame that sits at
	// the limit, and the knockdown chatters at its edge. ShipFrameMath.cpp says why it holds.
	//
	// Three axes and a position far from the origin, so every component carries rounding of its own.
	const FVector Far(1234.5, -678.9, 42.0);

	for (const float Frame : { 1.0f / 60.0f, 1.0f / 400.0f })
	{
		const TArray<float> Frames = { Frame };

		// A hull under a thrust that changes every frame, from a velocity it already had.
		const TArray<FFrameStep> Simulated = SimulatedSteps(Far, FVector(12.0, -3.5, 0.25), Frames, 120,
			[](int32 i) { return FVector(16.0 - 0.1 * i, -7.5 + 0.05 * i, 3.25); });

		// A release along an axis that is not a coordinate axis.
		const TArray<FFrameStep> Kinematic = KinematicSmoothstepSteps(
			Far, Far + FVector(0.6, 0.8, 0.0) * ReleaseMetres, CaptureSeconds, Frames, 3);

		for (const TArray<FFrameStep>* Steps : { &Simulated, &Kinematic })
		{
			const FReadings OnTimeBase = ReadOnTimeBase(*Steps, MaxPhysicsStepSeconds);
			const FReadings ByFrame = ReadByFrame(*Steps);

			int32 VelocityMismatches = 0;
			for (int32 i = 0; i < OnTimeBase.Velocities.Num(); ++i)
			{
				VelocityMismatches += (OnTimeBase.Velocities[i] == ByFrame.Velocities[i]) ? 0 : 1;
			}

			int32 AccelMismatches = 0;
			for (int32 i = 0; i < OnTimeBase.Accelerations.Num(); ++i)
			{
				AccelMismatches += (OnTimeBase.Accelerations[i] == ByFrame.Accelerations[i]) ? 0 : 1;
			}

			const TCHAR* Kind = (Steps == &Simulated) ? TEXT("simulated") : TEXT("kinematic");
			TestEqual(FString::Printf(TEXT("%s at 1/%.0f s: every velocity bit for bit"), Kind, 1.0 / Frame),
				VelocityMismatches, 0);
			TestEqual(FString::Printf(TEXT("%s at 1/%.0f s: every acceleration bit for bit"), Kind, 1.0 / Frame),
				AccelMismatches, 0);
		}
	}

	return true;
}
