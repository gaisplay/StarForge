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
		// Angular limits are an angular velocity vector: X roll, Y pitch, Z yaw.
		// Spec 7 asks for 180 degrees per second of roll and 120 of pitch and yaw,
		// which is three numbers and cannot be held by one.
		Limits.MaxAngularSpeed = FVector(
			FMath::DegreesToRadians(180.0),
			FMath::DegreesToRadians(120.0),
			FMath::DegreesToRadians(120.0));
		Limits.LinearGain = 2.0;
		Limits.AngularGain = 3.0;
		return Limits;
	}

	/** What the test ship array can produce on each axis, in newtons and newton-metres. */
	StarForge::FWrench MakeCapability()
	{
		StarForge::FWrench Capability;
		Capability.Force = FVector(8000.0, 8000.0, 8000.0);
		Capability.Torque = FVector(16000.0, 16000.0, 16000.0);
		return Capability;
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
		Intent, State, MakeAsymmetricShip(), MakeLimits(), MakeCapability());

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
		Intent, State, MakeAsymmetricShip(), MakeLimits(), MakeCapability());

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
	const StarForge::FShipMassProperties Mass = MakeAsymmetricShip();
	const StarForge::FFlightLimits Limits = MakeLimits();

	// Spinning about roll and pitch at exactly the commanded rate: no angular error
	// at all, yet Euler still demands torque to hold it.
	const double RollRate = Limits.MaxAngularSpeed.X;
	const double PitchRate = Limits.MaxAngularSpeed.Y;

	StarForge::FRigidBodyState State;
	State.AngularVelocity = FVector(RollRate, PitchRate, 0.0);

	StarForge::FFlightIntent Intent;
	Intent.AngularAxis = FVector(1.0, 1.0, 0.0);

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		Intent, State, Mass, Limits, MakeCapability());

	// omega x (I omega) with omega = (wx, wy, 0) and I = diag(Ix, Iy, Iz):
	//   x: wy*0 - 0*Iy*wy        = 0
	//   y: 0*Ix*wx - wx*0        = 0
	//   z: wx*(Iy*wy) - wy*(Ix*wx) = wx*wy*(Iy - Ix)
	const double ExpectedZ = RollRate * PitchRate * (60000.0 - 20000.0);

	TestTrue(TEXT("holding a spin still demands torque"),
		!Demand.Torque.IsNearlyZero(1.0));
	TestTrue(TEXT("the gyroscopic demand matches Euler equation"),
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
		Intent, State, MakeAsymmetricShip(), MakeLimits(), MakeCapability());

	TestTrue(TEXT("assist off keeps demanding forward thrust at top speed"),
		Demand.Force.X > 0.0);

	StarForge::FFlightIntent Assisted = Intent;
	Assisted.bFlightAssistEnabled = true;

	const StarForge::FWrench AssistedDemand = StarForge::ComputeDemandedWrench(
		Assisted, State, MakeAsymmetricShip(), MakeLimits(), MakeCapability());

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
			StarForge::ComputeDemandedWrench(Intent, State, Mass, Limits, MakeCapability());

		State.LinearVelocity += (Demand.Force / Mass.Mass) * Step;
		WorstOvershoot = FMath::Max(WorstOvershoot, State.LinearVelocity.X - Limits.MaxSpeed);
	}

	TestTrue(TEXT("the ship reaches its commanded speed"),
		FMath::IsNearlyEqual(State.LinearVelocity.X, Limits.MaxSpeed, 0.5));
	TestTrue(TEXT("and does not overshoot it"), WorstOvershoot < 0.5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerAssistOffIsProportionalTest,
	"StarForge.Core.FlightController.AssistOffIsProportionalToThrust",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerAssistOffIsProportionalTest::RunTest(const FString& Parameters)
{
	// Spec 4: with assist off the axis is a fraction of what the array can produce.
	// Half the stick must be half the thrust -- on a physical stick, a demand that
	// saturates the thrusters early leaves most of the travel doing nothing.
	const StarForge::FWrench Capability = MakeCapability();

	StarForge::FFlightIntent Intent;
	Intent.bFlightAssistEnabled = false;
	Intent.LinearAxis = FVector(0.5, 0.0, 0.0);

	StarForge::FRigidBodyState State;

	const StarForge::FWrench Half = StarForge::ComputeDemandedWrench(
		Intent, State, MakeAsymmetricShip(), MakeLimits(), Capability);

	TestTrue(TEXT("half the stick asks for half of what the array can produce"),
		FMath::IsNearlyEqual(Half.Force.X, 0.5 * Capability.Force.X, 1e-6));

	Intent.LinearAxis = FVector(1.0, 0.0, 0.0);
	const StarForge::FWrench Full = StarForge::ComputeDemandedWrench(
		Intent, State, MakeAsymmetricShip(), MakeLimits(), Capability);

	TestTrue(TEXT("full stick asks for exactly what the array can produce"),
		FMath::IsNearlyEqual(Full.Force.X, Capability.Force.X, 1e-6));

	// Speed must not enter into it: the ship at its limit is still pushed.
	State.LinearVelocity = FVector(200.0, 0.0, 0.0);
	const StarForge::FWrench AtSpeed = StarForge::ComputeDemandedWrench(
		Intent, State, MakeAsymmetricShip(), MakeLimits(), Capability);

	TestTrue(TEXT("assist off ignores how fast the ship already goes"),
		FMath::IsNearlyEqual(AtSpeed.Force.X, Capability.Force.X, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerPerAxisAngularLimitTest,
	"StarForge.Core.FlightController.AngularLimitIsPerAxis",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerPerAxisAngularLimitTest::RunTest(const FString& Parameters)
{
	// Spec 7 gives roll a higher ceiling than pitch. A single scalar cannot express
	// that, and the failure is silent: either roll is a third too slow or pitch is
	// half again too fast, and both feel like tuning rather than a bug.
	const StarForge::FShipMassProperties Mass = MakeAsymmetricShip();
	const StarForge::FFlightLimits Limits = MakeLimits();

	StarForge::FRigidBodyState State;

	StarForge::FFlightIntent Roll;
	Roll.AngularAxis = FVector(1.0, 0.0, 0.0);
	const StarForge::FWrench RollDemand = StarForge::ComputeDemandedWrench(
		Roll, State, Mass, Limits, MakeCapability());

	StarForge::FFlightIntent Pitch;
	Pitch.AngularAxis = FVector(0.0, 1.0, 0.0);
	const StarForge::FWrench PitchDemand = StarForge::ComputeDemandedWrench(
		Pitch, State, Mass, Limits, MakeCapability());

	// Demand is inertia times gain times the target rate, so comparing the two
	// axes means dividing out their different moments of inertia first.
	const double RollRateAsked = RollDemand.Torque.X / (20000.0 * Limits.AngularGain);
	const double PitchRateAsked = PitchDemand.Torque.Y / (60000.0 * Limits.AngularGain);

	TestTrue(TEXT("full roll asks for the roll ceiling"),
		FMath::IsNearlyEqual(RollRateAsked, FMath::DegreesToRadians(180.0), 1e-6));
	TestTrue(TEXT("full pitch asks for the pitch ceiling"),
		FMath::IsNearlyEqual(PitchRateAsked, FMath::DegreesToRadians(120.0), 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerAngularAxisOrderTest,
	"StarForge.Core.FlightController.AngularAxisOrderIsRollPitchYaw",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerAngularAxisOrderTest::RunTest(const FString& Parameters)
{
	// AngularAxis is an angular velocity vector: X roll, Y pitch, Z yaw. Spec 4
	// lists the three by name in a different order, which is a description of what
	// they are for and not of how they are stored. A ship that rolls when the pilot
	// asks for pitch is the shape this mistake takes, and it surfaces weeks later.
	//
	// The inertia here is diagonal in ship axes, so a demand about one axis alone
	// must produce torque about that axis alone.
	const StarForge::FShipMassProperties Mass = MakeAsymmetricShip();
	const StarForge::FFlightLimits Limits = MakeLimits();
	StarForge::FRigidBodyState State;

	StarForge::FFlightIntent Intent;
	Intent.AngularAxis = FVector(1.0, 0.0, 0.0);

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		Intent, State, Mass, Limits, MakeCapability());

	TestTrue(TEXT("a roll command torques about X"), Demand.Torque.X > 0.0);
	TestTrue(TEXT("and not about Y"), FMath::Abs(Demand.Torque.Y) < 1e-9);
	TestTrue(TEXT("and not about Z"), FMath::Abs(Demand.Torque.Z) < 1e-9);

	return true;
}
