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
