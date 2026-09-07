#include "Misc/AutomationTest.h"
#include "FlightController.h"

namespace
{
	StarForge::FShipMassProperties MakeAsymmetricShip()
	{
		// Deliberately unequal principal moments: a symmetric ship would hide a
		// missing gyroscopic term, because omega cross I omega vanishes when the
		// inertia is a multiple of the identity.
		StarForge::FShipMassProperties Mass;
		Mass.Mass = 10000.0;
		Mass.CentreOfMass = FVector::ZeroVector;
		Mass.InertiaDiagonal = FVector(20000.0, 60000.0, 50000.0);
		Mass.InertiaRotation = FQuat::Identity;
		return Mass;
	}

	StarForge::FFlightLimits MakeLimits()
	{
		StarForge::FFlightLimits Limits;
		Limits.MaxSpeed = 200.0;
		Limits.MaxAngularSpeed = FMath::DegreesToRadians(120.0);
		Limits.LinearGain = 2.0;
		Limits.AngularGain = 3.0;
		return Limits;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerAcceleratesFromRestTest,
	"StarForge.Core.FlightController.AcceleratesFromRest",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerAcceleratesFromRestTest::RunTest(const FString& Parameters)
{
	StarForge::FFlightIntent Intent;
	Intent.LinearAxis = FVector(1.0, 0.0, 0.0);

	StarForge::FRigidBodyState State;

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		Intent, State, MakeAsymmetricShip(), MakeLimits());

	// Full forward from rest: mass times the whole speed target times the gain.
	TestTrue(TEXT("full forward demand is mass times target speed times gain"),
		FMath::IsNearlyEqual(Demand.Force.X, 10000.0 * 200.0 * 2.0, 1.0));
	TestTrue(TEXT("no sideways demand"), FMath::Abs(Demand.Force.Y) < 1e-6);
	TestTrue(TEXT("no torque demand"), Demand.Torque.IsNearlyZero(1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerBrakesToStopTest,
	"StarForge.Core.FlightController.BrakesToStop",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerBrakesToStopTest::RunTest(const FString& Parameters)
{
	// Stick released while moving: assist demands the force that kills the speed.
	// This is what makes releasing the stick stop the ship rather than coast.
	StarForge::FFlightIntent Intent;

	StarForge::FRigidBodyState State;
	State.LinearVelocity = FVector(50.0, 0.0, 0.0);

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		Intent, State, MakeAsymmetricShip(), MakeLimits());

	TestTrue(TEXT("braking demand opposes the motion"), Demand.Force.X < 0.0);
	TestTrue(TEXT("braking demand is mass times the excess speed times gain"),
		FMath::IsNearlyEqual(Demand.Force.X, -10000.0 * 50.0 * 2.0, 1.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerGyroscopicTermTest,
	"StarForge.Core.FlightController.GyroscopicTermIsPresent",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerGyroscopicTermTest::RunTest(const FString& Parameters)
{
	// A ship already spinning about two axes at its commanded rate has no angular
	// error at all, yet Euler's equation still demands torque to hold that spin.
	// Without omega cross I omega this comes out zero and heavy ships turn like
	// light ones.
	const StarForge::FShipMassProperties Mass = MakeAsymmetricShip();
	const StarForge::FFlightLimits Limits = MakeLimits();

	StarForge::FRigidBodyState State;
	State.AngularVelocity = FVector(Limits.MaxAngularSpeed, Limits.MaxAngularSpeed, 0.0);

	StarForge::FFlightIntent Intent;
	Intent.AngularAxis = FVector(1.0, 1.0, 0.0);

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		Intent, State, Mass, Limits);

	// omega x (I omega) with omega = (w, w, 0) and I = diag(Ix, Iy, Iz):
	//   x: w*0 - 0*Iy*w            = 0
	//   y: 0*Ix*w - w*0            = 0
	//   z: w*(Iy*w) - w*(Ix*w)     = w^2 * (Iy - Ix)
	// So a yaw demand of w^2 * (60000 - 20000) and nothing on the other two axes.
	const double W = Limits.MaxAngularSpeed;
	const double ExpectedZ = W * W * (60000.0 - 20000.0);

	TestTrue(TEXT("holding a spin still demands torque"),
		!Demand.Torque.IsNearlyZero(1.0));
	TestTrue(TEXT("the gyroscopic demand matches Euler's equation"),
		FMath::IsNearlyEqual(Demand.Torque.Z, ExpectedZ, 1.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerAssistOffTest,
	"StarForge.Core.FlightController.AssistOffIsDirectDemand",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerAssistOffTest::RunTest(const FString& Parameters)
{
	// With assist off the stick stops asking for a speed and asks for thrust, so a
	// ship already at full speed keeps being pushed instead of being held there.
	StarForge::FFlightIntent Intent;
	Intent.LinearAxis = FVector(1.0, 0.0, 0.0);
	Intent.bFlightAssistEnabled = false;

	StarForge::FRigidBodyState State;
	State.LinearVelocity = FVector(200.0, 0.0, 0.0);

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		Intent, State, MakeAsymmetricShip(), MakeLimits());

	TestTrue(TEXT("assist off keeps demanding forward thrust at top speed"),
		Demand.Force.X > 0.0);

	StarForge::FFlightIntent Assisted = Intent;
	Assisted.bFlightAssistEnabled = true;

	const StarForge::FWrench AssistedDemand = StarForge::ComputeDemandedWrench(
		Assisted, State, MakeAsymmetricShip(), MakeLimits());

	TestTrue(TEXT("assist on stops demanding once the target speed is reached"),
		FMath::Abs(AssistedDemand.Force.X) < 1.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerConvergesTest,
	"StarForge.Core.FlightController.ConvergesWithoutOscillation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerConvergesTest::RunTest(const FString& Parameters)
{
	// Closed loop with a plain integrator and no thruster limit: commanded from
	// rest, the ship must reach its target speed and stay there. Overshoot here
	// would be felt in play as a ship that lurches past what the pilot asked for.
	const StarForge::FShipMassProperties Mass = MakeAsymmetricShip();
	const StarForge::FFlightLimits Limits = MakeLimits();

	StarForge::FFlightIntent Intent;
	Intent.LinearAxis = FVector(1.0, 0.0, 0.0);

	StarForge::FRigidBodyState State;

	const double Step = 1.0 / 120.0;
	double WorstOvershoot = 0.0;

	for (int32 Tick = 0; Tick < 2400; ++Tick)
	{
		const StarForge::FWrench Demand =
			StarForge::ComputeDemandedWrench(Intent, State, Mass, Limits);

		State.LinearVelocity += (Demand.Force / Mass.Mass) * Step;
		WorstOvershoot = FMath::Max(WorstOvershoot, State.LinearVelocity.X - Limits.MaxSpeed);
	}

	TestTrue(TEXT("the ship reaches its commanded speed"),
		FMath::IsNearlyEqual(State.LinearVelocity.X, Limits.MaxSpeed, 0.5));
	TestTrue(TEXT("and does not overshoot it"), WorstOvershoot < 0.5);

	return true;
}
