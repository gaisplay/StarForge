#include "Misc/AutomationTest.h"
#include "ViewMath.h"

#include <limits>

namespace
{
	constexpr double Deg = UE_DOUBLE_PI / 180.0;

	/** UCameraComponent's defaults: a 90 degree horizontal field at 16:9. */
	constexpr double DefaultFov = 90.0 * Deg;
	constexpr double Widescreen = 16.0 / 9.0;

	/**
	 * Half of the 48.861 x 30.08 x 22.449 m the Krishna's import test measured on the asset
	 * (Art/Krishna/ue_import_test.md on the art branch). Only the half extents enter the sphere.
	 */
	const FVector KrishnaHalfExtentsMetres(24.4305, 15.04, 11.2245);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FViewKrishnaFitsTheVerticalFieldTest,
	"StarForge.Core.View.KrishnaFitsTheVerticalField",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FViewKrishnaFitsTheVerticalFieldTest::RunTest(const FString& Parameters)
{
	// R^2 = 24.4305^2 + 15.04^2 + 11.2245^2
	//     = 596.84933025 + 226.2016 + 125.98940025 = 949.0403305, so R = 30.8065 m.
	const double R = KrishnaHalfExtentsMetres.Size();
	TestTrue(TEXT("the fixture: R^2 is 949.0403305"), FMath::IsNearlyEqual(R * R, 949.0403305, 1e-9));

	// tan(90 / 2) = 1, so tan(V / 2) = 1 / (16 / 9) = 9 / 16 = 0.5625 exactly, and
	// V = 2 atan(0.5625) = 58.7155 degrees. Dividing the angle instead of its tangent would say
	// 90 / (16 / 9) = 50.625; dropping the halves would take tan 90, which is infinite.
	const double V = StarForge::View::VerticalFov(DefaultFov, Widescreen);
	TestTrue(TEXT("tan(V / 2) is 9 / 16"), FMath::IsNearlyEqual(FMath::Tan(0.5 * V), 0.5625, 1e-12));
	TestTrue(TEXT("V is 58.7155 degrees"), FMath::IsNearlyEqual(V / Deg, 58.7155, 1e-4));

	// Landscape, so the vertical field is the narrower one and it binds.
	// sin(V / 2) = t / sqrt(1 + t^2) with t = 9 / 16, which is 9 / sqrt(337); so
	// d = R / sin(V / 2) = R sqrt(337) / 9 = 30.8065 * 2.03973 = 62.8369 m.
	// Written from the tangent, not through asin or atan, so it cannot share a mistake with
	// the code. The wrong answers it tells apart: the horizontal field, R / sin 45 = 43.567;
	// the tangent, R / tan(V / 2) = 16 R / 9 = 54.767; the full angle, R / sin V = 36.048.
	const double D = StarForge::View::FitSphereDistance(R, DefaultFov, Widescreen);
	TestTrue(TEXT("d is R sqrt(337) / 9"), FMath::IsNearlyEqual(D, R * FMath::Sqrt(337.0) / 9.0, 1e-9));
	TestTrue(TEXT("which is 62.8369 m"), FMath::IsNearlyEqual(D, 62.8369, 1e-4));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FViewDistanceMeetsTheDefiningIdentityTest,
	"StarForge.Core.View.DistanceMeetsTheDefiningIdentity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FViewDistanceMeetsTheDefiningIdentityTest::RunTest(const FString& Parameters)
{
	// The definition itself, over radii in both of the units the game has, fields from narrow
	// to wide and aspects on both sides of square: from d, the tangent cone of half-angle
	// asin(R / d) is exactly half the binding field, so d sin(F / 2) = R.
	const double Radii[] = { 1.0, 400.0 * FMath::Sqrt(3.0), 3080.668 };
	const double Fields[] = { 30.0 * Deg, 90.0 * Deg, 150.0 * Deg };
	const double Aspects[] = { 0.5, 1.0, Widescreen, 4.0 };

	for (const double R : Radii)
	{
		for (const double H : Fields)
		{
			for (const double A : Aspects)
			{
				const double Binding = FMath::Min(H, StarForge::View::VerticalFov(H, A));
				const double D = StarForge::View::FitSphereDistance(R, H, A);

				TestTrue(FString::Printf(TEXT("d sin(F / 2) = R at R %.3f, H %.0f deg, aspect %.3f"), R, H / Deg, A),
					FMath::IsNearlyEqual(D * FMath::Sin(0.5 * Binding), R, 1e-9 * R));
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FViewPortraitIsBoundByTheHorizontalFieldTest,
	"StarForge.Core.View.PortraitIsBoundByTheHorizontalField",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FViewPortraitIsBoundByTheHorizontalFieldTest::RunTest(const FString& Parameters)
{
	// At aspect 0.5 the image is twice as tall as it is wide: tan(V / 2) = 1 / 0.5 = 2, so
	// V = 2 atan 2 = 126.87 degrees, wider than the 90 across. The horizontal field binds, and
	// d = R / sin 45 = R sqrt 2. Fitting the vertical field would give R / sin 63.43 = 1.118 R
	// and cut the sphere off at the sides.
	const double R = 10.0;

	const double V = StarForge::View::VerticalFov(DefaultFov, 0.5);
	TestTrue(TEXT("tan(V / 2) is 2"), FMath::IsNearlyEqual(FMath::Tan(0.5 * V), 2.0, 1e-12));
	TestTrue(TEXT("and V is the wider field"), V > DefaultFov);

	TestTrue(TEXT("d is R sqrt 2"),
		FMath::IsNearlyEqual(StarForge::View::FitSphereDistance(R, DefaultFov, 0.5), R * FMath::Sqrt(2.0), 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FViewChasePoseLooksAtTheCentreTest,
	"StarForge.Core.View.ChasePoseLooksAtTheCentre",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FViewChasePoseLooksAtTheCentreTest::RunTest(const FString& Parameters)
{
	// The ship's own case: the centre of the bounds the import test measured, in metres,
	// X (-12.81064 + 36.05000) / 2 = 11.61968 and Z (-10.29893 + 12.15054) / 2 = 0.925805, and
	// the half-diagonal above. Off the origin on purpose, so a pose that forgot the centre
	// cannot pass.
	const FVector Centre(11.61968, 0.0, 0.925805);
	const double R = KrishnaHalfExtentsMetres.Size();
	const double D = StarForge::View::FitSphereDistance(R, DefaultFov, Widescreen);
	TestTrue(TEXT("the fixture has a distance"), D > 0.0);

	for (const double ElevationDeg : { 0.0, 20.0, 60.0 })
	{
		const double E = ElevationDeg * Deg;
		const FTransform Pose = StarForge::View::ChaseCameraPose(Centre, R, DefaultFov, Widescreen, E);
		const FVector Location = Pose.GetLocation();
		const FQuat Rotation = Pose.GetRotation();

		const FString At = FString::Printf(TEXT(" at %.0f deg"), ElevationDeg);

		TestTrue(TEXT("it sits at distance d") + At,
			FMath::IsNearlyEqual(FVector::Dist(Location, Centre), D, 1e-9 * D));

		// Behind and above, each by its own component. The look-at check below cannot see a pose
		// that is below the centre and pitched up at it, because that pose also looks at the centre.
		TestTrue(TEXT("behind the centre by d cos e") + At,
			FMath::IsNearlyEqual(Centre.X - Location.X, D * FMath::Cos(E), 1e-9 * D));
		TestTrue(TEXT("above it by d sin e") + At,
			FMath::IsNearlyEqual(Location.Z - Centre.Z, D * FMath::Sin(E), 1e-9 * D));
		TestTrue(TEXT("and not off to either side") + At,
			FMath::IsNearlyEqual(Location.Y, Centre.Y, 1e-9 * D));

		// The view axis runs from the camera through the centre, and the camera is not rolled:
		// its right is the frame's +Y, which is the same as saying yaw 0 and roll 0.
		TestTrue(TEXT("its forward points at the centre") + At,
			Rotation.GetForwardVector().Equals((Centre - Location).GetSafeNormal(), 1e-9));
		TestTrue(TEXT("its right is the frame's +Y") + At,
			Rotation.GetRightVector().Equals(FVector(0.0, 1.0, 0.0), 1e-9));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FViewInvalidInputsGiveZeroTest,
	"StarForge.Core.View.InvalidInputsGiveZero",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FViewInvalidInputsGiveZeroTest::RunTest(const FString& Parameters)
{
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const double Inf = std::numeric_limits<double>::infinity();

	// The fixture before the behaviour, as in BallisticsTest: a NaN a compiler flag had made
	// finite would let every refusal below pass for the wrong reason.
	TestTrue(TEXT("the NaN really is not finite"), !FMath::IsFinite(NaN));
	TestTrue(TEXT("and the infinity really is not finite"), !FMath::IsFinite(Inf));

	using StarForge::View::FitSphereDistance;
	const double R = 10.0;

	// Exactly zero each time, compared with ==: a NaN is not zero, and neither is an infinity.
	TestTrue(TEXT("zero radius"), FitSphereDistance(0.0, DefaultFov, Widescreen) == 0.0);
	TestTrue(TEXT("negative radius"), FitSphereDistance(-1.0, DefaultFov, Widescreen) == 0.0);
	TestTrue(TEXT("NaN radius"), FitSphereDistance(NaN, DefaultFov, Widescreen) == 0.0);
	TestTrue(TEXT("infinite radius"), FitSphereDistance(Inf, DefaultFov, Widescreen) == 0.0);

	TestTrue(TEXT("zero field"), FitSphereDistance(R, 0.0, Widescreen) == 0.0);
	TestTrue(TEXT("negative field"), FitSphereDistance(R, -0.1, Widescreen) == 0.0);
	TestTrue(TEXT("a field of pi"), FitSphereDistance(R, UE_DOUBLE_PI, Widescreen) == 0.0);
	TestTrue(TEXT("a field past pi"), FitSphereDistance(R, 4.0, Widescreen) == 0.0);
	TestTrue(TEXT("NaN field"), FitSphereDistance(R, NaN, Widescreen) == 0.0);

	TestTrue(TEXT("zero aspect"), FitSphereDistance(R, DefaultFov, 0.0) == 0.0);
	TestTrue(TEXT("negative aspect"), FitSphereDistance(R, DefaultFov, -1.0) == 0.0);
	TestTrue(TEXT("NaN aspect"), FitSphereDistance(R, DefaultFov, NaN) == 0.0);

	// Positive, so it passes the aspect's own guard, and leaves no vertical field at all.
	TestTrue(TEXT("infinite aspect"), FitSphereDistance(R, DefaultFov, Inf) == 0.0);

	// No distance, no pose: the identity, not the centre. Off the origin so the two differ.
	const FTransform Pose =
		StarForge::View::ChaseCameraPose(FVector(100.0, 200.0, 300.0), 0.0, DefaultFov, Widescreen, 20.0 * Deg);
	TestTrue(TEXT("the pose with no distance is at the frame's origin"), Pose.GetLocation().IsZero());
	TestTrue(TEXT("and unrotated"), Pose.GetRotation().Equals(FQuat::Identity, 1e-12));

	return true;
}
