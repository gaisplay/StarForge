#include "Misc/AutomationTest.h"
#include "DockingMath.h"

namespace
{
	constexpr double Deg = UE_DOUBLE_PI / 180.0;

	/** The ship's authored window: 3 m, 1 m/s, 10 degrees on each of the two axes. */
	StarForge::Docking::FDockingLimits MakeLimits()
	{
		StarForge::Docking::FDockingLimits Limits;
		Limits.DistanceMetres = 3.0;
		Limits.SpeedMetresPerSec = 1.0;
		Limits.ForwardAngleRad = 10.0 * Deg;
		Limits.UpAngleRad = 10.0 * Deg;
		return Limits;
	}

	/** An approach comfortably inside that window, for the refusal tests to spoil one field of. */
	StarForge::Docking::FDockingApproach MakeGoodApproach()
	{
		StarForge::Docking::FDockingApproach Approach;
		Approach.Distance = 2.0;
		Approach.SpeedMetresPerSec = 0.3;
		Approach.ForwardAngleRad = 4.0 * Deg;
		Approach.UpAngleRad = 2.0 * Deg;
		return Approach;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingDistanceBetweenPortsTest,
	"StarForge.Core.Docking.DistanceBetweenPorts",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingDistanceBetweenPortsTest::RunTest(const FString& Parameters)
{
	// Two ports three metres apart along X, facing each other: the ship's port at the
	// origin looking along +X, the station's at 300 looking back along -X. Distance is
	// between the port origins and nothing else -- not between hulls, which are metres
	// of geometry away in both directions.
	const FTransform ShipPort(FQuat::Identity, FVector::ZeroVector);
	const FTransform StationPort(FRotator(0.0, 180.0, 0.0).Quaternion(), FVector(300.0, 0.0, 0.0));

	const StarForge::Docking::FDockingApproach Approach =
		StarForge::Docking::ComputeDockingApproach(ShipPort, StationPort, 0.0);

	TestTrue(TEXT("distance is between the port origins"),
		FMath::IsNearlyEqual(Approach.Distance, 300.0, 1e-9));
	TestTrue(TEXT("facing each other is zero forward angle"),
		FMath::IsNearlyZero(Approach.ForwardAngleRad, 1e-6));
	TestTrue(TEXT("and zero up angle"), FMath::IsNearlyZero(Approach.UpAngleRad, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingSpeedIsCarriedThroughTest,
	"StarForge.Core.Docking.SpeedIsCarriedThrough",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingSpeedIsCarriedThroughTest::RunTest(const FString& Parameters)
{
	// The relative speed is carried, not computed. This function sees two poses at one
	// instant and cannot differentiate anything; the caller has a velocity instrument
	// and passes what it reads, in SI. Any arithmetic here would be a second, worse
	// answer to a question already answered.
	const StarForge::Docking::FDockingApproach Approach =
		StarForge::Docking::ComputeDockingApproach(
			FTransform::Identity, FTransform(FVector(100.0, 0.0, 0.0)), 0.75);

	TestTrue(TEXT("the speed passed in is the speed reported"),
		FMath::IsNearlyEqual(Approach.SpeedMetresPerSec, 0.75, 1e-12));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingForwardAngleIsTheMisalignmentTest,
	"StarForge.Core.Docking.ForwardAngleIsTheMisalignment",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingForwardAngleIsTheMisalignmentTest::RunTest(const FString& Parameters)
{
	// The station's port is turned 15 degrees past facing the ship: 180 + 15. The angle
	// under test is between the ship's +X and the station's -X, so a station rotated
	// fifteen degrees off dead-ahead reads as fifteen, not as a hundred and ninety-five.
	const FTransform ShipPort(FQuat::Identity, FVector::ZeroVector);
	const FTransform StationPort(FRotator(0.0, 195.0, 0.0).Quaternion(), FVector(300.0, 0.0, 0.0));

	const StarForge::Docking::FDockingApproach Approach =
		StarForge::Docking::ComputeDockingApproach(ShipPort, StationPort, 0.0);

	TestTrue(TEXT("fifteen degrees off dead-ahead reads as fifteen"),
		FMath::IsNearlyEqual(Approach.ForwardAngleRad, 15.0 * Deg, 1e-6));

	// A yaw cannot tilt Z, so the second axis has to stay clean. Two angles that move
	// together would be one angle wearing two names.
	TestTrue(TEXT("and a yaw leaves the up angle alone"),
		FMath::IsNearlyZero(Approach.UpAngleRad, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingUpAngleIsTheRollTest,
	"StarForge.Core.Docking.UpAngleIsTheRoll",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingUpAngleIsTheRollTest::RunTest(const FString& Parameters)
{
	// Built from the two axes it is meant to have rather than from a rotator, so the
	// fixture states the geometry instead of trusting a composition order -- and the
	// first two assertions check the fixture before the third checks the function.
	const double Tilt = 20.0 * Deg;
	const FVector StationForward(-1.0, 0.0, 0.0);
	const FVector StationUp(0.0, FMath::Sin(Tilt), FMath::Cos(Tilt));

	const FTransform StationPort(
		FRotationMatrix::MakeFromXZ(StationForward, StationUp).ToQuat(), FVector(300.0, 0.0, 0.0));

	TestTrue(TEXT("the fixture really faces back at the ship"),
		StationPort.GetUnitAxis(EAxis::X).Equals(StationForward, 1e-6));
	TestTrue(TEXT("and really carries the twenty-degree tilt"),
		StationPort.GetUnitAxis(EAxis::Z).Equals(StationUp, 1e-6));

	const StarForge::Docking::FDockingApproach Approach =
		StarForge::Docking::ComputeDockingApproach(FTransform::Identity, StationPort, 0.0);

	TestTrue(TEXT("a deck rolled twenty degrees reads as twenty"),
		FMath::IsNearlyEqual(Approach.UpAngleRad, 20.0 * Deg, 1e-6));
	TestTrue(TEXT("and the forward axis is still dead-on"),
		FMath::IsNearlyZero(Approach.ForwardAngleRad, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingCanDockInsideTheWindowTest,
	"StarForge.Core.Docking.CanDockInsideTheWindow",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingCanDockInsideTheWindowTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("an approach inside every limit is not refused"),
		StarForge::Docking::CanDock(MakeGoodApproach(), MakeLimits())
			== StarForge::Docking::EDockingRefusal::None);

	// Exactly on a limit is inside it. The window is a closed interval, so a pilot who
	// has flown to precisely three metres is not told to try again from three metres.
	StarForge::Docking::FDockingApproach OnTheEdge = MakeGoodApproach();
	OnTheEdge.Distance = 3.0;
	OnTheEdge.SpeedMetresPerSec = 1.0;
	OnTheEdge.ForwardAngleRad = 10.0 * Deg;
	OnTheEdge.UpAngleRad = 10.0 * Deg;

	TestTrue(TEXT("an approach exactly on every limit is still inside"),
		StarForge::Docking::CanDock(OnTheEdge, MakeLimits())
			== StarForge::Docking::EDockingRefusal::None);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingRefusesEachConditionOutsideItTest,
	"StarForge.Core.Docking.RefusesEachConditionOutsideIt",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingRefusesEachConditionOutsideItTest::RunTest(const FString& Parameters)
{
	const StarForge::Docking::FDockingLimits Limits = MakeLimits();

	// One field spoiled at a time. A refusal that named the wrong condition would send a
	// pilot to fix a thing that was never wrong, and the readout would agree with it.
	StarForge::Docking::FDockingApproach TooFar = MakeGoodApproach();
	TooFar.Distance = 3.5;
	TestTrue(TEXT("too far reads as distance"),
		StarForge::Docking::CanDock(TooFar, Limits) == StarForge::Docking::EDockingRefusal::Distance);

	StarForge::Docking::FDockingApproach TooFast = MakeGoodApproach();
	TooFast.SpeedMetresPerSec = 2.7;
	TestTrue(TEXT("too fast reads as speed"),
		StarForge::Docking::CanDock(TooFast, Limits) == StarForge::Docking::EDockingRefusal::Speed);

	StarForge::Docking::FDockingApproach Skewed = MakeGoodApproach();
	Skewed.ForwardAngleRad = 15.0 * Deg;
	TestTrue(TEXT("nose off reads as forward"),
		StarForge::Docking::CanDock(Skewed, Limits) == StarForge::Docking::EDockingRefusal::Forward);

	StarForge::Docking::FDockingApproach Rolled = MakeGoodApproach();
	Rolled.UpAngleRad = 15.0 * Deg;
	TestTrue(TEXT("deck rolled reads as up"),
		StarForge::Docking::CanDock(Rolled, Limits) == StarForge::Docking::EDockingRefusal::Up);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingRefusalOrderIsTheListedOrderTest,
	"StarForge.Core.Docking.RefusalOrderIsTheListedOrder",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingRefusalOrderIsTheListedOrderTest::RunTest(const FString& Parameters)
{
	const StarForge::Docking::FDockingLimits Limits = MakeLimits();

	// With two conditions broken the answer is the FIRST of them in the listed order,
	// and the order is deliberate: distance, speed, forward, up. It is the order in
	// which a pilot can act on the answer. Told "your nose is fifteen degrees off" at
	// forty metres, he corrects an alignment that will be wrong again by the time he
	// arrives; told "too far", he closes, and the alignment is then a real question.
	StarForge::Docking::FDockingApproach FarAndFast = MakeGoodApproach();
	FarAndFast.Distance = 3.5;
	FarAndFast.SpeedMetresPerSec = 2.7;
	TestTrue(TEXT("far and fast reads as distance"),
		StarForge::Docking::CanDock(FarAndFast, Limits) == StarForge::Docking::EDockingRefusal::Distance);

	StarForge::Docking::FDockingApproach FastAndSkewed = MakeGoodApproach();
	FastAndSkewed.SpeedMetresPerSec = 2.7;
	FastAndSkewed.ForwardAngleRad = 15.0 * Deg;
	TestTrue(TEXT("fast and skewed reads as speed"),
		StarForge::Docking::CanDock(FastAndSkewed, Limits) == StarForge::Docking::EDockingRefusal::Speed);

	StarForge::Docking::FDockingApproach SkewedAndRolled = MakeGoodApproach();
	SkewedAndRolled.ForwardAngleRad = 15.0 * Deg;
	SkewedAndRolled.UpAngleRad = 15.0 * Deg;
	TestTrue(TEXT("skewed and rolled reads as forward"),
		StarForge::Docking::CanDock(SkewedAndRolled, Limits) == StarForge::Docking::EDockingRefusal::Forward);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingTargetHullPoseMatesThePortsTest,
	"StarForge.Core.Docking.TargetHullPoseMatesThePorts",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingTargetHullPoseMatesThePortsTest::RunTest(const FString& Parameters)
{
	// The rig's real geometry, in the rig's centimetres. The station's port is 600 aft
	// along its own -Y and 265 below its pivot, facing out along -Y; the ship's port is
	// 400 forward along its own +Y at the same height, facing out along +Y. Neither port
	// sits on its hull's pivot, and that offset is precisely what a wrong multiplication
	// order gets wrong while still looking plausible.
	const FTransform StationPortWorld(
		FRotator(0.0, -90.0, 0.0).Quaternion(), FVector(0.0, -600.0, -265.0));
	const FTransform ShipPortLocalToHull(
		FRotator(0.0, 90.0, 0.0).Quaternion(), FVector(0.0, 400.0, -265.0));

	const FTransform HullWorld =
		StarForge::Docking::TargetHullPose(StationPortWorld, ShipPortLocalToHull);

	// Where the rig's hull is expected to end up. This constant lives here and nowhere
	// else: the component derives the pose from the formula, and a hard-coded pose in
	// the game module would stop tracking a wall that moved.
	TestTrue(TEXT("the hull lands a thousand centimetres aft of the station's pivot"),
		HullWorld.GetLocation().Equals(FVector(0.0, -1000.0, 0.0), 1e-6));
	TestTrue(TEXT("and unrotated with respect to it"),
		HullWorld.GetRotation().Rotator().IsNearlyZero(1e-4f));

	// The round trip: put the port back on that hull and the two ports must mate.
	const FTransform ShipPortWorld = ShipPortLocalToHull * HullWorld;
	const StarForge::Docking::FDockingApproach Mated =
		StarForge::Docking::ComputeDockingApproach(ShipPortWorld, StationPortWorld, 0.0);

	TestTrue(TEXT("the port origins coincide"), Mated.Distance < 1e-6);
	TestTrue(TEXT("the forward axes oppose"), Mated.ForwardAngleRad < 1e-6);
	TestTrue(TEXT("and the decks agree"), Mated.UpAngleRad < 1e-6);

	// Again with a station standing somewhere awkward, so nothing above can be passing
	// because the fixture happened to sit on the world axes.
	const FTransform TiltedStationPort(
		FRotationMatrix::MakeFromXZ(
			FVector(0.3, -0.5, 0.2).GetSafeNormal(), FVector(0.1, 0.4, 0.9)).ToQuat(),
		FVector(1234.0, -567.0, 890.0));

	const FTransform TiltedHull =
		StarForge::Docking::TargetHullPose(TiltedStationPort, ShipPortLocalToHull);
	const StarForge::Docking::FDockingApproach TiltedMated =
		StarForge::Docking::ComputeDockingApproach(
			ShipPortLocalToHull * TiltedHull, TiltedStationPort, 0.0);

	TestTrue(TEXT("origins coincide off the world axes too"), TiltedMated.Distance < 1e-6);
	TestTrue(TEXT("forward axes oppose off the world axes too"), TiltedMated.ForwardAngleRad < 1e-6);
	TestTrue(TEXT("decks agree off the world axes too"), TiltedMated.UpAngleRad < 1e-6);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingCapturePoseIsExactAtItsEndsTest,
	"StarForge.Core.Docking.CapturePoseIsExactAtItsEnds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingCapturePoseIsExactAtItsEndsTest::RunTest(const FString& Parameters)
{
	const FTransform Start(FRotator(0.0, 20.0, 0.0).Quaternion(), FVector(0.0, 0.0, 0.0));
	const FTransform Target(FRotator(0.0, 40.0, 0.0).Quaternion(), FVector(200.0, 0.0, 0.0));

	// Exact, not nearly. The last frame of a capture is the pose the attachment freezes
	// forever, and a hull left a millimetre short is a hull whose port never quite mates
	// -- with nothing to correct it afterwards, because attaching is the end of the
	// motion. Interpolating to alpha 1.0 does not give that back in floating point, so
	// the ends are returned rather than computed.
	TestTrue(TEXT("alpha zero is exactly the start"),
		StarForge::Docking::CapturePose(Start, Target, 0.0).Equals(Start, 0.0));
	TestTrue(TEXT("alpha one is exactly the target"),
		StarForge::Docking::CapturePose(Start, Target, 1.0).Equals(Target, 0.0));

	// And out of range clamps rather than extrapolating: a frame long enough to overshoot
	// the capture must not fly the hull past the station.
	TestTrue(TEXT("past one is still exactly the target"),
		StarForge::Docking::CapturePose(Start, Target, 1.7).Equals(Target, 0.0));
	TestTrue(TEXT("before zero is still exactly the start"),
		StarForge::Docking::CapturePose(Start, Target, -0.4).Equals(Start, 0.0));

	const FTransform Half = StarForge::Docking::CapturePose(Start, Target, 0.5);
	TestTrue(TEXT("half way is half way in position"),
		Half.GetLocation().Equals(FVector(100.0, 0.0, 0.0), 1e-6));
	TestTrue(TEXT("and half way in rotation"),
		FMath::IsNearlyEqual(Half.GetRotation().Rotator().Yaw, 30.0, 1e-3));

	return true;
}
