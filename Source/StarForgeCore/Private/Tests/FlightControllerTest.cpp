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
	FFlightControllerAssistOffTurnsTest,
	"StarForge.Core.FlightController.AssistOffTurnsAndKeepsTheGyroscopicTerm",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerAssistOffTurnsTest::RunTest(const FString& Parameters)
{
	// The two assist-off tests above set only the linear axis on a ship that is not turning,
	// and assert only on the force, so the angular half of that branch -- and the gyroscopic
	// term, which sits outside both branches -- were held by nothing there: a zeroed torque
	// with assist off, or the Euler term moved inside the assist branch, passed. Here the
	// stick asks for a mixed turn while the ship already spins about roll and pitch.
	//
	// Assist off, the torque is the stick's fraction of what the array can produce plus
	// w x (I w), exactly as in GyroscopicTermIsPresent: with w = (wx, wy, 0) and a diagonal
	// I, that term is (0, 0, wx * wy * (Iy - Ix)).
	const StarForge::FShipMassProperties Mass = MakeAsymmetricShip();
	const StarForge::FFlightLimits Limits = MakeLimits();
	const StarForge::FWrench Capability = MakeCapability();

	StarForge::FFlightIntent Intent;
	Intent.bFlightAssistEnabled = false;
	Intent.AngularAxis = FVector(0.5, -1.0, 0.25);

	StarForge::FRigidBodyState State;
	const double RollRate = 0.4;
	const double PitchRate = 0.3;
	State.AngularVelocity = FVector(RollRate, PitchRate, 0.0);

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(Intent, State, Mass, Limits, Capability);

	const double Gyro = RollRate * PitchRate * (60000.0 - 20000.0);
	TestTrue(TEXT("roll is half the array's roll torque"),
		FMath::IsNearlyEqual(Demand.Torque.X, 0.5 * Capability.Torque.X, 1e-6));
	TestTrue(TEXT("pitch is the whole of it, the other way"),
		FMath::IsNearlyEqual(Demand.Torque.Y, -1.0 * Capability.Torque.Y, 1e-6));
	TestTrue(TEXT("and yaw is a quarter of it plus the gyroscopic term, assist off as on"),
		FMath::IsNearlyEqual(Demand.Torque.Z, 0.25 * Capability.Torque.Z + Gyro, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerAxesAreClampedTest,
	"StarForge.Core.FlightController.AxesBeyondFullTravelAreClamped",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerAxesAreClampedTest::RunTest(const FString& Parameters)
{
	// The intent's axes are promised in [-1, 1], and not every operator keeps the promise:
	// the console operator copies the sf.Intent.* variables in as typed, and the pilot's post
	// hands the intent straight here. So the clamp in this function is the only one on that
	// path, and every other fixture in this file sits inside the range, where it does nothing.
	// Unclamped, sf.Intent.X 2 with assist on asks for 400 m/s, the boost figure, instead of
	// the 200 m/s manoeuvring limit.
	//
	// One axis at a time, with both signs among them, so the fixtures read the same under a
	// clamp of each component and under a clamp of the vector's length. The length is what
	// spec 7 means (the owner, 2026-10-01); DiagonalSpeedIsTheLimit holds that half.
	const StarForge::FShipMassProperties Mass = MakeAsymmetricShip();
	const StarForge::FFlightLimits Limits = MakeLimits();
	const StarForge::FWrench Capability = MakeCapability();
	const StarForge::FRigidBodyState AtRest;

	const FVector Moments(20000.0, 60000.0, 50000.0);
	const double Overdrive[3] = { 2.0, -3.0, 1.5 };

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const double Sign = FMath::Sign(Overdrive[Axis]);

		StarForge::FFlightIntent Linear;
		Linear.LinearAxis[Axis] = Overdrive[Axis];
		const StarForge::FWrench LinearDemand =
			StarForge::ComputeDemandedWrench(Linear, AtRest, Mass, Limits, Capability);
		TestTrue(*FString::Printf(TEXT("linear axis %d at %.1f asks for the full-stick speed and no more"), Axis, Overdrive[Axis]),
			FMath::IsNearlyEqual(LinearDemand.Force[Axis], Sign * Mass.Mass * Limits.MaxSpeed * Limits.LinearGain, 1e-6));

		StarForge::FFlightIntent Angular;
		Angular.AngularAxis[Axis] = Overdrive[Axis];
		const StarForge::FWrench AngularDemand =
			StarForge::ComputeDemandedWrench(Angular, AtRest, Mass, Limits, Capability);
		TestTrue(*FString::Printf(TEXT("angular axis %d at %.1f asks for the full-stick rate and no more"), Axis, Overdrive[Axis]),
			FMath::IsNearlyEqual(AngularDemand.Torque[Axis],
				Sign * Moments[Axis] * Limits.MaxAngularSpeed[Axis] * Limits.AngularGain, 1e-6));

		StarForge::FFlightIntent Unassisted = Linear;
		Unassisted.bFlightAssistEnabled = false;
		const StarForge::FWrench UnassistedDemand =
			StarForge::ComputeDemandedWrench(Unassisted, AtRest, Mass, Limits, Capability);
		TestTrue(*FString::Printf(TEXT("and assist off, linear axis %d asks for the whole array and no more"), Axis),
			FMath::IsNearlyEqual(UnassistedDemand.Force[Axis], Sign * Capability.Force[Axis], 1e-6));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFlightControllerDiagonalSpeedTest,
	"StarForge.Core.FlightController.DiagonalSpeedIsTheLimit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFlightControllerDiagonalSpeedTest::RunTest(const FString& Parameters)
{
	// Spec 7's 200 m/s is a speed. Scaled per axis, full stick on two axes asked for 283 m/s and
	// on three for 346; the hand-flown fight of checkpoint 7 reached 276 m/s on a diagonal.
	const StarForge::FShipMassProperties Mass = MakeAsymmetricShip();
	const StarForge::FFlightLimits Limits = MakeLimits();
	const StarForge::FWrench Capability = MakeCapability();
	const double Gain = Mass.Mass * Limits.LinearGain;

	const FVector Diagonals[] = {
		FVector(1.0, 1.0, 0.0), FVector(1.0, -1.0, 1.0), FVector(-0.9, 0.0, 0.8), FVector(2.0, 2.0, 2.0) };

	for (const FVector& Axis : Diagonals)
	{
		StarForge::FFlightIntent Intent;
		Intent.LinearAxis = Axis;

		// From rest the demand is the target velocity times mass times gain, so its length over
		// that product is the speed asked for.
		const StarForge::FRigidBodyState AtRest;
		const StarForge::FWrench FromRest =
			StarForge::ComputeDemandedWrench(Intent, AtRest, Mass, Limits, Capability);
		const FVector Asked = FromRest.Force / Gain;
		TestTrue(*FString::Printf(TEXT("stick (%.1f %.1f %.1f) asks for no more than %.0f m/s (asked %.6f)"),
				Axis.X, Axis.Y, Axis.Z, Limits.MaxSpeed, Asked.Size()),
			Asked.Size() <= Limits.MaxSpeed + 1e-9);

		// A full deflection, however many axes it spans, asks for exactly the limit, and along the
		// stick's own direction once each axis is inside full travel.
		const FVector Clamped(
			FMath::Clamp(Axis.X, -1.0, 1.0), FMath::Clamp(Axis.Y, -1.0, 1.0), FMath::Clamp(Axis.Z, -1.0, 1.0));
		if (Clamped.Size() >= 1.0)
		{
			TestTrue(*FString::Printf(TEXT("stick (%.1f %.1f %.1f) at full deflection asks for exactly %.0f m/s"),
					Axis.X, Axis.Y, Axis.Z, Limits.MaxSpeed),
				FMath::IsNearlyEqual(Asked.Size(), Limits.MaxSpeed, 1e-9));
			TestTrue(TEXT("along the stick's direction"),
				FVector::DotProduct(Asked.GetSafeNormal(), Clamped.GetSafeNormal()) > 1.0 - 1e-12);
		}

		// And a ship already moving at the limit along that direction is asked for nothing: the
		// per-axis form pushed it on towards 283 m/s.
		StarForge::FRigidBodyState AtTheLimit;
		AtTheLimit.LinearVelocity = Clamped.GetClampedToMaxSize(1.0) * Limits.MaxSpeed;
		const StarForge::FWrench Holding =
			StarForge::ComputeDemandedWrench(Intent, AtTheLimit, Mass, Limits, Capability);
		TestTrue(*FString::Printf(TEXT("stick (%.1f %.1f %.1f) at the speed it asked for pushes no further (%.6f N)"),
				Axis.X, Axis.Y, Axis.Z, Holding.Force.Size()),
			Holding.Force.Size() < 1e-6);
	}

	// The limit is the assist's. With assist off the stick asks for thrust, and full stick on two
	// axes is the whole array on both: that stays per axis.
	StarForge::FFlightIntent Unassisted;
	Unassisted.LinearAxis = FVector(1.0, 1.0, 0.0);
	Unassisted.bFlightAssistEnabled = false;
	const StarForge::FWrench Thrust =
		StarForge::ComputeDemandedWrench(Unassisted, StarForge::FRigidBodyState(), Mass, Limits, Capability);
	TestTrue(TEXT("assist off, a diagonal is the whole array on both axes"),
		FMath::IsNearlyEqual(Thrust.Force.X, Capability.Force.X, 1e-9)
		&& FMath::IsNearlyEqual(Thrust.Force.Y, Capability.Force.Y, 1e-9));

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
