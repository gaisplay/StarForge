#include "Misc/AutomationTest.h"
#include "Ballistics.h"

#include <limits>

namespace
{
	/** The stage's muzzle speed in metres per second; the fixtures work in metres. */
	constexpr double MuzzleSpeed = 1000.0;
	constexpr double LifeSeconds = 5.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBallisticsMuzzleVelocityAddsTheCarrierTest,
	"StarForge.Core.Ballistics.MuzzleVelocityAddsTheCarrier",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBallisticsMuzzleVelocityAddsTheCarrierTest::RunTest(const FString& Parameters)
{
	// A ship at the boost ceiling of spec 7 firing straight ahead: 400 + 1000.
	const FVector Forward = StarForge::Ballistics::ComputeMuzzleVelocity(
		FVector(400.0, 0.0, 0.0), FVector(1.0, 0.0, 0.0), MuzzleSpeed);
	TestTrue(TEXT("a shot forward leaves at the sum"), Forward.Equals(FVector(1400.0, 0.0, 0.0), 1e-9));

	// The same ship firing astern. The shot still leaves the muzzle at 1000 relative to the
	// ship, so in the world it travels backwards at 600 -- it does not stop, and it does not
	// keep the ship's sign. Getting this wrong produces a weapon that works dead ahead and
	// nowhere else, which reads as a turret problem.
	const FVector Astern = StarForge::Ballistics::ComputeMuzzleVelocity(
		FVector(400.0, 0.0, 0.0), FVector(-1.0, 0.0, 0.0), MuzzleSpeed);
	TestTrue(TEXT("a shot astern leaves at the difference"), Astern.Equals(FVector(-600.0, 0.0, 0.0), 1e-9));

	// The direction is normalised by the function, not trusted from the caller: a barrel
	// axis read off a transform comes back at 1.0000000000000002 often enough, and an
	// un-normalised one would make muzzle speed depend on which way the turret points.
	const FVector Diagonal = StarForge::Ballistics::ComputeMuzzleVelocity(
		FVector::ZeroVector, FVector(3.0, 4.0, 0.0), MuzzleSpeed);
	TestTrue(TEXT("the barrel direction is normalised"),
		FMath::IsNearlyEqual(Diagonal.Size(), MuzzleSpeed, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBallisticsOneStepEqualsManyStepsTest,
	"StarForge.Core.Ballistics.OneStepEqualsManySteps",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBallisticsOneStepEqualsManyStepsTest::RunTest(const FString& Parameters)
{
	// Straight-line motion in a vacuum, so frame-rate independence is PROVABLE rather than
	// measurable: a thousand steps land where one does up to floating-point accumulation and
	// nothing else. 0.001 is not representable in a double, and the thousand steps come out
	// about 1e-11 from the one in position and 1e-15 in age; the tolerances below leave some
	// five and six orders of magnitude over that. The hull has gravity and linear damping
	// explicitly off; a projectile that fell or slowed would contradict the ship that fired it.
	StarForge::Ballistics::FProjectileState Start;
	Start.Position = FVector(10.0, -20.0, 30.0);
	Start.Velocity = FVector(600.0, 800.0, 0.0);

	StarForge::Ballistics::FProjectileStep Big;
	TestTrue(TEXT("one whole second steps"),
		StarForge::Ballistics::StepProjectile(Start, 1.0, LifeSeconds, Big));

	StarForge::Ballistics::FProjectileState Walking = Start;
	for (int32 i = 0; i < 1000; ++i)
	{
		StarForge::Ballistics::FProjectileStep Small;
		TestTrue(TEXT("each thousandth steps"),
			StarForge::Ballistics::StepProjectile(Walking, 0.001, LifeSeconds, Small));
		Walking = Small.NextState;
	}

	TestTrue(TEXT("a thousand steps land where one does"),
		Walking.Position.Equals(Big.NextState.Position, 1e-6));
	TestTrue(TEXT("and have aged the same"),
		FMath::IsNearlyEqual(Walking.AgeSeconds, Big.NextState.AgeSeconds, 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBallisticsSegmentsLeaveNoGapTest,
	"StarForge.Core.Ballistics.SegmentsLeaveNoGap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBallisticsSegmentsLeaveNoGapTest::RunTest(const FString& Parameters)
{
	// The whole point of a swept projectile: the segment the game sweeps this frame must
	// begin exactly where last frame's ended. A gap of even a float's width between frames
	// is a hole a target can sit in, and it would be found as "sometimes it goes through".
	// Exact equality, not nearly: the start is the previous position, it is not recomputed.
	StarForge::Ballistics::FProjectileState State;
	State.Position = FVector(0.0, 0.0, 0.0);
	State.Velocity = FVector(1000.0, 0.0, 0.0);

	FVector PreviousEnd = State.Position;
	for (int32 i = 0; i < 16; ++i)
	{
		StarForge::Ballistics::FProjectileStep Step;
		TestTrue(TEXT("steps"), StarForge::Ballistics::StepProjectile(State, 1.0 / 60.0, LifeSeconds, Step));

		TestTrue(TEXT("the segment starts where the last one ended"),
			Step.SegmentStart.Equals(PreviousEnd, 0.0));
		TestTrue(TEXT("and ends at the new position"),
			Step.SegmentEnd.Equals(Step.NextState.Position, 0.0));

		PreviousEnd = Step.SegmentEnd;
		State = Step.NextState;
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBallisticsExpiryIsInSecondsTest,
	"StarForge.Core.Ballistics.ExpiryIsInSeconds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBallisticsExpiryIsInSecondsTest::RunTest(const FString& Parameters)
{
	// Range is a design quantity. If the last step were not clipped to MaxAgeSeconds, range
	// would be speed times (life plus up to one frame) -- 16.7 m longer at 60 fps than at
	// 400. A range that grows when the frame rate drops is a frame count wearing a different
	// name, and this project has now found that shape three times.
	//
	// Not the stage's five seconds: 1/60 and 1/400 both divide 5 exactly, so neither frame rate
	// would end on a partial step and a last step clipped to the whole life instead of to what
	// is left of it would pass. Neither divides 5.001, so both halves below end on a partial step.
	const double OddLifeSeconds = 5.001;

	const auto TotalPath = [OddLifeSeconds](double Dt)
	{
		StarForge::Ballistics::FProjectileState State;
		State.Velocity = FVector(MuzzleSpeed, 0.0, 0.0);

		double Path = 0.0;
		for (int32 i = 0; i < 100000; ++i)
		{
			StarForge::Ballistics::FProjectileStep Step;
			if (!StarForge::Ballistics::StepProjectile(State, Dt, OddLifeSeconds, Step)) { break; }
			Path += (Step.SegmentEnd - Step.SegmentStart).Size();
			State = Step.NextState;
			if (Step.bExpired) { break; }
		}
		return Path;
	};

	const double Coarse = TotalPath(1.0 / 60.0);
	const double Fine = TotalPath(1.0 / 400.0);

	TestTrue(TEXT("range is speed times life at 60 fps"),
		FMath::IsNearlyEqual(Coarse, MuzzleSpeed * OddLifeSeconds, 1e-6));
	TestTrue(TEXT("and the same at 400"),
		FMath::IsNearlyEqual(Fine, MuzzleSpeed * OddLifeSeconds, 1e-6));

	// And a single step longer than the whole life still stops at the life, rather than
	// flying a hundred kilometres because the machine hitched.
	StarForge::Ballistics::FProjectileState Fresh;
	Fresh.Velocity = FVector(MuzzleSpeed, 0.0, 0.0);
	StarForge::Ballistics::FProjectileStep Hitch;
	TestTrue(TEXT("a hitch still steps"),
		StarForge::Ballistics::StepProjectile(Fresh, 30.0, OddLifeSeconds, Hitch));
	TestTrue(TEXT("a hitch is clipped to the life"),
		Hitch.SegmentEnd.Equals(FVector(MuzzleSpeed * OddLifeSeconds, 0.0, 0.0), 1e-6));
	TestTrue(TEXT("and is reported expired"), Hitch.bExpired);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBallisticsNonFiniteIsRefusedTest,
	"StarForge.Core.Ballistics.NonFiniteIsRefused",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FBallisticsNonFiniteIsRefusedTest::RunTest(const FString& Parameters)
{
	// Guards written as the negation of "valid", the same way CanDock's are, and for the
	// same reason: a NaN compares false against every plain comparison, so `if (Dt > 0)`
	// written the obvious way round lets a NaN through into the integrator, and the
	// projectile then sits at a position of NaNs sweeping a segment nobody can hit. Negated,
	// the first condition refuses it.
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const double Inf = std::numeric_limits<double>::infinity();

	// The fixture before the behaviour, as in DockingMathTest: a NaN that a compiler flag had
	// quietly turned into a finite number would make every refusal below pass for the wrong
	// reason.
	TestTrue(TEXT("the NaN really is not finite"), !FMath::IsFinite(NaN));
	TestTrue(TEXT("and the infinity really is not finite"), !FMath::IsFinite(Inf));

	StarForge::Ballistics::FProjectileStep Out;
	Out.SegmentEnd = FVector(7.0, 7.0, 7.0);
	const FVector Sentinel = Out.SegmentEnd;

	StarForge::Ballistics::FProjectileState Good;
	Good.Velocity = FVector(MuzzleSpeed, 0.0, 0.0);

	TestFalse(TEXT("zero dt is refused"),
		StarForge::Ballistics::StepProjectile(Good, 0.0, LifeSeconds, Out));
	TestFalse(TEXT("negative dt is refused"),
		StarForge::Ballistics::StepProjectile(Good, -0.016, LifeSeconds, Out));
	TestFalse(TEXT("NaN dt is refused"),
		StarForge::Ballistics::StepProjectile(Good, NaN, LifeSeconds, Out));

	StarForge::Ballistics::FProjectileState NanPosition = Good;
	NanPosition.Position = FVector(NaN, 0.0, 0.0);
	TestFalse(TEXT("a non-finite position is refused"),
		StarForge::Ballistics::StepProjectile(NanPosition, 0.016, LifeSeconds, Out));

	StarForge::Ballistics::FProjectileState InfVelocity = Good;
	InfVelocity.Velocity = FVector(Inf, 0.0, 0.0);
	TestFalse(TEXT("an infinite velocity is refused"),
		StarForge::Ballistics::StepProjectile(InfVelocity, 0.016, LifeSeconds, Out));

	StarForge::Ballistics::FProjectileState BadAge = Good;
	BadAge.AgeSeconds = NaN;
	TestFalse(TEXT("a non-finite age is refused"),
		StarForge::Ballistics::StepProjectile(BadAge, 0.016, LifeSeconds, Out));

	TestTrue(TEXT("and nothing written on any refusal"), Out.SegmentEnd.Equals(Sentinel, 0.0));

	// A fixture that two guards both refuse cannot fail when either one is deleted. The NaN age
	// above is such a fixture: it is not finite, and it also leaves a NaN remaining life. The
	// checks below say which guard is the only one in their way.

	// Only the remaining-life guard refuses a state already at its life. Without it the step
	// is Min(dt, 0), a zero-length success, and past the life Min(dt, negative) flies the
	// segment backwards and still reports success.
	StarForge::Ballistics::FProjectileState AtLife = Good;
	AtLife.AgeSeconds = LifeSeconds;
	TestFalse(TEXT("a state already at its life is refused"),
		StarForge::Ballistics::StepProjectile(AtLife, 0.016, LifeSeconds, Out));
	TestTrue(TEXT("and the at-life refusal wrote nothing"), Out.SegmentEnd.Equals(Sentinel, 0.0));

	// Only the finite-age guard refuses an age of minus infinity. The remaining life is then
	// plus infinity, which passes its own guard, and the projectile would never expire.
	StarForge::Ballistics::FProjectileState MinusInfiniteAge = Good;
	MinusInfiniteAge.AgeSeconds = -Inf;
	TestFalse(TEXT("an age of minus infinity is refused"),
		StarForge::Ballistics::StepProjectile(MinusInfiniteAge, 0.016, LifeSeconds, Out));
	TestTrue(TEXT("and the minus-infinite age refusal wrote nothing"), Out.SegmentEnd.Equals(Sentinel, 0.0));

	TestFalse(TEXT("a zero life is refused"),
		StarForge::Ballistics::StepProjectile(Good, 0.016, 0.0, Out));
	TestTrue(TEXT("and the zero-life refusal wrote nothing"), Out.SegmentEnd.Equals(Sentinel, 0.0));

	// The zero life above is also refused by the remaining-life guard, because an age of zero
	// leaves nothing remaining of a life of zero. For any age at or above zero the two guards
	// agree, so the life guard can only be pinned on its own with an age below zero.
	StarForge::Ballistics::FProjectileState NegativeAge = Good;
	NegativeAge.AgeSeconds = -1.0;
	TestFalse(TEXT("a zero life is refused even with life remaining"),
		StarForge::Ballistics::StepProjectile(NegativeAge, 0.016, 0.0, Out));
	TestTrue(TEXT("and that refusal wrote nothing"), Out.SegmentEnd.Equals(Sentinel, 0.0));

	return true;
}
