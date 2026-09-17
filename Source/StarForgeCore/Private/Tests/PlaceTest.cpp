#include "Misc/AutomationTest.h"
#include "Place.h"

#include <limits>

namespace
{
	/**
	 * The stage's ceilings, spelled out rather than taken from FPlaceLimits' defaults.
	 *
	 * A test that reads the same defaults the code reads asserts that the code agrees with
	 * itself. These three numbers are the spec's: 2^42 cm measured in the world-size spike
	 * (spec 4), and 90 s at 2.998e8 m/s from spec 5.
	 */
	constexpr double EngineRadiusMetres = 4.398046511104e10;   // 2^42 cm / 100
	constexpr double SlowestHullSpeed = 2.998e8;
	constexpr double CrossingBudget = 90.0;

	/** 2.998e8 * 90 = 2.6982e10, and both factors are exact in a double, so this is exact. */
	constexpr double CrossingCeilingMetres = 2.6982e10;

	StarForge::Place::FPlaceLimits MakeLimits()
	{
		StarForge::Place::FPlaceLimits Limits;
		Limits.EngineRadiusMetres = EngineRadiusMetres;
		Limits.SlowestHullSpeedMetresPerSec = SlowestHullSpeed;
		Limits.CrossingBudgetSeconds = CrossingBudget;
		return Limits;
	}

	/** The rig's place: 18 km of radius, which is spec 6's "36 km in 90 s at 400 m/s". */
	constexpr double RigRadiusMetres = 18000.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaceAtTheOriginFitsWithBothMarginsReportedTest,
	"StarForge.Core.Place.AtTheOriginFitsWithBothMarginsReported",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPlaceAtTheOriginFitsWithBothMarginsReportedTest::RunTest(const FString& Parameters)
{
	const StarForge::Place::FPlaceFit Fit =
		StarForge::Place::MeasurePlace(FVector::ZeroVector, RigRadiusMetres, MakeLimits());

	TestTrue(TEXT("the rig's own place fits"),
		Fit.Refusal == StarForge::Place::EPlaceRefusal::None);

	// Every number the assembly line prints, asserted here, because the line is printed on
	// EVERY place and a wrong figure in it would read as a healthy instrument.
	TestTrue(TEXT("reach is the radius when the place sits on the origin"),
		FMath::IsNearlyEqual(Fit.ReachMetres, RigRadiusMetres, 1e-6));
	TestTrue(TEXT("the engine ceiling is reported as given"),
		FMath::IsNearlyEqual(Fit.EngineCeilingMetres, EngineRadiusMetres, 1e-6));
	TestTrue(TEXT("the engine margin is the ceiling less the reach"),
		FMath::IsNearlyEqual(Fit.EngineMarginMetres, EngineRadiusMetres - RigRadiusMetres, 1e-6));
	TestTrue(TEXT("diameter is twice the radius"),
		FMath::IsNearlyEqual(Fit.DiameterMetres, 2.0 * RigRadiusMetres, 1e-6));
	TestTrue(TEXT("the crossing ceiling is speed times budget"),
		FMath::IsNearlyEqual(Fit.CrossingCeilingMetres, CrossingCeilingMetres, 1e-3));
	TestTrue(TEXT("the crossing margin is that ceiling less the diameter"),
		FMath::IsNearlyEqual(Fit.CrossingMarginMetres, CrossingCeilingMetres - 2.0 * RigRadiusMetres, 1e-3));

	// 36 000 / 2.998e8 = 1.2008e-4 s. The place the rig builds is crossed in ordinary space
	// in 90 s and at the slowest hull's superlight speed in a tenth of a millisecond.
	TestTrue(TEXT("the crossing time is the diameter over the speed"),
		FMath::IsNearlyEqual(Fit.CrossingSeconds, 2.0 * RigRadiusMetres / SlowestHullSpeed, 1e-12));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaceReachIsTheOriginPlusTheRadiusTest,
	"StarForge.Core.Place.ReachIsTheOriginPlusTheRadius",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPlaceReachIsTheOriginPlusTheRadiusTest::RunTest(const FString& Parameters)
{
	const StarForge::Place::FPlaceLimits Limits = MakeLimits();

	// The two points the world-size spike actually measured, with the rig's radius on them.
	// 4.30e10 m flew: 200.0 m/s, 0.000 cm of drift. 4.50e10 m did not: the ship vanished the
	// frame its physics body came on, with an empty log.
	const StarForge::Place::FPlaceFit Near =
		StarForge::Place::MeasurePlace(FVector(4.30e10, 0.0, 0.0), RigRadiusMetres, Limits);
	TestTrue(TEXT("the measured working distance fits"),
		Near.Refusal == StarForge::Place::EPlaceRefusal::None);
	TestTrue(TEXT("its reach carries the radius"),
		FMath::IsNearlyEqual(Near.ReachMetres, 4.30e10 + RigRadiusMetres, 1.0));

	const StarForge::Place::FPlaceFit Far =
		StarForge::Place::MeasurePlace(FVector(4.50e10, 0.0, 0.0), RigRadiusMetres, Limits);
	TestTrue(TEXT("the measured vanishing distance is refused"),
		Far.Refusal == StarForge::Place::EPlaceRefusal::EngineReach);
	TestTrue(TEXT("and the margin is negative by how far it overshoots"),
		Far.EngineMarginMetres < 0.0);

	// The radius is what makes this a REACH rather than a distance: a place whose centre is
	// inside and whose edge is outside must be refused. One metre of radius decides it, and a
	// double holds a metre at 4.4e10 with four digits to spare.
	const StarForge::Place::FPlaceFit Edge =
		StarForge::Place::MeasurePlace(FVector(EngineRadiusMetres - 1000.0, 0.0, 0.0), 1000.0, Limits);
	TestTrue(TEXT("an edge exactly on the ceiling fits"),
		Edge.Refusal == StarForge::Place::EPlaceRefusal::None);

	const StarForge::Place::FPlaceFit JustOver =
		StarForge::Place::MeasurePlace(FVector(EngineRadiusMetres - 1000.0, 0.0, 0.0), 1001.0, Limits);
	TestTrue(TEXT("one metre of extra radius is refused"),
		JustOver.Refusal == StarForge::Place::EPlaceRefusal::EngineReach);

	// The reach is a length, not a coordinate: a place out along -Y is exactly as far away.
	const StarForge::Place::FPlaceFit Negative =
		StarForge::Place::MeasurePlace(FVector(0.0, -4.50e10, 0.0), RigRadiusMetres, Limits);
	TestTrue(TEXT("distance has no sign"),
		Negative.Refusal == StarForge::Place::EPlaceRefusal::EngineReach);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaceACrossingLongerThanTheBudgetIsRefusedTest,
	"StarForge.Core.Place.ACrossingLongerThanTheBudgetIsRefused",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPlaceACrossingLongerThanTheBudgetIsRefusedTest::RunTest(const FString& Parameters)
{
	const StarForge::Place::FPlaceLimits Limits = MakeLimits();

	// Exactly half the crossing ceiling: the diameter is the ceiling to the last bit, because
	// 2.998e8 and 90 are both integers well inside a double's exact range.
	const StarForge::Place::FPlaceFit Exact =
		StarForge::Place::MeasurePlace(FVector::ZeroVector, CrossingCeilingMetres * 0.5, Limits);
	TestTrue(TEXT("a place crossed in exactly the budget fits"),
		Exact.Refusal == StarForge::Place::EPlaceRefusal::None);
	TestTrue(TEXT("and it takes exactly the budget"),
		FMath::IsNearlyEqual(Exact.CrossingSeconds, CrossingBudget, 1e-9));

	const StarForge::Place::FPlaceFit Over =
		StarForge::Place::MeasurePlace(FVector::ZeroVector, CrossingCeilingMetres * 0.5 + 1.0e6, Limits);
	TestTrue(TEXT("a place a thousand kilometres larger is refused"),
		Over.Refusal == StarForge::Place::EPlaceRefusal::CrossingTime);
	TestTrue(TEXT("and it is refused for taking longer than the budget"),
		Over.CrossingSeconds > CrossingBudget);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaceTheTighterCeilingIsTheOneThatBindsTest,
	"StarForge.Core.Place.TheTighterCeilingIsTheOneThatBinds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPlaceTheTighterCeilingIsTheOneThatBindsTest::RunTest(const FString& Parameters)
{
	const StarForge::Place::FPlaceLimits Limits = MakeLimits();

	// The control the rig performs by hand: 2.0e10 m of radius on the origin. Its reach is
	// 2.0e10, well inside the engine's 4.398e10, so the engine ceiling PASSES; its diameter is
	// 4.0e10 against the crossing ceiling's 2.6982e10, so the crossing one does not. Without
	// this case the time ceiling is code that looks alive and has never decided anything.
	const StarForge::Place::FPlaceFit TimeBound =
		StarForge::Place::MeasurePlace(FVector::ZeroVector, 2.0e10, Limits);
	TestTrue(TEXT("time is what refuses it"),
		TimeBound.Refusal == StarForge::Place::EPlaceRefusal::CrossingTime);
	TestTrue(TEXT("the engine margin is positive while the crossing one is not"),
		TimeBound.EngineMarginMetres > 0.0 && TimeBound.CrossingMarginMetres < 0.0);

	// The other way round: a small place far out clears the crossing ceiling and fails the
	// engine's. Under spec 10.4's one-line phrasing -- the sum against the smaller ceiling --
	// this place would be refused for a crossing that takes seven microseconds, and the
	// refusal would send whoever reads it to shrink a place that is already tiny.
	const StarForge::Place::FPlaceFit ReachBound =
		StarForge::Place::MeasurePlace(FVector(4.50e10, 0.0, 0.0), 1000.0, Limits);
	TestTrue(TEXT("the engine is what refuses a small place too far out"),
		ReachBound.Refusal == StarForge::Place::EPlaceRefusal::EngineReach);
	TestTrue(TEXT("its crossing margin is comfortably positive"),
		ReachBound.CrossingMarginMetres > 0.0);

	// Failing both reports the engine, because moving a place is what makes shrinking it worth
	// discussing. Same reasoning as CanDock's order: the refusal a person can act on first.
	const StarForge::Place::FPlaceFit Both =
		StarForge::Place::MeasurePlace(FVector(4.50e10, 0.0, 0.0), 2.0e10, Limits);
	TestTrue(TEXT("a place failing both is told about the engine"),
		Both.Refusal == StarForge::Place::EPlaceRefusal::EngineReach);
	TestTrue(TEXT("and both margins are reported negative anyway"),
		Both.EngineMarginMetres < 0.0 && Both.CrossingMarginMetres < 0.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaceANonFinitePlaceIsRefusedAsNotFiniteTest,
	"StarForge.Core.Place.ANonFinitePlaceIsRefusedAsNotFinite",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPlaceANonFinitePlaceIsRefusedAsNotFiniteTest::RunTest(const FString& Parameters)
{
	const StarForge::Place::FPlaceLimits Limits = MakeLimits();
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const double Inf = std::numeric_limits<double>::infinity();

	// The reason matters more than the refusal. Every comparison with a NaN is false, so a
	// predicate without this guard would fall through the engine test and answer EngineReach --
	// a confident, specific, wrong diagnosis of data that is simply broken.
	TestTrue(TEXT("a NaN radius is not a place that is too big"),
		StarForge::Place::MeasurePlace(FVector::ZeroVector, NaN, Limits).Refusal
			== StarForge::Place::EPlaceRefusal::NotFinite);
	TestTrue(TEXT("an infinite radius likewise"),
		StarForge::Place::MeasurePlace(FVector::ZeroVector, Inf, Limits).Refusal
			== StarForge::Place::EPlaceRefusal::NotFinite);
	TestTrue(TEXT("a NaN in the origin likewise"),
		StarForge::Place::MeasurePlace(FVector(NaN, 0.0, 0.0), 1000.0, Limits).Refusal
			== StarForge::Place::EPlaceRefusal::NotFinite);
	TestTrue(TEXT("and an infinity in the origin"),
		StarForge::Place::MeasurePlace(FVector(0.0, 0.0, Inf), 1000.0, Limits).Refusal
			== StarForge::Place::EPlaceRefusal::NotFinite);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaceARadiusOfZeroIsNotAPlaceTest,
	"StarForge.Core.Place.ARadiusOfZeroIsNotAPlace",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPlaceARadiusOfZeroIsNotAPlaceTest::RunTest(const FString& Parameters)
{
	const StarForge::Place::FPlaceLimits Limits = MakeLimits();

	// Spec 5: a place is a continuous VOLUME. A radius of zero passes both ceilings with the
	// largest margins in the file and describes nothing anyone can fly in, so it would be the
	// one authoring mistake this predicate blesses.
	TestTrue(TEXT("zero radius is refused"),
		StarForge::Place::MeasurePlace(FVector::ZeroVector, 0.0, Limits).Refusal
			== StarForge::Place::EPlaceRefusal::RadiusNotPositive);
	TestTrue(TEXT("a negative radius is refused as the same thing"),
		StarForge::Place::MeasurePlace(FVector::ZeroVector, -1000.0, Limits).Refusal
			== StarForge::Place::EPlaceRefusal::RadiusNotPositive);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaceLimitsThatCannotBeUsedAreRefusedTest,
	"StarForge.Core.Place.LimitsThatCannotBeUsedAreRefused",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPlaceLimitsThatCannotBeUsedAreRefusedTest::RunTest(const FString& Parameters)
{
	const double Inf = std::numeric_limits<double>::infinity();

	// A speed of zero divides into the crossing time and answers infinity, which compares
	// larger than the budget -- so the place would be refused for "crossing time" when the
	// truth is that nobody said how fast the slowest hull goes.
	StarForge::Place::FPlaceLimits NoSpeed = MakeLimits();
	NoSpeed.SlowestHullSpeedMetresPerSec = 0.0;
	TestTrue(TEXT("a zero hull speed refuses the limits, not the place"),
		StarForge::Place::MeasurePlace(FVector::ZeroVector, 1000.0, NoSpeed).Refusal
			== StarForge::Place::EPlaceRefusal::LimitsNotUsable);

	StarForge::Place::FPlaceLimits NoBudget = MakeLimits();
	NoBudget.CrossingBudgetSeconds = 0.0;
	TestTrue(TEXT("a zero crossing budget likewise"),
		StarForge::Place::MeasurePlace(FVector::ZeroVector, 1000.0, NoBudget).Refusal
			== StarForge::Place::EPlaceRefusal::LimitsNotUsable);

	StarForge::Place::FPlaceLimits NoEngine = MakeLimits();
	NoEngine.EngineRadiusMetres = Inf;
	TestTrue(TEXT("an infinite engine ceiling is not a ceiling"),
		StarForge::Place::MeasurePlace(FVector::ZeroVector, 1000.0, NoEngine).Refusal
			== StarForge::Place::EPlaceRefusal::LimitsNotUsable);

	// Checked BEFORE the place, so a broken world does not get blamed on the place inside it.
	StarForge::Place::FPlaceLimits BrokenBoth = NoSpeed;
	TestTrue(TEXT("broken limits outrank a broken place"),
		StarForge::Place::MeasurePlace(FVector::ZeroVector, -1.0, BrokenBoth).Refusal
			== StarForge::Place::EPlaceRefusal::LimitsNotUsable);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaceTheMeasurementSurvivesARefusalTest,
	"StarForge.Core.Place.TheMeasurementSurvivesARefusal",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPlaceTheMeasurementSurvivesARefusalTest::RunTest(const FString& Parameters)
{
	// Spec 10.4 asks the refusal to carry "the reason and both numbers". The assembly prints
	// one line per place whether it fits or not, so a refused place has to arrive with every
	// figure filled in; otherwise the loudest line in the project would be the emptiest.
	const StarForge::Place::FPlaceFit Refused =
		StarForge::Place::MeasurePlace(FVector(4.50e10, 0.0, 0.0), RigRadiusMetres, MakeLimits());

	TestTrue(TEXT("a refused place still reports its reach"),
		FMath::IsNearlyEqual(Refused.ReachMetres, 4.50e10 + RigRadiusMetres, 1.0));
	TestTrue(TEXT("and both ceilings"),
		FMath::IsNearlyEqual(Refused.EngineCeilingMetres, EngineRadiusMetres, 1e-6)
			&& FMath::IsNearlyEqual(Refused.CrossingCeilingMetres, CrossingCeilingMetres, 1e-3));
	TestTrue(TEXT("and its diameter and crossing time"),
		FMath::IsNearlyEqual(Refused.DiameterMetres, 2.0 * RigRadiusMetres, 1e-6)
			&& Refused.CrossingSeconds > 0.0);

	// The names are what the log line prints, so they are asserted rather than trusted.
	TestEqual(TEXT("None"), FString(StarForge::Place::RefusalName(
		StarForge::Place::EPlaceRefusal::None)), FString(TEXT("fits")));
	TestEqual(TEXT("LimitsNotUsable"), FString(StarForge::Place::RefusalName(
		StarForge::Place::EPlaceRefusal::LimitsNotUsable)), FString(TEXT("limits not usable")));
	TestEqual(TEXT("NotFinite"), FString(StarForge::Place::RefusalName(
		StarForge::Place::EPlaceRefusal::NotFinite)), FString(TEXT("not a finite place")));
	TestEqual(TEXT("RadiusNotPositive"), FString(StarForge::Place::RefusalName(
		StarForge::Place::EPlaceRefusal::RadiusNotPositive)), FString(TEXT("radius not positive")));
	TestEqual(TEXT("EngineReach"), FString(StarForge::Place::RefusalName(
		StarForge::Place::EPlaceRefusal::EngineReach)), FString(TEXT("engine reach")));
	TestEqual(TEXT("CrossingTime"), FString(StarForge::Place::RefusalName(
		StarForge::Place::EPlaceRefusal::CrossingTime)), FString(TEXT("crossing time")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlaceTheDefaultLimitsAreTheSpecsNumbersTest,
	"StarForge.Core.Place.TheDefaultLimitsAreTheSpecsNumbers",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPlaceTheDefaultLimitsAreTheSpecsNumbersTest::RunTest(const FString& Parameters)
{
	// Every other test in this file spells its own limits through MakeLimits(), and does so on
	// purpose: a test that reads the same defaults the code reads asserts only that the code
	// agrees with itself.
	//
	// That leaves the defaults asserted by nobody -- and the defaults are what the game uses.
	// Measured rather than argued: changing EngineRadiusMetres in the header from e10 to e12,
	// which is the centimetre ceiling written where the metre one belongs, failed not one of
	// the other eight tests. The border of the world would sit a hundred times too far out,
	// every place would fit, and hulls would go back to vanishing the frame they start
	// simulating, with a clean log -- the exact failure this whole stage exists to make
	// impossible.
	//
	// So the defaults get one test of their own, and it is the only place in the file where
	// reading them is correct.
	const StarForge::Place::FPlaceLimits Defaults;

	TestTrue(TEXT("the engine ceiling is 2^42 cm carried into metres"),
		FMath::IsNearlyEqual(Defaults.EngineRadiusMetres, EngineRadiusMetres, 1e-6));
	TestTrue(TEXT("the slowest hull's speed is the speed of light"),
		FMath::IsNearlyEqual(Defaults.SlowestHullSpeedMetresPerSec, SlowestHullSpeed, 1e-6));
	TestTrue(TEXT("the crossing budget is the owner's ninety seconds"),
		FMath::IsNearlyEqual(Defaults.CrossingBudgetSeconds, CrossingBudget, 1e-9));

	// Spelled again as a bare comparison, because IsNearlyEqual with a tolerance of 1e-6 on a
	// figure of 4.4e10 would pass a ceiling wrong by four kilometres. The factor-of-a-hundred
	// mistake this test exists for is caught either way; a smaller one would not be.
	TestEqual(TEXT("and the engine ceiling is that number exactly"),
		Defaults.EngineRadiusMetres, 4398046511104.0 / 100.0);

	return true;
}
