#include "Misc/AutomationTest.h"
#include "ThrusterDef.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrusterWrenchThroughCentreTest,
	"StarForge.Core.Thruster.WrenchThroughCentre",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrusterWrenchThroughCentreTest::RunTest(const FString& Parameters)
{
	// A thruster whose line of action passes through the centre of mass produces
	// pure force and no torque, however far along that line it sits.
	StarForge::FThrusterDef Thruster;
	Thruster.Location = FVector(-3.0, 0.0, 0.0);
	Thruster.Direction = FVector(1.0, 0.0, 0.0);
	Thruster.MaxThrust = 1000.0;

	const StarForge::FWrench Wrench =
		StarForge::ComputeThrusterWrench(Thruster, FVector::ZeroVector);

	TestTrue(TEXT("force is thrust along the thruster's direction"),
		Wrench.Force.Equals(FVector(1000.0, 0.0, 0.0), 1e-9));
	TestTrue(TEXT("a line of action through the centre of mass makes no torque"),
		Wrench.Torque.IsNearlyZero(1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrusterWrenchOffAxisTest,
	"StarForge.Core.Thruster.WrenchOffAxis",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrusterWrenchOffAxisTest::RunTest(const FString& Parameters)
{
	// Two metres out along +Y, pushing along +X. The lever arm is r = (0,2,0) and
	// the force F = (500,0,0), so r x F = (0*0 - 0*0, 0*500 - 0*0, 0*0 - 2*500)
	// = (0, 0, -1000) newton-metres: it yaws the ship nose-right.
	StarForge::FThrusterDef Thruster;
	Thruster.Location = FVector(0.0, 2.0, 0.0);
	Thruster.Direction = FVector(1.0, 0.0, 0.0);
	Thruster.MaxThrust = 500.0;

	const StarForge::FWrench Wrench =
		StarForge::ComputeThrusterWrench(Thruster, FVector::ZeroVector);

	TestTrue(TEXT("force is unchanged by the offset"),
		Wrench.Force.Equals(FVector(500.0, 0.0, 0.0), 1e-9));
	TestTrue(TEXT("torque is the lever arm crossed into the force"),
		Wrench.Torque.Equals(FVector(0.0, 0.0, -1000.0), 1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrusterWrenchRespectsCentreOfMassTest,
	"StarForge.Core.Thruster.WrenchRespectsCentreOfMass",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrusterWrenchRespectsCentreOfMassTest::RunTest(const FString& Parameters)
{
	// Same thruster as above, but the centre of mass has moved onto its line of
	// action. The torque must vanish -- the lever arm is measured from the centre
	// of mass, not from the origin, and a ship whose load shifts must feel that.
	StarForge::FThrusterDef Thruster;
	Thruster.Location = FVector(0.0, 2.0, 0.0);
	Thruster.Direction = FVector(1.0, 0.0, 0.0);
	Thruster.MaxThrust = 500.0;

	const StarForge::FWrench Wrench =
		StarForge::ComputeThrusterWrench(Thruster, FVector(0.0, 2.0, 0.0));

	TestTrue(TEXT("moving the centre of mass onto the line of action kills the torque"),
		Wrench.Torque.IsNearlyZero(1e-9));

	return true;
}

#include "ThrustAllocator.h"

namespace
{
	/**
	 * A symmetric twenty-four thruster array: four positions around the hull, each
	 * carrying an opposed pair along every axis.
	 *
	 * The opposed pairs are what make this array able to produce a pure torque at
	 * all. Throttles are physically confined to [0, 1] -- a thruster cannot run
	 * backwards -- so an array carrying only +X, +Y and +Z thrusters can produce a
	 * yaw moment only by also pushing the ship sideways: zeroing the net force
	 * forces every contributing throttle to zero and takes the moment with it. Only
	 * a force couple, which needs two thrusters pointing opposite ways, turns the
	 * ship without shoving it.
	 *
	 * Symmetry is also what makes the parasitic checks meaningful -- an asymmetric
	 * layout has honest cross-coupling and would fail them for the right reasons.
	 */
	TArray<StarForge::FThrusterDef> MakeSymmetricArray()
	{
		TArray<StarForge::FThrusterDef> Thrusters;

		const FVector Corners[4] = {
			FVector( 2.0,  2.0, 0.0),
			FVector( 2.0, -2.0, 0.0),
			FVector(-2.0,  2.0, 0.0),
			FVector(-2.0, -2.0, 0.0)
		};

		const FVector Axes[6] = {
			FVector( 1.0,  0.0,  0.0),
			FVector(-1.0,  0.0,  0.0),
			FVector( 0.0,  1.0,  0.0),
			FVector( 0.0, -1.0,  0.0),
			FVector( 0.0,  0.0,  1.0),
			FVector( 0.0,  0.0, -1.0)
		};

		for (const FVector& Corner : Corners)
		{
			for (const FVector& Axis : Axes)
			{
				StarForge::FThrusterDef Thruster;
				Thruster.Location = Corner;
				Thruster.Direction = Axis;
				Thruster.MaxThrust = 1000.0;
				Thrusters.Add(Thruster);
			}
		}

		return Thrusters;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrustAllocatorPureForceTest,
	"StarForge.Core.ThrustAllocator.PureForceMakesNoTorque",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrustAllocatorPureForceTest::RunTest(const FString& Parameters)
{
	StarForge::FThrustAllocator Allocator;
	TestTrue(TEXT("a symmetric array builds"),
		Allocator.Build(MakeSymmetricArray(), FVector::ZeroVector));

	StarForge::FWrench Desired;
	Desired.Force = FVector(2000.0, 0.0, 0.0);

	TArray<double> Throttles;
	Allocator.Solve(Desired, Throttles);

	const StarForge::FWrench Achieved = Allocator.ComputeAchieved(Throttles);

	TestTrue(TEXT("the requested force is delivered"),
		Achieved.Force.Equals(Desired.Force, 1.0));

	// Parasitic torque is judged against what this array could produce on the same
	// axis at full throttle, not against an absolute number -- a bigger ship would
	// otherwise pass or fail the same test for the wrong reason.
	for (int32 Axis = 3; Axis < 6; ++Axis)
	{
		const double Budget = 0.01 * Allocator.GetAxisCapability(Axis);
		TestTrue(*FString::Printf(TEXT("parasitic torque on axis %d is within one percent"), Axis),
			FMath::Abs(Achieved.Component(Axis)) <= Budget);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrustAllocatorPureTorqueTest,
	"StarForge.Core.ThrustAllocator.PureTorqueMakesNoForce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrustAllocatorPureTorqueTest::RunTest(const FString& Parameters)
{
	StarForge::FThrustAllocator Allocator;
	TestTrue(TEXT("a symmetric array builds"),
		Allocator.Build(MakeSymmetricArray(), FVector::ZeroVector));

	StarForge::FWrench Desired;
	Desired.Torque = FVector(0.0, 0.0, 1000.0);

	TArray<double> Throttles;
	Allocator.Solve(Desired, Throttles);

	const StarForge::FWrench Achieved = Allocator.ComputeAchieved(Throttles);

	TestTrue(TEXT("the requested torque is delivered"),
		Achieved.Torque.Equals(Desired.Torque, 1.0));

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const double Budget = 0.01 * Allocator.GetAxisCapability(Axis);
		TestTrue(*FString::Printf(TEXT("parasitic force on axis %d is within one percent"), Axis),
			FMath::Abs(Achieved.Component(Axis)) <= Budget);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrustAllocatorSaturationKeepsDirectionTest,
	"StarForge.Core.ThrustAllocator.SaturationKeepsDirection",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrustAllocatorSaturationKeepsDirectionTest::RunTest(const FString& Parameters)
{
	StarForge::FThrustAllocator Allocator;
	TestTrue(TEXT("a symmetric array builds"),
		Allocator.Build(MakeSymmetricArray(), FVector::ZeroVector));

	// Ask for far more forward force than the array can ever deliver.
	StarForge::FWrench Desired;
	Desired.Force = FVector(1.0e9, 0.0, 0.0);

	TArray<double> Throttles;
	Allocator.Solve(Desired, Throttles);

	for (double Throttle : Throttles)
	{
		TestTrue(TEXT("every throttle stays inside its physical range"),
			Throttle >= -1e-9 && Throttle <= 1.0 + 1e-9);
	}

	const StarForge::FWrench Achieved = Allocator.ComputeAchieved(Throttles);

	TestTrue(TEXT("the ship still pushes forward"), Achieved.Force.X > 0.0);
	TestTrue(TEXT("an impossible request must not push the ship sideways"),
		FMath::Abs(Achieved.Force.Y) <= 0.01 * FMath::Abs(Achieved.Force.X));
	TestTrue(TEXT("an impossible request must not push the ship up"),
		FMath::Abs(Achieved.Force.Z) <= 0.01 * FMath::Abs(Achieved.Force.X));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrustAllocatorSurvivesThrusterLossTest,
	"StarForge.Core.ThrustAllocator.SurvivesThrusterLoss",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrustAllocatorSurvivesThrusterLossTest::RunTest(const FString& Parameters)
{
	// Rebuild without one thruster, as the ship will when one is shot away. The
	// array must still solve, and the degraded handling that follows is a
	// consequence of the geometry rather than anything written for the occasion.
	TArray<StarForge::FThrusterDef> Damaged = MakeSymmetricArray();
	Damaged.RemoveAt(0);

	StarForge::FThrustAllocator Allocator;
	TestTrue(TEXT("a damaged array still builds"),
		Allocator.Build(Damaged, FVector::ZeroVector));

	StarForge::FWrench Desired;
	Desired.Force = FVector(2000.0, 0.0, 0.0);

	TArray<double> Throttles;
	Allocator.Solve(Desired, Throttles);

	TestEqual(TEXT("one throttle per surviving thruster"), Throttles.Num(), Damaged.Num());

	for (double Throttle : Throttles)
	{
		TestTrue(TEXT("every throttle stays inside its physical range"),
			Throttle >= -1e-9 && Throttle <= 1.0 + 1e-9);
	}

	const StarForge::FWrench Achieved = Allocator.ComputeAchieved(Throttles);
	TestTrue(TEXT("the ship still makes most of the requested forward force"),
		Achieved.Force.X > 0.5 * Desired.Force.X);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FThrustAllocatorRejectsDegenerateArrayTest,
	"StarForge.Core.ThrustAllocator.RejectsDegenerateArray",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FThrustAllocatorRejectsDegenerateArrayTest::RunTest(const FString& Parameters)
{
	// One thruster cannot span six axes. Building must fail loudly: a ship that
	// silently refuses to turn is far harder to diagnose than one that refuses to
	// finish spawning.
	TArray<StarForge::FThrusterDef> Single;
	StarForge::FThrusterDef Thruster;
	Thruster.Location = FVector::ZeroVector;
	Thruster.Direction = FVector(1.0, 0.0, 0.0);
	Thruster.MaxThrust = 1000.0;
	Single.Add(Thruster);

	StarForge::FThrustAllocator Allocator;
	TestFalse(TEXT("an array that cannot span six axes is rejected"),
		Allocator.Build(Single, FVector::ZeroVector));
	TestFalse(TEXT("and reports itself unbuilt"), Allocator.IsBuilt());

	return true;
}
