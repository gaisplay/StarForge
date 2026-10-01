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

	// Ninety degrees cannot tell which way round the rotation goes: R and its inverse differ
	// there by a half turn, which commutes with a diagonal tensor, so swapping RotateVector
	// and UnrotateVector gives the same (2, 0, 0). At 45 degrees they part. The principal
	// axes are the ship's carried by InertiaRotation, so a vector along one of them must
	// come back scaled by that axis's moment alone: principal X, ship (1, 1, 0) / sqrt 2,
	// by 1, and principal Y, ship (-1, 1, 0) / sqrt 2, by 2. Swapped, the first reads twice
	// itself and the second once.
	Mass.InertiaRotation = FRotator(0.0, 45.0, 0.0).Quaternion();
	const FVector PrincipalX = Mass.InertiaRotation.RotateVector(FVector(1.0, 0.0, 0.0));
	const FVector PrincipalY = Mass.InertiaRotation.RotateVector(FVector(0.0, 1.0, 0.0));
	TestTrue(TEXT("the fixture's principal X lies along ship (1, 1, 0)"),
		PrincipalX.Equals(FVector(1.0, 1.0, 0.0) / FMath::Sqrt(2.0), 1e-9));
	TestTrue(TEXT("at 45 degrees a vector along principal X comes back times its moment of 1"),
		Mass.TransformByInertia(PrincipalX).Equals(PrincipalX, 1e-9));
	TestTrue(TEXT("and one along principal Y times its moment of 2"),
		Mass.TransformByInertia(PrincipalY).Equals(2.0 * PrincipalY, 1e-9));

	// With identity rotation the same call must be a plain componentwise scale, so
	// the rotation branch cannot be quietly wrong in the common case either.
	Mass.InertiaRotation = FQuat::Identity;
	TestTrue(TEXT("identity rotation is a plain componentwise scale"),
		Mass.TransformByInertia(FVector(1.0, 1.0, 1.0)).Equals(FVector(1.0, 2.0, 3.0), 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnitsDeadZoneTest,
	"StarForge.Core.Units.DeadZone",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FUnitsDeadZoneTest::RunTest(const FString& Parameters)
{
	// Inside the zone is nothing at all: a stick resting off-centre must not fly the
	// ship, and on a physical stick "resting off-centre" is the normal case.
	TestTrue(TEXT("noise inside the dead zone is discarded"),
		FMath::IsNearlyZero(StarForge::Units::ApplyDeadZone(0.05, 0.1), 1e-12));
	TestTrue(TEXT("and it is discarded either way round"),
		FMath::IsNearlyZero(StarForge::Units::ApplyDeadZone(-0.05, 0.1), 1e-12));

	// Just outside it, the output starts from zero rather than jumping to the
	// threshold. Returning the raw value would put a step at the most sensitive part
	// of the travel.
	// Bounded on both sides: an implementation that swallowed the first sliver of real
	// travel and returned a flat zero just past the threshold would pass a one-sided
	// upper bound, and the stick would have a wider dead zone than it was told to have.
	const double JustPast = StarForge::Units::ApplyDeadZone(0.11, 0.1);
	TestTrue(TEXT("just past the zone the output is near zero, not near the threshold"),
		JustPast < 0.02);
	TestTrue(TEXT("but it is above zero, so the zone is not wider than asked"),
		JustPast > 0.0);

	// Full travel still means full travel, in both directions.
	TestTrue(TEXT("full deflection is unchanged"),
		FMath::IsNearlyEqual(StarForge::Units::ApplyDeadZone(1.0, 0.1), 1.0, 1e-12));
	TestTrue(TEXT("and full deflection the other way too"),
		FMath::IsNearlyEqual(StarForge::Units::ApplyDeadZone(-1.0, 0.1), -1.0, 1e-12));

	// Halfway between the threshold and full travel is halfway out, not 0.55.
	TestTrue(TEXT("the remaining travel is rescaled, not offset"),
		FMath::IsNearlyEqual(StarForge::Units::ApplyDeadZone(0.55, 0.1), 0.5, 1e-9));

	// The same midpoint pulled the other way. Only full deflection was checked for a
	// negative before, and full deflection is the one negative value the rescale returns
	// correctly even if the sign is restored from the wrong quantity — a branch that
	// rescaled the magnitude and then forgot the sign, or took Sign of the rescaled
	// value, still gives -1 there. Mid-travel is where that bug shows.
	TestTrue(TEXT("negative mid-travel is rescaled with its sign intact"),
		FMath::IsNearlyEqual(StarForge::Units::ApplyDeadZone(-0.55, 0.1), -0.5, 1e-9));

	// Hardware and mixed inputs can hand us more than full deflection; the clamp is what
	// stops that reaching the flight loop as a command it is not entitled to assume.
	// Without FMath::Min this returns 2.111.
	TestTrue(TEXT("beyond full deflection is clamped to full, not passed through"),
		FMath::IsNearlyEqual(StarForge::Units::ApplyDeadZone(2.0, 0.1), 1.0, 1e-12));

	// A dead zone of 1.0 is what an operator types into the editor to mean "ignore this
	// stick entirely", and it has to mean that at every input, the stop included. A zone
	// clamped to 0.99 reads 0.5 at 0.995 and full deflection at 1.0, so a pad pushed to its
	// stop flies the ship at full thrust. Over-range input is the case that would otherwise
	// reach the rescale with a zero denominator: (2 - 1) / 0 is an infinity, which the
	// output clamp would turn into full deflection rather than nothing.
	TestTrue(TEXT("a dead zone of one ignores full deflection"),
		StarForge::Units::ApplyDeadZone(1.0, 1.0) == 0.0);
	TestTrue(TEXT("and the last hundredth of the travel"),
		StarForge::Units::ApplyDeadZone(0.995, 1.0) == 0.0);
	TestTrue(TEXT("and over-range input"),
		StarForge::Units::ApplyDeadZone(2.0, 1.0) == 0.0);

	// A zone between 0.99 and 1 is honoured to the last bit of it, not cut to 0.99: 0.992 is
	// inside a zone of 0.995, where the clamped form read 0.2.
	TestTrue(TEXT("a dead zone of 0.995 covers 0.992"),
		StarForge::Units::ApplyDeadZone(0.992, 0.995) == 0.0);

	// A zero dead zone must be a pass-through, so turning the feature off cannot
	// itself distort the axis.
	TestTrue(TEXT("a zero dead zone changes nothing"),
		FMath::IsNearlyEqual(StarForge::Units::ApplyDeadZone(0.37, 0.0), 0.37, 1e-12));

	return true;
}
