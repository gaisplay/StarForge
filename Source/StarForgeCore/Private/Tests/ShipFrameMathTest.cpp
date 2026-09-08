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
	// Spinning at 2 rad/s about Z, an occupant 5 m out along X undergoes
	// omega^2 * r = 20 m/s^2 of centripetal acceleration, directed back toward
	// the spin axis: -20 along X. What he subjectively feels is the opposite,
	// outward direction; only the magnitude matters to the grav-plate check.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameLookYawTurnsAboutDeckTest,
	"StarForge.Core.ShipFrame.LookYawTurnsAboutDeck",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameLookYawTurnsAboutDeckTest::RunTest(const FString& Parameters)
{
	// Level deck, view along +X. A quarter turn to the right must swing the view
	// onto +Y and leave it level with the deck.
	const FQuat Result = StarForge::ShipFrame::ApplyLookStep(
		FQuat::Identity, FVector::UpVector, 90.0, 0.0);

	TestTrue(TEXT("a right turn swings the view onto +Y"),
		Result.GetForwardVector().Equals(FVector(0.0, 1.0, 0.0), 1e-6));
	TestTrue(TEXT("and leaves it level with the deck"),
		FMath::IsNearlyZero(FVector::DotProduct(Result.GetForwardVector(), FVector::UpVector), 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameLookPitchRaisesTowardDeckUpTest,
	"StarForge.Core.ShipFrame.LookPitchRaisesTowardDeckUp",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameLookPitchRaisesTowardDeckUpTest::RunTest(const FString& Parameters)
{
	// Positive pitch means looking up, and up is defined by the deck.
	const FQuat Result = StarForge::ShipFrame::ApplyLookStep(
		FQuat::Identity, FVector::UpVector, 0.0, 45.0);

	const FVector Forward = Result.GetForwardVector();

	TestTrue(TEXT("a positive pitch step raises the view toward the deck normal"),
		FVector::DotProduct(Forward, FVector::UpVector) > 0.5);
	TestTrue(TEXT("and does not swing it sideways"),
		FMath::IsNearlyZero(Forward.Y, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameLookPitchOnRolledDeckTest,
	"StarForge.Core.ShipFrame.LookPitchOnRolledDeck",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameLookPitchOnRolledDeckTest::RunTest(const FString& Parameters)
{
	// The defect this test exists for: look input applied in world axes instead of
	// the deck's. With the ship rolled ninety degrees the world pitch axis is the
	// occupant's own up-down axis, so "look up" rolled or yawed him instead of
	// pitching him -- and because the rig rolls continuously at 180 deg/s, the axis
	// kept precessing and the view swept a circle.
	//
	// Ship rolled +90 about its X: the deck normal that was +Z now points along -Y,
	// and the occupant's right, which was +Y, now points along +Z.
	const FQuat Rolled = FQuat(FVector::ForwardVector, FMath::DegreesToRadians(90.0));
	const FVector DeckUp = Rolled.RotateVector(FVector::UpVector);
	const FVector RightBefore = Rolled.GetRightVector();

	const FQuat Result = StarForge::ShipFrame::ApplyLookStep(Rolled, DeckUp, 0.0, 30.0);

	const FVector Forward = Result.GetForwardVector();

	TestTrue(TEXT("pitch raises the view toward the deck normal, not the world's"),
		FVector::DotProduct(Forward, DeckUp) > 0.4);
	TestTrue(TEXT("pitch rotates about the view's own right axis, leaving it fixed"),
		Result.GetRightVector().Equals(RightBefore, 1e-6));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameLookPitchStopsAtZenithTest,
	"StarForge.Core.ShipFrame.LookPitchStopsAtZenith",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameLookPitchStopsAtZenithTest::RunTest(const FString& Parameters)
{
	// Asking for far more pitch than exists must stop the view at the zenith rather
	// than tumbling it over the top. The limit is measured against the deck, not the
	// world horizon -- an occupant on a rolling ship has no world horizon, which is
	// why the engine's own pitch clamp is deliberately lifted.
	const FQuat Up = StarForge::ShipFrame::ApplyLookStep(
		FQuat::Identity, FVector::UpVector, 0.0, 200.0);
	const FQuat Down = StarForge::ShipFrame::ApplyLookStep(
		FQuat::Identity, FVector::UpVector, 0.0, -200.0);

	const double UpAngle = FMath::RadiansToDegrees(
		FMath::Asin(FVector::DotProduct(Up.GetForwardVector(), FVector::UpVector)));
	const double DownAngle = FMath::RadiansToDegrees(
		FMath::Asin(FVector::DotProduct(Down.GetForwardVector(), FVector::UpVector)));

	TestTrue(TEXT("looking up stops just short of straight up"),
		UpAngle > 80.0 && UpAngle <= 89.0 + 1e-6);
	TestTrue(TEXT("looking down stops just short of straight down"),
		DownAngle < -80.0 && DownAngle >= -89.0 - 1e-6);

	return true;
}
