#include "Misc/AutomationTest.h"
#include "DockingMath.h"
#include "ShipFrameMath.h"

#include <cmath>
#include <limits>

namespace
{
namespace StationKitTests
{
	/**
	 * A station whose -Y face lies PastMetres past a port plane at Y = 0 facing -Y, with the
	 * station behind the plane: X +-30, Y -PastMetres..36, Z +-16. The berth's own proportions.
	 */
	FBox FaceOnThePlane(double PastMetres)
	{
		return FBox(FVector(-30.0, -PastMetres, -16.0), FVector(30.0, 36.0, 16.0));
	}

	/** The berth's physical box in its own frame, metres (station-kit spec 6.8), and Port0 on it. */
	FBox BerthBoxMetres()
	{
		return FBox(FVector(-18.38, -18.10, -16.0), FVector(41.62, 17.90, 16.0));
	}
	const FVector BerthPort0Metres(0.0, -18.10, -2.75);

	/** A kit port's +X: yawed -90 degrees, it points along the station's -Y. */
	const FVector AwayAxis(0.0, -1.0, 0.0);

	constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
	constexpr double Inf = std::numeric_limits<double>::infinity();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingBoundsPastPortPlaneIsZeroWhenFlushTest,
	"StarForge.Core.Docking.BoundsPastPortPlaneIsZeroWhenFlush",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingBoundsPastPortPlaneIsZeroWhenFlushTest::RunTest(const FString& Parameters)
{
	using namespace StationKitTests;

	// Rule 1 held exactly: the face lies on the plane and the station is behind it. The support along
	// -Y is the face itself, -18 + 18 in the arithmetic. With the axis taken the wrong way round the
	// same box would read its whole depth, 36 m, and every station would be red.
	TestTrue(TEXT("a face on the plane reaches nothing past it"),
		FMath::IsNearlyEqual(StarForge::Docking::BoundsPastPortPlane(FaceOnThePlane(0.0), FVector::ZeroVector, AwayAxis), 0.0, 1e-12));

	TestTrue(TEXT("the berth's box ends on Port0's plane"),
		FMath::IsNearlyEqual(StarForge::Docking::BoundsPastPortPlane(BerthBoxMetres(), BerthPort0Metres, AwayAxis), 0.0, 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingBoundsPastPortPlaneMeasuresAlongTheAxisTest,
	"StarForge.Core.Docking.BoundsPastPortPlaneMeasuresAlongTheAxis",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingBoundsPastPortPlaneMeasuresAlongTheAxisTest::RunTest(const FString& Parameters)
{
	using namespace StationKitTests;

	// Control N1's collider and N1b's, as the maths sees them.
	TestTrue(TEXT("a metre past the plane reads 1.0"),
		FMath::IsNearlyEqual(StarForge::Docking::BoundsPastPortPlane(FaceOnThePlane(1.0), FVector::ZeroVector, AwayAxis), 1.0, 1e-12));
	TestTrue(TEXT("twenty centimetres reads 0.2"),
		FMath::IsNearlyEqual(StarForge::Docking::BoundsPastPortPlane(FaceOnThePlane(0.2), FVector::ZeroVector, AwayAxis), 0.2, 1e-12));

	// The same metre with the port somewhere else: the answer is a length from the plane, not a coordinate.
	const FVector Elsewhere(5.0, -3.0, 1.0);
	const FBox Shifted = FaceOnThePlane(1.0).ShiftBy(Elsewhere);
	TestTrue(TEXT("measured from the port, wherever it is"),
		FMath::IsNearlyEqual(StarForge::Docking::BoundsPastPortPlane(Shifted, Elsewhere, AwayAxis), 1.0, 1e-12));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingBoundsPastPortPlaneIgnoresWhatIsBehindTest,
	"StarForge.Core.Docking.BoundsPastPortPlaneIgnoresWhatIsBehind",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingBoundsPastPortPlaneIgnoresWhatIsBehindTest::RunTest(const FString& Parameters)
{
	using namespace StationKitTests;

	// A box five metres short of the plane is not "minus five past it". The instrument looks one way
	// (spec 5, rule 1), and a boom that stops short of its port is caught by another number.
	const FBox Behind(FVector(-30.0, 5.0, -16.0), FVector(30.0, 40.0, 16.0));
	const double Past = StarForge::Docking::BoundsPastPortPlane(Behind, FVector::ZeroVector, AwayAxis);
	TestTrue(TEXT("what is behind the plane is exactly nothing past it"), Past == 0.0);

	// A station with nothing physical in it has nothing past any plane.
	TestTrue(TEXT("an empty box reaches nothing"),
		StarForge::Docking::BoundsPastPortPlane(FBox(ForceInit), FVector::ZeroVector, AwayAxis) == 0.0);

	// Nothing past a plane that can be measured. A port with no axis, or with an axis or an origin that is not
	// finite, answers NaN before the box is looked at, so an empty box cannot turn a port nobody could measure
	// into the calm zero above.
	TestFalse(TEXT("no axis answers no number"),
		FMath::IsFinite(StarForge::Docking::BoundsPastPortPlane(FaceOnThePlane(0.0), FVector::ZeroVector, FVector::ZeroVector)));
	TestFalse(TEXT("no axis over an empty box answers no number"),
		FMath::IsFinite(StarForge::Docking::BoundsPastPortPlane(FBox(ForceInit), FVector::ZeroVector, FVector::ZeroVector)));
	TestFalse(TEXT("a NaN axis over an empty box answers no number"),
		FMath::IsFinite(StarForge::Docking::BoundsPastPortPlane(FBox(ForceInit), FVector::ZeroVector, FVector(NaN, 0.0, 0.0))));
	TestFalse(TEXT("an infinite axis over an empty box answers no number"),
		FMath::IsFinite(StarForge::Docking::BoundsPastPortPlane(FBox(ForceInit), FVector::ZeroVector, FVector(Inf, 0.0, 0.0))));
	TestFalse(TEXT("a NaN origin over an empty box answers no number"),
		FMath::IsFinite(StarForge::Docking::BoundsPastPortPlane(FBox(ForceInit), FVector(NaN, 0.0, 0.0), AwayAxis)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingBoundsPastPortPlaneUsesTheSupportOnATurnedAxisTest,
	"StarForge.Core.Docking.BoundsPastPortPlaneUsesTheSupportOnATurnedAxis",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingBoundsPastPortPlaneUsesTheSupportOnATurnedAxisTest::RunTest(const FString& Parameters)
{
	// A unit-half cube about the port, the axis at 45 degrees: the corner (1, 1) is furthest along it,
	// at sqrt(2). A function that takes only the largest component of the axis reads 0.707 and calls a
	// protrusion half of what it is -- exact on the axes, wrong everywhere else.
	const FBox Cube(FVector(-1.0), FVector(1.0));
	TestTrue(TEXT("45 degrees: the corner, at sqrt(2)"),
		FMath::IsNearlyEqual(StarForge::Docking::BoundsPastPortPlane(Cube, FVector::ZeroVector, FVector(1.0, 1.0, 0.0)),
			FMath::Sqrt(2.0), 1e-12));

	// 30 degrees on a 2 x 1 x 1 half-box: 2 cos 30 + 1 sin 30.
	const double Rad = FMath::DegreesToRadians(30.0);
	const FBox Long(FVector(-2.0, -1.0, -1.0), FVector(2.0, 1.0, 1.0));
	TestTrue(TEXT("30 degrees: both components count"),
		FMath::IsNearlyEqual(
			StarForge::Docking::BoundsPastPortPlane(Long, FVector::ZeroVector, FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.0)),
			2.0 * FMath::Cos(Rad) + FMath::Sin(Rad), 1e-12));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingBoundsPastPortPlaneReadsZeroAtThePrintedResolutionTest,
	"StarForge.Core.Docking.BoundsPastPortPlaneReadsZeroAtThePrintedResolution",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingBoundsPastPortPlaneReadsZeroAtThePrintedResolutionTest::RunTest(const FString& Parameters)
{
	using namespace StationKitTests;

	// A nanometre past the plane is past it, and it is not red: the line prints metres to 0.001, and
	// red starts where the printed figure stops being 0.000 (spec 6.8). An exact comparison with zero
	// would paint a turned station red on a healthy build.
	const double Nano = StarForge::Docking::BoundsPastPortPlane(FaceOnThePlane(1e-9), FVector::ZeroVector, AwayAxis);
	TestTrue(TEXT("a nanometre is past the plane"), Nano > 0.0);
	TestFalse(TEXT("and is not red"), StarForge::Docking::IsRedAtPrintedResolution(Nano));

	// How a turned station arrives: its box and its port carried into the world through 90 degrees of
	// yaw and an offset, and measured there. Flush in its own frame, it must stay calm here.
	const FTransform Turned(FRotator(0.0, 90.0, 0.0), FVector(1234.5, -678.9, 42.0));
	const double Turn = StarForge::Docking::BoundsPastPortPlane(
		BerthBoxMetres().TransformBy(Turned),
		Turned.TransformPositionNoScale(BerthPort0Metres),
		Turned.TransformVectorNoScale(AwayAxis));
	TestTrue(TEXT("turned 90 degrees, the flush berth is not negative"), Turn >= 0.0);
	TestFalse(TEXT("and is not red"), StarForge::Docking::IsRedAtPrintedResolution(Turn));

	// A centimetre is the smallest real protrusion a whole-centimetre builder can make: red.
	TestTrue(TEXT("a centimetre is red"), StarForge::Docking::IsRedAtPrintedResolution(
		StarForge::Docking::BoundsPastPortPlane(FaceOnThePlane(0.01), FVector::ZeroVector, AwayAxis)));

	// The figures above hold any threshold between a nanometre and a centimetre, and asking about the constant
	// itself agrees with whatever the constant says. So the line's own print pins it, from both sides (spec
	// 6.8): the smallest red figure must not print as 0.000, and the largest calm one, a double under it, must.
	// One positive double does both, the first that "%.3f" rounds up to 0.001: the double nearest 0.0005, which is
	// 0.5 * 0.001 and lies 1.04e-20 above the tie. A whole millimetre, or a threshold one double above 0.0005,
	// prints a calm 0.001 and fails the last assertion; one double below prints a red 0.000 and fails the one
	// before it. The first two pin the comparison: red at the threshold itself, calm one double under it.
	const double SmallestRed = StarForge::Docking::RedAtOrAboveMetres;
	const double LargestCalm = std::nextafter(SmallestRed, 0.0);
	TestTrue(TEXT("the threshold itself is red"), StarForge::Docking::IsRedAtPrintedResolution(SmallestRed));
	TestFalse(TEXT("and one double under it is calm"), StarForge::Docking::IsRedAtPrintedResolution(LargestCalm));
	TestNotEqual(TEXT("the smallest red figure does not print as 0.000"),
		FString::Printf(TEXT("%.3f"), SmallestRed), FString(TEXT("0.000")));
	TestEqual(TEXT("the largest calm figure prints as 0.000"),
		FString::Printf(TEXT("%.3f"), LargestCalm), FString(TEXT("0.000")));

	TestFalse(TEXT("zero is not red"), StarForge::Docking::IsRedAtPrintedResolution(0.0));
	TestTrue(TEXT("and a figure nobody could compute is red, not calm"), StarForge::Docking::IsRedAtPrintedResolution(NaN));

	// And the measure hands the line that NaN, not a zero. A NaN in a box carries through the support and through
	// FMath::Max(0.0, Support); the same call the other way round answers 0.0 for it, because Max keeps its second
	// argument whenever the comparison fails.
	TestTrue(TEXT("a box with a NaN in it is red"), StarForge::Docking::IsRedAtPrintedResolution(
		StarForge::Docking::BoundsPastPortPlane(FBox(FVector(-1.0), FVector(NaN, 1.0, 1.0)), FVector::ZeroVector, AwayAxis)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingBoxReachOutsideIsZeroInsideTest,
	"StarForge.Core.Docking.BoxReachOutsideIsZeroInside",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingBoxReachOutsideIsZeroInsideTest::RunTest(const FString& Parameters)
{
	const FBox Envelope(FVector(-1.0), FVector(1.0));
	TestTrue(TEXT("a box inside its envelope reaches nothing out"),
		StarForge::Docking::BoxReachOutside(FBox(FVector(-0.5), FVector(0.5)), Envelope) == 0.0);
	TestTrue(TEXT("a box flush on every face reaches nothing out"),
		StarForge::Docking::BoxReachOutside(Envelope, Envelope) == 0.0);

	// Spec 6.8 on the boom's own numbers, block frame B, metres. The channel's +Y end is the hall-side
	// face of the front wall, -11.8, because the door in that wall belongs to the boom.
	const FBox Boom0(FVector(-12.62, -24.0, 12.0), FVector(-10.62, -11.8, 14.5));
	const FBox Channel(FVector(-12.62, -24.0, 12.0), FVector(-10.62, -11.8, 14.5));
	TestTrue(TEXT("Boom0 is within its channel"),
		FMath::IsNearlyEqual(StarForge::Docking::BoxReachOutside(Boom0, Channel), 0.0, 1e-12));

	// The trap spec 6.8 steps around: an envelope ended on the wall's outer face, -12, reads the
	// 20 cm of door as the volume leaving its channel, on a healthy build.
	const FBox ChannelToTheOuterFace(FVector(-12.62, -24.0, 12.0), FVector(-10.62, -12.0, 14.5));
	TestTrue(TEXT("an envelope ended at -12 reads 0.200"),
		FMath::IsNearlyEqual(StarForge::Docking::BoxReachOutside(Boom0, ChannelToTheOuterFace), 0.2, 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingBoxReachOutsideTakesTheLargestExcessTest,
	"StarForge.Core.Docking.BoxReachOutsideTakesTheLargestExcess",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingBoxReachOutsideTakesTheLargestExcessTest::RunTest(const FString& Parameters)
{
	using namespace StationKitTests;

	const FBox Envelope(FVector(-1.0), FVector(1.0));
	const FBox Out(FVector(-1.0, -1.0, -1.2), FVector(2.0, 1.0, 1.0));
	TestTrue(TEXT("a metre out of +X and 0.2 out of -Z reads the metre"),
		FMath::IsNearlyEqual(StarForge::Docking::BoxReachOutside(Out, Envelope), 1.0, 1e-12));

	// Every one of the six faces counts: a hall that leaves by its roof is as out as one that leaves by its front.
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		for (int32 Side = 0; Side < 2; ++Side)
		{
			FBox Probe(FVector(-1.0), FVector(1.0));
			if (Side == 0)
			{
				Probe.Min[Axis] -= 0.5;
			}
			else
			{
				Probe.Max[Axis] += 0.5;
			}
			TestTrue(FString::Printf(TEXT("half a metre out of the %s face of axis %d"), Side == 0 ? TEXT("min") : TEXT("max"), Axis),
				FMath::IsNearlyEqual(StarForge::Docking::BoxReachOutside(Probe, Envelope), 0.5, 1e-12));
		}
	}

	// An envelope nobody could read is not a calm zero.
	TestFalse(TEXT("an invalid envelope answers no number"),
		FMath::IsFinite(StarForge::Docking::BoxReachOutside(Out, FBox(ForceInit))));
	TestFalse(TEXT("nor does an envelope with a NaN in it"),
		FMath::IsFinite(StarForge::Docking::BoxReachOutside(Out, FBox(FVector(-1.0), FVector(NaN, 1.0, 1.0)))));

	// Nor is a volume nobody could read. Without the guard a NaN in Inner is forgotten by the next FMath::Max, which
	// keeps its second argument whenever the comparison fails: the NaN volume below would read a finite -0.5.
	TestFalse(TEXT("an invalid volume answers no number"),
		FMath::IsFinite(StarForge::Docking::BoxReachOutside(FBox(ForceInit), Envelope)));
	TestFalse(TEXT("nor does a volume with a NaN in it"),
		FMath::IsFinite(StarForge::Docking::BoxReachOutside(FBox(FVector(NaN, -0.5, -0.5), FVector(0.5)), Envelope)));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDockingReleaseStandoffGrowsByTheProtrusionTest,
	"StarForge.Core.Docking.ReleaseStandoffGrowsByTheProtrusion",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDockingReleaseStandoffGrowsByTheProtrusionTest::RunTest(const FString& Parameters)
{
	using namespace StationKitTests;

	// Spec 5, rule 1: d = R - h + p. The hull's box is 8 m, R = 4 sqrt(3); its +Y face lies on the port
	// plane, so its centre is h = 4 m out along the away axis; p is how far the station's box reaches
	// past the plane. Everything in metres and seconds, the release's own units.
	const double Radius = 4.0 * FMath::Sqrt(3.0);
	const FVector HullCentre(0.0, -4.0, 0.0);
	const auto Standoff = [&](double P)
	{
		return StarForge::Docking::ReleaseStandoff(HullCentre, Radius, AwayAxis, FaceOnThePlane(P));
	};
	const auto Peak = [](double D) { return StarForge::Docking::SmoothstepPeakAcceleration(D, 1.0); };
	const double Flush = Radius - 4.0;   // 2.928203

	TestTrue(TEXT("p = 0 stands the hull 2.928 m out"), FMath::IsNearlyEqual(Standoff(0.0), Flush, 1e-9));
	TestTrue(TEXT("p = 0.2 stands it 3.128 m out"), FMath::IsNearlyEqual(Standoff(0.2), Flush + 0.2, 1e-9));
	TestTrue(TEXT("p = 1.0 stands it 3.928 m out"), FMath::IsNearlyEqual(Standoff(1.0), Flush + 1.0, 1e-9));
	TestTrue(TEXT("and the peaks are 17.57, 18.77, 23.57 m/s2"),
		FMath::IsNearlyEqual(Peak(Standoff(0.0)), 6.0 * Flush, 1e-6)
		&& FMath::IsNearlyEqual(Peak(Standoff(0.2)), 6.0 * (Flush + 0.2), 1e-6)
		&& FMath::IsNearlyEqual(Peak(Standoff(1.0)), 6.0 * (Flush + 1.0), 1e-6));

	// The two thresholds, each from both sides, with the comparisons the warnings make (spec 5). The window
	// warns on d > 3.0, so p stays inside up to and including 7 - 4 sqrt(3) = 0.0717967697... m; the ceiling
	// warns on 6d >= 19.62, that is d >= 3.27, so p stays under it only below 7.27 - 4 sqrt(3) =
	// 0.3417967697... m. The six-decimal 0.071797 and 0.341797 are those bounds rounded up, by 2.3e-7 m, so
	// each already lies past its limit, as do the rounded 0.072 and 0.342. The probes bracket each bound
	// instead: 0.0717 and 0.3417 lie 9.7e-5 m on the calm side of it, 0.0719 and 0.3419 1.03e-4 m past it.
	const double Window = StarForge::Docking::FDockingLimits().DistanceMetres;
	const double Ceiling = StarForge::ShipFrame::GravPlateLimitMetersPerSecSq;
	TestTrue(TEXT("p = 0.0717 stays inside the 3.0 m window"), Standoff(0.0717) <= Window);
	TestTrue(TEXT("p = 0.0719 leaves it"), Standoff(0.0719) > Window);
	TestTrue(TEXT("p = 0.3417 peaks under the ceiling"), Peak(Standoff(0.3417)) < Ceiling);
	TestTrue(TEXT("p = 0.3419 peaks at or over it"), Peak(Standoff(0.3419)) >= Ceiling);

	return true;
}
