#include "Misc/AutomationTest.h"
#include "TurretMath.h"

namespace
{
	constexpr double Deg = UE_DOUBLE_PI / 180.0;

	/** The rig's limits: yaw +/-120, pitch -30..+60, 90 deg/s, half a degree is on target. */
	StarForge::Turret::FTurretLimits MakeLimits()
	{
		StarForge::Turret::FTurretLimits Limits;
		Limits.MaxYawRad = 120.0 * Deg;
		Limits.MinPitchRad = -30.0 * Deg;
		Limits.MaxPitchRad = 60.0 * Deg;
		Limits.TraverseRateRadPerSec = 90.0 * Deg;
		Limits.OnTargetToleranceRad = 0.5 * Deg;
		return Limits;
	}

	/** One frame at the rig's fixed 60 fps. */
	constexpr double Frame = 1.0 / 60.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretBearingDeadAheadTest,
	"StarForge.Core.Turret.BearingDeadAhead",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretBearingDeadAheadTest::RunTest(const FString& Parameters)
{
	StarForge::Turret::FTurretAim Bearing;
	const bool bFound = StarForge::Turret::ComputeBearing(
		FTransform::Identity, FVector(100.0, 0.0, 0.0), Bearing);

	TestTrue(TEXT("a target straight ahead has a bearing"), bFound);
	TestTrue(TEXT("yaw is zero"), FMath::IsNearlyZero(Bearing.YawRad, 1e-9));
	TestTrue(TEXT("pitch is zero"), FMath::IsNearlyZero(Bearing.PitchRad, 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretBearingYawMatchesEngineSignTest,
	"StarForge.Core.Turret.BearingYawMatchesEngineSign",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretBearingYawMatchesEngineSignTest::RunTest(const FString& Parameters)
{
	// Checked against the engine's own idea of a +45 yaw rather than against a number
	// believed to be right. FRotator's yaw sends +X toward +Y; the bearing has to agree
	// or the barrel swings one way while the readout prints the other.
	const FVector Ahead = FRotator(0.0, 45.0, 0.0).Vector() * 100.0;

	StarForge::Turret::FTurretAim Bearing;
	TestTrue(TEXT("found"), StarForge::Turret::ComputeBearing(FTransform::Identity, Ahead, Bearing));
	TestTrue(TEXT("a +45 engine yaw reads as +45 here"),
		FMath::IsNearlyEqual(Bearing.YawRad, 45.0 * Deg, 1e-6));
	TestTrue(TEXT("and carries no pitch"), FMath::IsNearlyZero(Bearing.PitchRad, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretBearingPitchIsElevationTest,
	"StarForge.Core.Turret.BearingPitchIsElevation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretBearingPitchIsElevationTest::RunTest(const FString& Parameters)
{
	// A +45 engine pitch lifts +X toward +Z, and so does this pitch.
	const FVector Up45 = FRotator(45.0, 0.0, 0.0).Vector() * 100.0;

	StarForge::Turret::FTurretAim Bearing;
	TestTrue(TEXT("found"), StarForge::Turret::ComputeBearing(FTransform::Identity, Up45, Bearing));
	TestTrue(TEXT("a +45 engine pitch reads as +45 here"),
		FMath::IsNearlyEqual(Bearing.PitchRad, 45.0 * Deg, 1e-6));
	TestTrue(TEXT("and carries no yaw"), FMath::IsNearlyZero(Bearing.YawRad, 1e-6));

	// Straight up is ninety, and yaw is then undefined but must not be garbage: atan2
	// of (0, 0) is zero, which is what a barrel pointing up would sensibly keep. The yaw
	// starts on a sentinel, so a bearing that left it alone cannot pass for one that wrote
	// the zero.
	Bearing.YawRad = 1.23;
	TestTrue(TEXT("found"), StarForge::Turret::ComputeBearing(
		FTransform::Identity, FVector(0.0, 0.0, 100.0), Bearing));
	TestTrue(TEXT("straight up is ninety degrees of pitch"),
		FMath::IsNearlyEqual(Bearing.PitchRad, 90.0 * Deg, 1e-9));
	TestTrue(TEXT("and zero yaw, not garbage"), Bearing.YawRad == 0.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretBearingIsTakenInTheMountFrameTest,
	"StarForge.Core.Turret.BearingIsTakenInTheMountFrame",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretBearingIsTakenInTheMountFrameTest::RunTest(const FString& Parameters)
{
	// The mount sits ten metres out along world Y and faces world +Y. A target a metre
	// further along Y is dead ahead of it. One a metre toward world -X is off its right
	// shoulder, because a +90 yaw sends local +Y onto world -X.
	const FTransform Mount(FRotator(0.0, 90.0, 0.0).Quaternion(), FVector(0.0, 1000.0, 0.0));

	StarForge::Turret::FTurretAim Ahead;
	TestTrue(TEXT("found"), StarForge::Turret::ComputeBearing(Mount, FVector(0.0, 1100.0, 0.0), Ahead));
	TestTrue(TEXT("a target along the mount's forward is dead ahead"),
		FMath::IsNearlyZero(Ahead.YawRad, 1e-6) && FMath::IsNearlyZero(Ahead.PitchRad, 1e-6));

	StarForge::Turret::FTurretAim Right;
	TestTrue(TEXT("found"), StarForge::Turret::ComputeBearing(Mount, FVector(-100.0, 1000.0, 0.0), Right));
	TestTrue(TEXT("a target off the mount's right shoulder bears +90"),
		FMath::IsNearlyEqual(Right.YawRad, 90.0 * Deg, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretBearingRefusesACoincidentTargetTest,
	"StarForge.Core.Turret.BearingRefusesACoincidentTarget",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretBearingRefusesACoincidentTargetTest::RunTest(const FString& Parameters)
{
	// There is no direction to a point you are standing on. Any angle returned for it
	// would be a fabrication, so the answer is "no bearing", and the output is left
	// alone so a caller cannot mistake a stale value for a fresh one.
	const FTransform Mount(FQuat::Identity, FVector(5.0, 6.0, 7.0));

	StarForge::Turret::FTurretAim Bearing;
	Bearing.YawRad = 1.23;
	Bearing.PitchRad = 4.56;

	TestFalse(TEXT("a target on the mount has no bearing"),
		StarForge::Turret::ComputeBearing(Mount, FVector(5.0, 6.0, 7.0), Bearing));
	TestTrue(TEXT("and the output is untouched"),
		FMath::IsNearlyEqual(Bearing.YawRad, 1.23, 1e-12) && FMath::IsNearlyEqual(Bearing.PitchRad, 4.56, 1e-12));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretAimRoundTripsThroughTheRotatorTest,
	"StarForge.Core.Turret.AimRoundTripsThroughTheRotator",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretAimRoundTripsThroughTheRotatorTest::RunTest(const FString& Parameters)
{
	// The executor turns the barrel by AimToRotator and the readout prints the aim.
	// The two agree only if a point along the rotated barrel bears back at the same
	// aim -- which is the whole contract between the numbers and the picture.
	StarForge::Turret::FTurretAim Aim;
	Aim.YawRad = 30.0 * Deg;
	Aim.PitchRad = 20.0 * Deg;

	const FVector AlongBarrel = StarForge::Turret::AimToRotator(Aim).Vector() * 250.0;

	StarForge::Turret::FTurretAim Back;
	TestTrue(TEXT("found"), StarForge::Turret::ComputeBearing(FTransform::Identity, AlongBarrel, Back));
	TestTrue(TEXT("yaw survives the round trip"), FMath::IsNearlyEqual(Back.YawRad, Aim.YawRad, 1e-5));
	TestTrue(TEXT("pitch survives the round trip"), FMath::IsNearlyEqual(Back.PitchRad, Aim.PitchRad, 1e-5));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretStepIsRateLimitedTest,
	"StarForge.Core.Turret.StepIsRateLimited",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretStepIsRateLimitedTest::RunTest(const FString& Parameters)
{
	// Ninety degrees a second over a sixtieth of a second is a degree and a half, on
	// each axis independently. 90 / 60 = 1.5.
	StarForge::Turret::FTurretAim Current;
	StarForge::Turret::FTurretAim Bearing;
	Bearing.YawRad = 45.0 * Deg;
	Bearing.PitchRad = 45.0 * Deg;

	const StarForge::Turret::FAimStep Step =
		StarForge::Turret::StepAim(Current, Bearing, MakeLimits(), Frame);

	TestTrue(TEXT("yaw moves a degree and a half"), FMath::IsNearlyEqual(Step.Aim.YawRad, 1.5 * Deg, 1e-9));
	TestTrue(TEXT("pitch moves a degree and a half"), FMath::IsNearlyEqual(Step.Aim.PitchRad, 1.5 * Deg, 1e-9));
	TestTrue(TEXT("and it is still slewing"), Step.State == StarForge::Turret::EAimState::Slewing);

	// The rate is per second, not per frame, and at a sixtieth of a second the two are one
	// number: a step of Rate / 60 passes everything above. At 400 fps the same second is
	// 400 steps of 0.225 degrees, where a per-frame step would traverse at 600 deg/s.
	const StarForge::Turret::FAimStep Fast =
		StarForge::Turret::StepAim(Current, Bearing, MakeLimits(), 1.0 / 400.0);
	TestTrue(TEXT("at 1/400 s yaw moves 0.225 degrees"), FMath::IsNearlyEqual(Fast.Aim.YawRad, 0.225 * Deg, 1e-9));
	TestTrue(TEXT("and so does pitch"), FMath::IsNearlyEqual(Fast.Aim.PitchRad, 0.225 * Deg, 1e-9));

	// And the other way: every bearing above lies up and to the right, so yaw and pitch were
	// only ever asked to increase. A step clamped to [0, MaxStep] instead of
	// [-MaxStep, MaxStep] would never bring the barrel back.
	StarForge::Turret::FTurretAim DownLeft;
	DownLeft.YawRad = -45.0 * Deg;
	DownLeft.PitchRad = -20.0 * Deg;
	const StarForge::Turret::FAimStep Back =
		StarForge::Turret::StepAim(Current, DownLeft, MakeLimits(), Frame);
	TestTrue(TEXT("toward a bearing on the left yaw moves -1.5 degrees"),
		FMath::IsNearlyEqual(Back.Aim.YawRad, -1.5 * Deg, 1e-9));
	TestTrue(TEXT("and toward one below pitch moves -1.5 degrees"),
		FMath::IsNearlyEqual(Back.Aim.PitchRad, -1.5 * Deg, 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretStepReportsOnTargetTest,
	"StarForge.Core.Turret.StepReportsOnTarget",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretStepReportsOnTargetTest::RunTest(const FString& Parameters)
{
	// A tenth of a degree short: the step is bigger than the error, so the barrel
	// lands exactly on the bearing rather than overshooting it, and says so.
	StarForge::Turret::FTurretAim Current;
	Current.YawRad = 44.9 * Deg;
	StarForge::Turret::FTurretAim Bearing;
	Bearing.YawRad = 45.0 * Deg;

	const StarForge::Turret::FAimStep Step =
		StarForge::Turret::StepAim(Current, Bearing, MakeLimits(), Frame);

	TestTrue(TEXT("the barrel arrives on the bearing"), FMath::IsNearlyEqual(Step.Aim.YawRad, 45.0 * Deg, 1e-9));
	TestTrue(TEXT("and reports on target"), Step.State == StarForge::Turret::EAimState::OnTarget);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretStepIsOnTargetJustInsideTheToleranceTest,
	"StarForge.Core.Turret.StepIsOnTargetJustInsideTheTolerance",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretStepIsOnTargetJustInsideTheToleranceTest::RunTest(const FString& Parameters)
{
	// Every other fixture here that reaches OnTarget lands on the bearing exactly,
	// which leaves the tolerance itself unbound: any value from a hair's breadth to
	// several degrees would read the same. It is the one number that decides what the
	// gunner's readout is willing to call calm, so it is pinned from both sides --
	// from inside here, from outside in the test below.
	//
	// The rate limit is what parks the barrel short of the bearing: 1.9 degrees to
	// travel and 1.5 in a frame leaves four tenths of a degree of error on each axis,
	// inside the fixture's half-degree tolerance and nowhere near zero.
	StarForge::Turret::FTurretAim Current;
	Current.YawRad = 8.1 * Deg;
	Current.PitchRad = 8.1 * Deg;
	StarForge::Turret::FTurretAim Bearing;
	Bearing.YawRad = 10.0 * Deg;
	Bearing.PitchRad = 10.0 * Deg;

	const StarForge::Turret::FAimStep Step =
		StarForge::Turret::StepAim(Current, Bearing, MakeLimits(), Frame);

	TestTrue(TEXT("yaw stops four tenths of a degree short of the bearing"),
		FMath::IsNearlyEqual(Step.Aim.YawRad, 9.6 * Deg, 1e-9));
	TestTrue(TEXT("pitch stops four tenths of a degree short of the bearing"),
		FMath::IsNearlyEqual(Step.Aim.PitchRad, 9.6 * Deg, 1e-9));
	TestTrue(TEXT("and an error inside the tolerance reads as on target"),
		Step.State == StarForge::Turret::EAimState::OnTarget);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretStepIsSlewingJustOutsideTheToleranceTest,
	"StarForge.Core.Turret.StepIsSlewingJustOutsideTheTolerance",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretStepIsSlewingJustOutsideTheToleranceTest::RunTest(const FString& Parameters)
{
	// The other side of the same number, and the side that costs something when it is
	// wrong. A tolerance widened by a bad conversion -- degrees handed over as radians
	// turns half a degree into twenty-eight -- prints a calm readout for a barrel
	// pointing well away from the target, and nothing goes red to say otherwise. Two
	// tenths of a degree further out than the test above is the whole difference
	// between the two answers.
	//
	// Each axis is taken outside the tolerance while the other sits exactly on the
	// bearing, so the number is bound on both and neither half of the conjunction in
	// StepAim can be dropped without a failure here.
	const StarForge::Turret::FTurretLimits Limits = MakeLimits();

	StarForge::Turret::FTurretAim Bearing;
	Bearing.YawRad = 10.0 * Deg;
	Bearing.PitchRad = 10.0 * Deg;

	// 2.1 degrees to travel and 1.5 in a frame: six tenths of a degree left over.
	StarForge::Turret::FTurretAim YawShort;
	YawShort.YawRad = 7.9 * Deg;
	YawShort.PitchRad = 10.0 * Deg;

	const StarForge::Turret::FAimStep YawStep =
		StarForge::Turret::StepAim(YawShort, Bearing, Limits, Frame);

	TestTrue(TEXT("yaw stops six tenths of a degree short of the bearing"),
		FMath::IsNearlyEqual(YawStep.Aim.YawRad, 9.4 * Deg, 1e-9));
	TestTrue(TEXT("with pitch already on the bearing"),
		FMath::IsNearlyEqual(YawStep.Aim.PitchRad, 10.0 * Deg, 1e-9));
	TestTrue(TEXT("and a yaw error outside the tolerance is still slewing"),
		YawStep.State == StarForge::Turret::EAimState::Slewing);

	StarForge::Turret::FTurretAim PitchShort;
	PitchShort.YawRad = 10.0 * Deg;
	PitchShort.PitchRad = 7.9 * Deg;

	const StarForge::Turret::FAimStep PitchStep =
		StarForge::Turret::StepAim(PitchShort, Bearing, Limits, Frame);

	TestTrue(TEXT("pitch stops six tenths of a degree short of the bearing"),
		FMath::IsNearlyEqual(PitchStep.Aim.PitchRad, 9.4 * Deg, 1e-9));
	TestTrue(TEXT("with yaw already on the bearing"),
		FMath::IsNearlyEqual(PitchStep.Aim.YawRad, 10.0 * Deg, 1e-9));
	TestTrue(TEXT("and a pitch error outside the tolerance is still slewing"),
		PitchStep.State == StarForge::Turret::EAimState::Slewing);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretStepAtTheStopIsNotOnTargetTest,
	"StarForge.Core.Turret.StepAtTheStopIsNotOnTarget",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretStepAtTheStopIsNotOnTargetTest::RunTest(const FString& Parameters)
{
	// Parked on the +120 stop with the target at +130. The angle cannot change, and
	// that is precisely why the angle alone would lie: it reads the same as a barrel
	// on a target at exactly 120. The state is what tells the two apart.
	StarForge::Turret::FTurretAim Current;
	Current.YawRad = 120.0 * Deg;
	StarForge::Turret::FTurretAim Bearing;
	Bearing.YawRad = 130.0 * Deg;

	const StarForge::Turret::FAimStep Step =
		StarForge::Turret::StepAim(Current, Bearing, MakeLimits(), Frame);

	TestTrue(TEXT("the barrel stays on the stop"), FMath::IsNearlyEqual(Step.Aim.YawRad, 120.0 * Deg, 1e-9));
	TestTrue(TEXT("and says it cannot bear, not that it is on target"),
		Step.State == StarForge::Turret::EAimState::CannotBear);

	// The same on the left, which is where half the arc's stops are. The yaw test is on the
	// bearing's magnitude: without the absolute value, -130 is under +120, the target reads
	// as bearable, and the barrel parked on the -120 stop ten degrees off reports Slewing for
	// ever instead. And the stop is a stop from both sides: the goal clamped only from above
	// would take the barrel to -121.5 on this step.
	StarForge::Turret::FTurretAim LeftStop;
	LeftStop.YawRad = -120.0 * Deg;
	StarForge::Turret::FTurretAim LeftBearing;
	LeftBearing.YawRad = -130.0 * Deg;

	const StarForge::Turret::FAimStep Left =
		StarForge::Turret::StepAim(LeftStop, LeftBearing, MakeLimits(), Frame);

	TestTrue(TEXT("the barrel stays on the -120 stop"), FMath::IsNearlyEqual(Left.Aim.YawRad, -120.0 * Deg, 1e-9));
	TestTrue(TEXT("and a target at -130 cannot be borne either"),
		Left.State == StarForge::Turret::EAimState::CannotBear);

	// And above the pitch ceiling, a pirate overhead at +70 against the +60 stop: one degree
	// from the stop, a step of 1.5 has to end on it and not past it, and the state has to say
	// the target is out of reach rather than that the barrel is still on its way. Every other
	// out-of-reach pitch here is below the floor.
	StarForge::Turret::FTurretAim NearCeiling;
	NearCeiling.PitchRad = 59.0 * Deg;
	StarForge::Turret::FTurretAim Overhead;
	Overhead.PitchRad = 70.0 * Deg;

	const StarForge::Turret::FAimStep Up =
		StarForge::Turret::StepAim(NearCeiling, Overhead, MakeLimits(), Frame);

	TestTrue(TEXT("the barrel stops on the +60 stop, not past it"), FMath::IsNearlyEqual(Up.Aim.PitchRad, 60.0 * Deg, 1e-9));
	TestTrue(TEXT("and a target above the ceiling cannot be borne"),
		Up.State == StarForge::Turret::EAimState::CannotBear);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretStepSaysCannotBearFromTheFirstStepTest,
	"StarForge.Core.Turret.StepSaysCannotBearFromTheFirstStep",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretStepSaysCannotBearFromTheFirstStepTest::RunTest(const FString& Parameters)
{
	// Below the pitch floor. The barrel starts toward the stop at the traverse rate
	// and says CannotBear from the first step, not only when it gets there: the
	// answer is about the target, not about how far the barrel has got.
	StarForge::Turret::FTurretAim Current;
	StarForge::Turret::FTurretAim Bearing;
	Bearing.PitchRad = -40.0 * Deg;

	const StarForge::Turret::FAimStep First =
		StarForge::Turret::StepAim(Current, Bearing, MakeLimits(), Frame);

	TestTrue(TEXT("pitch starts down at the traverse rate"), FMath::IsNearlyEqual(First.Aim.PitchRad, -1.5 * Deg, 1e-9));
	TestTrue(TEXT("and it already says cannot bear"), First.State == StarForge::Turret::EAimState::CannotBear);

	// Parked on the floor stop, still cannot bear, and never below the stop.
	Current.PitchRad = -30.0 * Deg;
	const StarForge::Turret::FAimStep Parked =
		StarForge::Turret::StepAim(Current, Bearing, MakeLimits(), Frame);

	TestTrue(TEXT("the barrel does not go below the floor stop"), FMath::IsNearlyEqual(Parked.Aim.PitchRad, -30.0 * Deg, 1e-9));
	TestTrue(TEXT("and still cannot bear"), Parked.State == StarForge::Turret::EAimState::CannotBear);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTurretStepYawTakesTheLongWayRoundThroughZeroTest,
	"StarForge.Core.Turret.StepYawTakesTheLongWayRoundThroughZero",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FTurretStepYawTakesTheLongWayRoundThroughZeroTest::RunTest(const FString& Parameters)
{
	// The header promises yaw always moves through zero, and no other fixture crosses
	// the wrap, so nothing held the code to it: replacing the step's difference with an
	// unwound shortest arc would leave every other test in this file green and break
	// the mount. With the stop at exactly pi -- documented legal -- the shortest arc
	// from -179 to +179 runs the wrong way, off the back of the mount, and the stop
	// clamp pins the barrel on -180 where it sits reporting Slewing for ever while the
	// target is in plain view.
	StarForge::Turret::FTurretLimits Limits = MakeLimits();
	Limits.MaxYawRad = UE_DOUBLE_PI;

	StarForge::Turret::FTurretAim Bearing;
	Bearing.YawRad = 179.0 * Deg;

	StarForge::Turret::FAimStep Step;
	Step.Aim.YawRad = -179.0 * Deg;

	// One step is enough to tell the two rules apart: through zero the barrel comes up
	// to -177.5, the shortest arc sends it down onto the -180 stop.
	Step = StarForge::Turret::StepAim(Step.Aim, Bearing, Limits, Frame);

	TestTrue(TEXT("the first step moves yaw toward zero, not toward the stop"),
		FMath::IsNearlyEqual(Step.Aim.YawRad, -177.5 * Deg, 1e-9));
	TestTrue(TEXT("and the long way round is a slew, not a refusal"),
		Step.State == StarForge::Turret::EAimState::Slewing);

	// And it gets there. 358 degrees at 90 a second is 3.978 s, which at 1.5 degrees a
	// frame is 238 whole steps and a short last one: 239 frames at the rig's 60 fps,
	// one of which has already been taken above.
	constexpr int32 FramesTheLongWayRound = 239;
	for (int32 FrameIndex = 1; FrameIndex < FramesTheLongWayRound; ++FrameIndex)
	{
		Step = StarForge::Turret::StepAim(Step.Aim, Bearing, Limits, Frame);
	}

	TestTrue(TEXT("after 239 frames the barrel is on the bearing at +179"),
		FMath::IsNearlyEqual(Step.Aim.YawRad, 179.0 * Deg, 1e-9));
	TestTrue(TEXT("and reports on target rather than stuck against a stop"),
		Step.State == StarForge::Turret::EAimState::OnTarget);

	// The same turn takes the same time at 400 fps, and the time is what is asserted: a
	// count of frames is a frame rate. The state turns OnTarget on the first step that ends
	// within the half-degree tolerance, 357.5 degrees out, so after 357.5 / 90 = 3.972 s of
	// summed steps and before one more step has passed -- 239 frames at 60 fps (3.983 s), 1589
	// at 400 (3.9725 s). A traverse counted per frame would arrive in 0.6 s at 400.
	const double TimeToTolerance = (358.0 - 0.5) / 90.0;
	for (const double StepSeconds : { 1.0 / 60.0, 1.0 / 400.0 })
	{
		StarForge::Turret::FAimStep Turn;
		Turn.Aim.YawRad = -179.0 * Deg;
		double Elapsed = 0.0;
		int32 Steps = 0;
		do
		{
			Turn = StarForge::Turret::StepAim(Turn.Aim, Bearing, Limits, StepSeconds);
			Elapsed += StepSeconds;
			++Steps;
		}
		while (Turn.State != StarForge::Turret::EAimState::OnTarget && Steps < 10000);

		TestTrue(FString::Printf(TEXT("at 1/%.0f s the long way round is on target after %.4f s, not sooner than %.4f"),
				1.0 / StepSeconds, Elapsed, TimeToTolerance),
			Elapsed >= TimeToTolerance - 1e-9);
		TestTrue(FString::Printf(TEXT("and within one step of it (%d steps)"), Steps),
			Elapsed < TimeToTolerance + StepSeconds);
	}

	return true;
}
