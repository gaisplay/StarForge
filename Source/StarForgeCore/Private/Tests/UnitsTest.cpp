#include "Misc/AutomationTest.h"
#include "Units.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitsLengthTest,
	"StarForge.Core.Units.Length",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUnitsLengthTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("a metre is a hundred centimetres"),
		FMath::IsNearlyEqual(StarForge::Units::MetresToCm(1.0), 100.0, 1e-12));
	TestTrue(TEXT("and back again"),
		FMath::IsNearlyEqual(StarForge::Units::CmToMetres(100.0), 1.0, 1e-12));

	// Round trip on an awkward number, because the failure this guards against is a
	// factor applied twice rather than a factor got wrong.
	const double Original = 3.7;
	TestTrue(TEXT("length survives a round trip"),
		FMath::IsNearlyEqual(
			StarForge::Units::CmToMetres(StarForge::Units::MetresToCm(Original)),
			Original, 1e-12));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitsForceTest,
	"StarForge.Core.Units.Force",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUnitsForceTest::RunTest(const FString& Parameters)
{
	// One newton accelerates one kilogram at one metre per second squared. Unreal
	// measures that acceleration in centimetres, so the same force reads as 100.
	TestTrue(TEXT("a newton is a hundred Unreal force units"),
		FMath::IsNearlyEqual(StarForge::Units::NewtonsToUu(1.0), 100.0, 1e-9));

	// A newton-metre carries a length twice over -- once in the force, once in the
	// lever arm -- so its factor is a hundred squared, not a hundred.
	TestTrue(TEXT("a newton-metre is ten thousand Unreal torque units"),
		FMath::IsNearlyEqual(StarForge::Units::NewtonMetresToUu(1.0), 10000.0, 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitsInertiaTest,
	"StarForge.Core.Units.Inertia",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUnitsInertiaTest::RunTest(const FString& Parameters)
{
	// Chaos reports the inertia tensor in kilogram centimetres squared.
	const FVector FromChaos(10000.0, 20000.0, 30000.0);
	const FVector InSi = StarForge::Units::InertiaUuToSi(FromChaos);

	TestTrue(TEXT("inertia divides by a hundred squared"),
		InSi.Equals(FVector(1.0, 2.0, 3.0), 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitsWrenchTest,
	"StarForge.Core.Units.Wrench",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUnitsWrenchTest::RunTest(const FString& Parameters)
{
	// Force and torque take different factors, so converting a wrench with one
	// factor for both is the mistake this exists to catch.
	StarForge::FWrench Si;
	Si.Force = FVector(1.0, 2.0, 3.0);
	Si.Torque = FVector(4.0, 5.0, 6.0);

	const StarForge::FWrench Uu = StarForge::Units::WrenchToUu(Si);

	TestTrue(TEXT("force scales by a hundred"),
		Uu.Force.Equals(FVector(100.0, 200.0, 300.0), 1e-6));
	TestTrue(TEXT("torque scales by ten thousand"),
		Uu.Torque.Equals(FVector(40000.0, 50000.0, 60000.0), 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInertiaRotationIsHonouredTest,
	"StarForge.Core.Units.InertiaRotationIsHonoured",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FInertiaRotationIsHonouredTest::RunTest(const FString& Parameters)
{
	// TransformByInertia has a rotation branch that no test has ever executed --
	// every fixture so far left InertiaRotation identity. Chaos is documented to
	// hand back an identity rotation too, so runtime will not cover it either, and
	// an untested branch that only wakes on an unusual hull is the worst kind.
	//
	// Principal moments (1, 2, 3) with the principal axes yawed 90 degrees from the
	// ship axes. A ship-frame +X vector lies along the principal +Y axis, whose
	// moment is 2, so the result is twice the input and still along ship +X.
	StarForge::FShipMassProperties Mass;
	Mass.InertiaDiagonal = FVector(1.0, 2.0, 3.0);
	Mass.InertiaRotation = FRotator(0.0, 90.0, 0.0).Quaternion();

	const FVector Result = Mass.TransformByInertia(FVector(1.0, 0.0, 0.0));

	TestTrue(TEXT("the principal-axis rotation is applied both ways"),
		Result.Equals(FVector(2.0, 0.0, 0.0), 1e-9));

	// With identity rotation the same call must be a plain componentwise scale, so
	// the rotation branch cannot be quietly wrong in the common case either.
	Mass.InertiaRotation = FQuat::Identity;
	TestTrue(TEXT("identity rotation is a plain componentwise scale"),
		Mass.TransformByInertia(FVector(1.0, 1.0, 1.0)).Equals(FVector(1.0, 2.0, 3.0), 1e-9));

	return true;
}
