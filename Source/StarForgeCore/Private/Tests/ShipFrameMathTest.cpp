#include "Misc/AutomationTest.h"
#include "ShipFrameMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameCarryTranslationTest,
	"StarForge.Core.ShipFrame.CarryTranslation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameCarryTranslationTest::RunTest(const FString& Parameters)
{
	const FTransform PrevShip(FQuat::Identity, FVector(0.0, 0.0, 0.0));
	const FTransform CurrShip(FQuat::Identity, FVector(1000.0, 0.0, 0.0));
	const FTransform Occupant(FQuat::Identity, FVector(50.0, 0.0, 0.0));

	const FTransform Carried =
		StarForge::ShipFrame::CarryOccupant(PrevShip, CurrShip, Occupant);

	TestTrue(TEXT("occupant travels with the ship's translation"),
		Carried.GetLocation().Equals(FVector(1050.0, 0.0, 0.0), 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameCarryRotationTest,
	"StarForge.Core.ShipFrame.CarryRotation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameCarryRotationTest::RunTest(const FString& Parameters)
{
	// Ship yaws 90 degrees about its own pivot; an occupant one metre ahead of the
	// pivot must end up one metre to the left of it.
	const FTransform PrevShip(FQuat::Identity, FVector::ZeroVector);
	const FTransform CurrShip(FRotator(0.0, 90.0, 0.0).Quaternion(), FVector::ZeroVector);
	const FTransform Occupant(FQuat::Identity, FVector(100.0, 0.0, 0.0));

	const FTransform Carried =
		StarForge::ShipFrame::CarryOccupant(PrevShip, CurrShip, Occupant);

	TestTrue(TEXT("occupant is swept around the ship's pivot"),
		Carried.GetLocation().Equals(FVector(0.0, 100.0, 0.0), 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameLocalInvarianceTest,
	"StarForge.Core.ShipFrame.LocalInvariance",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameLocalInvarianceTest::RunTest(const FString& Parameters)
{
	// The whole point of the carry: whatever the ship does, the occupant's
	// ship-local transform must come out unchanged.
	const FTransform PrevShip(FRotator(10.0, 20.0, 30.0).Quaternion(), FVector(500.0, -200.0, 75.0));
	const FTransform CurrShip(FRotator(-40.0, 155.0, 90.0).Quaternion(), FVector(-9000.0, 4000.0, 120.0));
	const FTransform Occupant(FRotator(0.0, 45.0, 0.0).Quaternion(), FVector(560.0, -150.0, 160.0));

	const FTransform LocalBefore = StarForge::ShipFrame::WorldToLocal(PrevShip, Occupant);
	const FTransform Carried = StarForge::ShipFrame::CarryOccupant(PrevShip, CurrShip, Occupant);
	const FTransform LocalAfter = StarForge::ShipFrame::WorldToLocal(CurrShip, Carried);

	TestTrue(TEXT("ship-local position is preserved"),
		LocalAfter.GetLocation().Equals(LocalBefore.GetLocation(), 0.01));
	TestTrue(TEXT("ship-local rotation is preserved"),
		LocalAfter.GetRotation().Equals(LocalBefore.GetRotation(), 0.0001));

	return true;
}
