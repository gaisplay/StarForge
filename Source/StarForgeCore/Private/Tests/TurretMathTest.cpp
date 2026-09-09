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
	// of (0, 0) is zero, which is what a barrel pointing up would sensibly keep.
	TestTrue(TEXT("found"), StarForge::Turret::ComputeBearing(
		FTransform::Identity, FVector(0.0, 0.0, 100.0), Bearing));
	TestTrue(TEXT("straight up is ninety degrees of pitch"),
		FMath::IsNearlyEqual(Bearing.PitchRad, 90.0 * Deg, 1e-9));

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
