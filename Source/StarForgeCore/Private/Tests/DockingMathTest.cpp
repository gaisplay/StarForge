#include "Misc/AutomationTest.h"
#include "DockingMath.h"
#include "ShipFrameMath.h"

#include <limits>

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

	/**
	 * The loop rig's station, in metres, as the union of its physical panels' bounds with the
	 * station at the origin. X: the side walls are centred at +-600 cm and 20 cm thick, so
	 * +-610. Y: the far wall is centred at +400 and reaches +410; the tunnel runs from the -Y
	 * wall out to its mouth at -600, and its end faces lie in that plane, so -600 and not
	 * -610. Z: floor and ceiling are centred at +-400 and 20 cm thick, so +-410.
	 */
	FBox MakeRigStationBoundsMetres()
	{
		return FBox(FVector(-6.1, -6.0, -4.1), FVector(6.1, 4.1, 4.1));
	}

	/** The hull box's half-extent is 400 cm on every axis; its half-diagonal is 4 * sqrt(3) m. */
	double RigHullRadiusMetres()
	{
		return 4.0 * FMath::Sqrt(3.0);
	}

	/**
	 * Docked, the hull's centre is 1000 cm aft of the station's pivot along -Y (see
	 * TargetHullPoseMatesThePorts), so its +Y face at -6.0 m lies flush on the tunnel mouth.
	 */
	const FVector RigDockedHullCentreMetres(0.0, -10.0, 0.0);

	/** The station port's +X: yawed -90 degrees, it points along the station's -Y. */
	const FVector RigPortAwayAxis(0.0, -1.0, 0.0);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingNonFiniteApproachIsRefusedTest,
	"StarForge.Core.Docking.NonFiniteApproachIsRefused",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingNonFiniteApproachIsRefusedTest::RunTest(const FString& Parameters)
{
	// A function whose whole job is refusing must not answer "permitted" to a question it
	// could not evaluate. Written as `Value > Limit`, every guard in CanDock is false for a
	// NaN, all four fall through, and the answer is None -- an instrument reading plausibly
	// while dead, which is the exact shape of this project's two worst defects: the drift
	// clock that ran at double speed and the hull that reported full thrust while asleep.
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const double Inf = std::numeric_limits<double>::infinity();

	// The fixture before the behaviour. A NaN that a compiler flag had quietly turned into
	// a finite number would make every assertion below pass for the wrong reason, and the
	// test would then be one more instrument that reads well and measures nothing.
	TestTrue(TEXT("the NaN really is not finite"), !FMath::IsFinite(NaN));
	TestTrue(TEXT("and the infinity really is not finite"), !FMath::IsFinite(Inf));

	const StarForge::Docking::FDockingLimits Limits = MakeLimits();

	// One non-finite field at a time, each named by the guard that has to catch it. Both
	// paths into this are reachable: the speed is carried in from the caller's instrument
	// untouched, and a port location that is not finite makes FVector::Dist return NaN.
	StarForge::Docking::FDockingApproach NoDistance = MakeGoodApproach();
	NoDistance.Distance = NaN;
	TestTrue(TEXT("a NaN distance is refused, and named as distance"),
		StarForge::Docking::CanDock(NoDistance, Limits) == StarForge::Docking::EDockingRefusal::Distance);

	StarForge::Docking::FDockingApproach NoSpeed = MakeGoodApproach();
	NoSpeed.SpeedMetresPerSec = NaN;
	TestTrue(TEXT("a NaN speed is refused, and named as speed"),
		StarForge::Docking::CanDock(NoSpeed, Limits) == StarForge::Docking::EDockingRefusal::Speed);

	StarForge::Docking::FDockingApproach NoForward = MakeGoodApproach();
	NoForward.ForwardAngleRad = NaN;
	TestTrue(TEXT("a NaN forward angle is refused, and named as forward"),
		StarForge::Docking::CanDock(NoForward, Limits) == StarForge::Docking::EDockingRefusal::Forward);

	StarForge::Docking::FDockingApproach NoUp = MakeGoodApproach();
	NoUp.UpAngleRad = NaN;
	TestTrue(TEXT("a NaN up angle is refused, and named as up"),
		StarForge::Docking::CanDock(NoUp, Limits) == StarForge::Docking::EDockingRefusal::Up);

	// Infinity is the other half of "not finite" and refuses under either spelling of the
	// guard, so it is here to pin the whole class rather than the one member that bites.
	StarForge::Docking::FDockingApproach Infinite = MakeGoodApproach();
	Infinite.Distance = Inf;
	TestTrue(TEXT("an infinite distance is refused too"),
		StarForge::Docking::CanDock(Infinite, Limits) == StarForge::Docking::EDockingRefusal::Distance);

	// And the path the two angles take in, which no field above reaches: a port whose rotation
	// is not finite while its location is. The distance between the origins is then a sound
	// figure inside the window, and both axes are NaN. The clamp in front of Acos used to turn
	// their NaN dot product into 1.0, an angle of zero -- nose-on, decks parallel -- and
	// CanDock granted the docking on a pose nobody could read. Positions in metres, so the
	// approach goes to CanDock as it comes: the transforms carry whatever unit they are given.
	const FTransform GoodStationPort(FRotator(0.0, 180.0, 0.0).Quaternion(), FVector(2.0, 0.0, 0.0));
	const FQuat NaNRotation(NaN, NaN, NaN, NaN);
	const FQuat InfiniteRotation(Inf, 0.0, 0.0, 1.0);

	for (const FQuat& Broken : { NaNRotation, InfiniteRotation })
	{
		const StarForge::Docking::FDockingApproach FromTheShip =
			StarForge::Docking::ComputeDockingApproach(
				FTransform(Broken, FVector::ZeroVector), GoodStationPort, 0.3);
		TestTrue(TEXT("the fixture's distance is a sound figure inside the window"),
			FMath::IsNearlyEqual(FromTheShip.Distance, 2.0, 1e-12));
		TestFalse(TEXT("a ship port whose rotation is not finite has no forward angle"),
			FMath::IsFinite(FromTheShip.ForwardAngleRad));
		TestFalse(TEXT("and no up angle"), FMath::IsFinite(FromTheShip.UpAngleRad));
		TestTrue(TEXT("and the docking is refused on the first angle it cannot read"),
			StarForge::Docking::CanDock(FromTheShip, Limits) == StarForge::Docking::EDockingRefusal::Forward);

		const StarForge::Docking::FDockingApproach FromTheStation =
			StarForge::Docking::ComputeDockingApproach(
				FTransform::Identity, FTransform(Broken, FVector(2.0, 0.0, 0.0)), 0.3);
		TestFalse(TEXT("a station port whose rotation is not finite has no forward angle either"),
			FMath::IsFinite(FromTheStation.ForwardAngleRad));
		TestFalse(TEXT("nor an up angle"), FMath::IsFinite(FromTheStation.UpAngleRad));
		TestTrue(TEXT("and is refused the same way"),
			StarForge::Docking::CanDock(FromTheStation, Limits) == StarForge::Docking::EDockingRefusal::Forward);
	}

	// The same failure one function over: both of CapturePose's range guards are false for
	// a NaN alpha, so it would slerp the hull to a pose of NaNs -- and a capture ends in an
	// attachment that freezes whatever pose it was handed, with nothing to correct it.
	const FTransform Start(FRotator(0.0, 20.0, 0.0).Quaternion(), FVector::ZeroVector);
	const FTransform Target(FRotator(0.0, 40.0, 0.0).Quaternion(), FVector(200.0, 0.0, 0.0));
	TestTrue(TEXT("a NaN alpha holds the hull exactly at the start"),
		StarForge::Docking::CapturePose(Start, Target, NaN).Equals(Start, 0.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingReleaseStandoffOnTheRigTest,
	"StarForge.Core.Docking.ReleaseStandoffOnTheRig",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingReleaseStandoffOnTheRigTest::RunTest(const FString& Parameters)
{
	const FBox Station = MakeRigStationBoundsMetres();
	const double Radius = RigHullRadiusMetres();

	// The fixture before the behaviour: docked, the hull's sphere really does overlap the
	// station, by 4 * sqrt(3) - 4 metres, which is the case every undock starts from.
	TestTrue(TEXT("docked, the hull's sphere overlaps the station"),
		FMath::Sqrt(Station.ComputeSquaredDistanceToPoint(RigDockedHullCentreMetres)) < Radius);

	// Straight out along -Y the centre stays inside the box's X and Z ranges, so the gap is
	// along Y alone: the face at -6.0 against a centre at -10 - t, that is 4 + t. Clear when
	// 4 + t = 4 * sqrt(3), so t = 4 * sqrt(3) - 4 = 2.92820323 m.
	const double Standoff = StarForge::Docking::ReleaseStandoff(
		RigDockedHullCentreMetres, Radius, RigPortAwayAxis, Station);

	TestTrue(TEXT("the rig's standoff is 4 * sqrt(3) - 4 m"),
		FMath::IsNearlyEqual(Standoff, 4.0 * FMath::Sqrt(3.0) - 4.0, 1e-6));

	// Spec 8's re-docking relies on this: a released ship stands inside its own window, so
	// the same key that let it go can take it back without flying anywhere.
	TestTrue(TEXT("and it leaves the ship inside the 3.0 m docking window"),
		Standoff <= StarForge::Docking::FDockingLimits().DistanceMetres);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingReleaseStandoffIsZeroWhenClearTest,
	"StarForge.Core.Docking.ReleaseStandoffIsZeroWhenClear",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingReleaseStandoffIsZeroWhenClearTest::RunTest(const FString& Parameters)
{
	const FBox Station = MakeRigStationBoundsMetres();
	const double Radius = RigHullRadiusMetres();

	// Twenty metres aft the gap is 14 m, twice the radius, and moving further aft or
	// sideways never closes it. Exactly zero: nothing to ease out is no easing at all.
	const FVector FarAft(0.0, -20.0, 0.0);
	TestTrue(TEXT("clear and moving away is exactly zero"),
		StarForge::Docking::ReleaseStandoff(FarAft, Radius, RigPortAwayAxis, Station) == 0.0);
	TestTrue(TEXT("clear and moving along the station is exactly zero"),
		StarForge::Docking::ReleaseStandoff(FarAft, Radius, FVector(1.0, 0.0, 0.0), Station) == 0.0);

	// A station with no physical panel has an invalid box and nothing to be cleared of.
	TestTrue(TEXT("an empty station asks for no standoff"),
		StarForge::Docking::ReleaseStandoff(RigDockedHullCentreMetres, Radius, RigPortAwayAxis, FBox(ForceInit)) == 0.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingReleaseStandoffFollowsTheStationTest,
	"StarForge.Core.Docking.ReleaseStandoffFollowsTheStation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingReleaseStandoffFollowsTheStationTest::RunTest(const FString& Parameters)
{
	const FBox Station = MakeRigStationBoundsMetres();
	const double Radius = RigHullRadiusMetres();
	const double Base = StarForge::Docking::ReleaseStandoff(
		RigDockedHullCentreMetres, Radius, RigPortAwayAxis, Station);

	// A station one metre further along the away axis -- over the hull, one metre deeper
	// into it -- needs exactly one metre more; one metre back the other way, one less.
	TestTrue(TEXT("a station a metre deeper over the hull asks a metre more"),
		FMath::IsNearlyEqual(
			StarForge::Docking::ReleaseStandoff(
				RigDockedHullCentreMetres, Radius, RigPortAwayAxis, Station.ShiftBy(RigPortAwayAxis)),
			Base + 1.0, 1e-6));
	TestTrue(TEXT("a station a metre further off asks a metre less"),
		FMath::IsNearlyEqual(
			StarForge::Docking::ReleaseStandoff(
				RigDockedHullCentreMetres, Radius, RigPortAwayAxis, Station.ShiftBy(-RigPortAwayAxis)),
			Base - 1.0, 1e-6));

	// And the whole scene moved together changes nothing: the answer is a length between
	// the two bodies, not a position in the world.
	const FVector Anywhere(1234.5, -678.9, 42.0);
	TestTrue(TEXT("moving hull and station together changes nothing"),
		FMath::IsNearlyEqual(
			StarForge::Docking::ReleaseStandoff(
				RigDockedHullCentreMetres + Anywhere, Radius, RigPortAwayAxis, Station.ShiftBy(Anywhere)),
			Base, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingReleaseStandoffNormalisesTheDirectionTest,
	"StarForge.Core.Docking.ReleaseStandoffNormalisesTheDirection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingReleaseStandoffNormalisesTheDirectionTest::RunTest(const FString& Parameters)
{
	const FBox Station = MakeRigStationBoundsMetres();
	const double Radius = RigHullRadiusMetres();

	// The answer is a distance along the direction, so the direction's length must not
	// scale it: a direction of length five taken as given would come back a fifth as long.
	TestTrue(TEXT("a direction five long gives the unit direction's answer"),
		FMath::IsNearlyEqual(
			StarForge::Docking::ReleaseStandoff(RigDockedHullCentreMetres, Radius, FVector(0.0, -5.0, 0.0), Station),
			StarForge::Docking::ReleaseStandoff(RigDockedHullCentreMetres, Radius, RigPortAwayAxis, Station),
			1e-9));

	TestTrue(TEXT("no direction at all is no standoff"),
		StarForge::Docking::ReleaseStandoff(RigDockedHullCentreMetres, Radius, FVector::ZeroVector, Station) == 0.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingReleaseStandoffSidewaysClearsTheEdgeTest,
	"StarForge.Core.Docking.ReleaseStandoffSidewaysClearsTheEdge",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingReleaseStandoffSidewaysClearsTheEdgeTest::RunTest(const FString& Parameters)
{
	// F3's sideways undock, asked of the maths: the hull slides along +X across the face
	// it was docked flush against. Along the whole face the gap is the 4 m along Y alone,
	// under the radius; past the edge at x = 6.1 it becomes sqrt((t - 6.1)^2 + 4^2). Clear
	// when that equals 4 * sqrt(3): (t - 6.1)^2 = 48 - 16 = 32, so t = 6.1 + 4 * sqrt(2)
	// = 11.75685425 m -- four times the straight-out answer, which is why the release goes
	// along the port's axis and nowhere else.
	const double Standoff = StarForge::Docking::ReleaseStandoff(
		RigDockedHullCentreMetres, RigHullRadiusMetres(), FVector(1.0, 0.0, 0.0), MakeRigStationBoundsMetres());

	TestTrue(TEXT("sliding along the face clears at 6.1 + 4 * sqrt(2) m"),
		FMath::IsNearlyEqual(Standoff, 6.1 + 4.0 * FMath::Sqrt(2.0), 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingReleaseStandoffThroughTheStationIsTheFarSideTest,
	"StarForge.Core.Docking.ReleaseStandoffThroughTheStationIsTheFarSide",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingReleaseStandoffThroughTheStationIsTheFarSideTest::RunTest(const FString& Parameters)
{
	// Clear where it starts is not clear along the way. Twenty metres aft and heading along
	// +Y, the hull is 14 m off now and passes straight through the station; it is clear for
	// good only once its centre is 4 * sqrt(3) past the far wall at +4.1, at y = 4.1 +
	// 4 * sqrt(3), which is t = 24.1 + 4 * sqrt(3) = 31.02820323 m. An answer of zero here
	// is a function that looked at t = 0 and stopped.
	const FVector FarAft(0.0, -20.0, 0.0);
	const double Standoff = StarForge::Docking::ReleaseStandoff(
		FarAft, RigHullRadiusMetres(), FVector(0.0, 1.0, 0.0), MakeRigStationBoundsMetres());

	TestTrue(TEXT("a ray through the station clears on its far side"),
		FMath::IsNearlyEqual(Standoff, 24.1 + 4.0 * FMath::Sqrt(3.0), 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingReleaseStandoffIsTheSameAtAnyStationPoseTest,
	"StarForge.Core.Docking.ReleaseStandoffIsTheSameAtAnyStationPose",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingReleaseStandoffIsTheSameAtAnyStationPoseTest::RunTest(const FString& Parameters)
{
	const FBox StationLocal = MakeRigStationBoundsMetres();
	const double Radius = RigHullRadiusMetres();
	const double Expected = 4.0 * FMath::Sqrt(3.0) - 4.0;
	const FVector Anywhere(1234.5, -678.9, 42.0);

	// The fixture before the behaviour: a world-aligned box round the rig's station yawed
	// three degrees is the wrong question, and it gives the wrong answer. Its -Y face is at
	// -(6.0 cos + 6.1 sin) and the hull leaves along (sin, -cos), so the gap is
	// (10 + t) cos - 6.0 cos - 6.1 sin, and it reaches 4 * sqrt(3) at
	// t = 4 * sqrt(3) / cos + 6.1 tan - 4 = 3.2573986 m, over the 3.0 m window.
	{
		const FTransform Yawed(FRotator(0.0, 3.0, 0.0), Anywhere);
		const double WorldAligned = StarForge::Docking::ReleaseStandoff(
			Yawed.TransformPositionNoScale(RigDockedHullCentreMetres), Radius,
			Yawed.TransformVectorNoScale(RigPortAwayAxis), StationLocal.TransformBy(Yawed));
		const double Cos3 = FMath::Cos(3.0 * Deg);
		TestTrue(TEXT("a world-aligned box round a yawed station asks too much"),
			FMath::IsNearlyEqual(WorldAligned, 4.0 * FMath::Sqrt(3.0) / Cos3 + 6.1 * FMath::Tan(3.0 * Deg) - 4.0, 1e-6));
		TestTrue(TEXT("enough to put the released ship outside its window"),
			WorldAligned > StarForge::Docking::FDockingLimits().DistanceMetres);
	}

	// Measured in the station's frame, the pose is irrelevant: the hull and the station move
	// together, and every pose gives the unrotated rig's 4 * sqrt(3) - 4. The last pose is
	// scaled, and the scale must be ignored, because the box is already in the hull's lengths.
	const FTransform Poses[] = {
		FTransform(FRotator(0.0, 3.0, 0.0), Anywhere),
		FTransform(FRotator(0.0, 10.0, 0.0), Anywhere),
		FTransform(FRotator(0.0, 20.0, 0.0), Anywhere),
		FTransform(FRotator(0.0, 45.0, 0.0), Anywhere),
		FTransform(FRotator(0.0, 90.0, 0.0), Anywhere),
		FTransform(FRotator(17.0, -130.0, 29.0), Anywhere),
		FTransform(FRotator(17.0, -130.0, 29.0).Quaternion(), Anywhere, FVector(2.0)),
	};
	for (const FTransform& Pose : Poses)
	{
		const double Standoff = StarForge::Docking::ReleaseStandoff(
			Pose.TransformPositionNoScale(RigDockedHullCentreMetres), Radius,
			Pose.TransformVectorNoScale(RigPortAwayAxis), Pose, StationLocal);
		TestTrue(FString::Printf(TEXT("the station turned %s, scaled %.1f, asks the rig's standoff"),
				*Pose.Rotator().ToString(), Pose.GetScale3D().X),
			FMath::IsNearlyEqual(Standoff, Expected, 1e-6));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingSmoothstepPeakAccelerationTest,
	"StarForge.Core.Docking.SmoothstepPeakAcceleration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingSmoothstepPeakAccelerationTest::RunTest(const FString& Parameters)
{
	// 6d/T^2. Three metres in one second is 18; in two seconds a quarter of that, which is
	// the case that tells T^2 from T -- at one second the two are the same number.
	TestTrue(TEXT("three metres in a second peaks at 18 m/s2"),
		StarForge::Docking::SmoothstepPeakAcceleration(3.0, 1.0) == 18.0);
	TestTrue(TEXT("three metres in two seconds peaks at 4.5 m/s2"),
		StarForge::Docking::SmoothstepPeakAcceleration(3.0, 2.0) == 4.5);

	// The rig's release, 2.9282 m over the one-second capture time: 17.57 m/s2, under the
	// grav-plate ceiling, so a man standing aboard stays on his feet while the ship eases out.
	TestTrue(TEXT("the rig's release stays under the grav-plate limit"),
		StarForge::Docking::SmoothstepPeakAcceleration(2.9282, 1.0)
			< StarForge::ShipFrame::GravPlateLimitMetersPerSecSq);

	// A path with no duration is covered in one frame -- the docking component treats a
	// non-positive CaptureSeconds as instant -- so its acceleration is unbounded, and it
	// must read as over any limit rather than as zero, or the grav-plate warning written for
	// exactly that case stays silent.
	TestTrue(TEXT("three metres in no time is unbounded"),
		StarForge::Docking::SmoothstepPeakAcceleration(3.0, 0.0) == TNumericLimits<double>::Max());
	TestTrue(TEXT("and over the grav-plate limit"),
		StarForge::Docking::SmoothstepPeakAcceleration(3.0, -1.0)
			>= StarForge::ShipFrame::GravPlateLimitMetersPerSecSq);
	TestTrue(TEXT("no distance is no acceleration, whatever the duration"),
		StarForge::Docking::SmoothstepPeakAcceleration(0.0, 0.0) == 0.0);

	return true;
}
