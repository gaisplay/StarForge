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
	// pivot must end up one metre to the ship's right, since a +90 degree yaw sends
	// local forward (+X) onto world +Y.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameLocalToWorldTest,
	"StarForge.Core.ShipFrame.LocalToWorld",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameLocalToWorldTest::RunTest(const FString& Parameters)
{
	// Ship sits 10 m out along Y, yawed 90 degrees. A point 2 m ahead of the ship's
	// nose is therefore 2 m further along world Y.
	const FTransform ShipWorld(FRotator(0.0, 90.0, 0.0).Quaternion(), FVector(0.0, 1000.0, 0.0));
	const FTransform OccupantLocal(FQuat::Identity, FVector(200.0, 0.0, 0.0));

	const FTransform World = StarForge::ShipFrame::LocalToWorld(ShipWorld, OccupantLocal);

	TestTrue(TEXT("local offset is rotated by the ship and added to its position"),
		World.GetLocation().Equals(FVector(0.0, 1200.0, 0.0), 0.01));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameCentripetalTest,
	"StarForge.Core.ShipFrame.Centripetal",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameCentripetalTest::RunTest(const FString& Parameters)
{
	// Spinning at 2 rad/s about Z, an occupant 5 m out along X feels
	// omega^2 * r = 20 m/s^2 pulling him outward, i.e. -20 along X.
	const FVector NoLinearAccel = FVector::ZeroVector;
	const FVector Spin(0.0, 0.0, 2.0);
	const FVector Offset(5.0, 0.0, 0.0);

	const FVector Felt =
		StarForge::ShipFrame::ComputeFeltAcceleration(NoLinearAccel, Spin, Offset);

	TestTrue(TEXT("centripetal term points back toward the axis"),
		Felt.Equals(FVector(-20.0, 0.0, 0.0), 0.001));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameGravPlateLimitTest,
	"StarForge.Core.ShipFrame.GravPlateLimit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameGravPlateLimitTest::RunTest(const FString& Parameters)
{
	// The design fixes the compensation ceiling at 2 g. Just under it the
	// occupant keeps his footing; just over it he does not.
	TestFalse(TEXT("19.0 m/s^2 stays within compensation"),
		StarForge::ShipFrame::ExceedsGravPlateLimit(FVector(19.0, 0.0, 0.0)));

	TestTrue(TEXT("20.0 m/s^2 exceeds compensation"),
		StarForge::ShipFrame::ExceedsGravPlateLimit(FVector(20.0, 0.0, 0.0)));

	return true;
}
