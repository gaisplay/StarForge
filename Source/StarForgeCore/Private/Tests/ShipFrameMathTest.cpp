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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameUncompensatedBelowLimitIsZeroTest,
	"StarForge.Core.ShipFrame.UncompensatedBelowLimitIsZero",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameUncompensatedBelowLimitIsZeroTest::RunTest(const FString& Parameters)
{
	// Inside the plates' ceiling nothing is left over: a man under 15 m/s^2 of felt
	// acceleration feels the deck, not a shove.
	TestTrue(TEXT("below the limit nothing is left for him to feel"),
		StarForge::ShipFrame::ComputeUncompensatedAcceleration(FVector(0.0, 0.0, 15.0)).IsNearlyZero(1e-12));

	// And exactly at the limit is still nothing: the plates absorb up to and including it.
	TestTrue(TEXT("at the limit still nothing"),
		StarForge::ShipFrame::ComputeUncompensatedAcceleration(
			FVector(StarForge::ShipFrame::GravPlateLimitMetersPerSecSq, 0.0, 0.0)).IsNearlyZero(1e-9));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameUncompensatedIsTheExcessReversedTest,
	"StarForge.Core.ShipFrame.UncompensatedIsTheExcessReversed",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameUncompensatedIsTheExcessReversedTest::RunTest(const FString& Parameters)
{
	// The spec's own worst case: a full roll with a man four metres from the axis.
	// omega^2 r = pi^2 * 4 = 39.48 m/s^2 of centripetal acceleration, pointing from him
	// toward the axis, which is -X here. The plates absorb 19.62 of it. What is left,
	// 19.86, is what he feels, and he feels it the other way: thrown toward the wall,
	// not pulled toward the axis. A sign error here slides him the wrong way across the
	// deck and nothing crashes.
	const double Centripetal = 4.0 * UE_DOUBLE_PI * UE_DOUBLE_PI;
	const FVector Push = StarForge::ShipFrame::ComputeUncompensatedAcceleration(FVector(-Centripetal, 0.0, 0.0));

	TestTrue(TEXT("the push is the excess over the limit"),
		FMath::IsNearlyEqual(Push.X, Centripetal - StarForge::ShipFrame::GravPlateLimitMetersPerSecSq, 1e-9));
	TestTrue(TEXT("and it points away from the axis"), Push.X > 0.0);
	TestTrue(TEXT("with nothing on the other axes"),
		FMath::IsNearlyZero(Push.Y, 1e-12) && FMath::IsNearlyZero(Push.Z, 1e-12));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameUncompensatedClampsMagnitudeTest,
	"StarForge.Core.ShipFrame.UncompensatedClampsMagnitude",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameUncompensatedClampsMagnitudeTest::RunTest(const FString& Parameters)
{
	// Off-axis on purpose. Every other fixture here points along one axis, where a
	// ceiling applied to the vector's length and a ceiling applied to each component
	// separately give the same answer, so neither would notice the other. They are not
	// the same thing: the plates hold down what a body feels, and what it feels is one
	// acceleration with a magnitude, not three that each get their own allowance.
	//
	// 15 and 15 is 21.21 long, over the 19.62 ceiling, so the man goes down -- while
	// component by component nothing reaches the ceiling at all and he would stay on
	// his feet through a manoeuvre that should have floored him.
	const FVector Push = StarForge::ShipFrame::ComputeUncompensatedAcceleration(FVector(15.0, 15.0, 0.0));

	const double Length = FMath::Sqrt(2.0) * 15.0;
	const double Excess = Length - StarForge::ShipFrame::GravPlateLimitMetersPerSecSq;
	const double PerAxis = -Excess / FMath::Sqrt(2.0);

	TestTrue(TEXT("a diagonal over the ceiling in length is not absorbed"), !Push.IsNearlyZero(1e-9));
	TestTrue(TEXT("the excess is shared between the axes by direction"),
		FMath::IsNearlyEqual(Push.X, PerAxis, 1e-9) && FMath::IsNearlyEqual(Push.Y, PerAxis, 1e-9));
	TestTrue(TEXT("and the push opposes what was felt"), Push.X < 0.0 && Push.Y < 0.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameFootingDropsAtOnceTest,
	"StarForge.Core.ShipFrame.FootingDropsAtOnce",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameFootingDropsAtOnceTest::RunTest(const FString& Parameters)
{
	StarForge::ShipFrame::FFootingState Footing;

	// Standing, and nothing over the limit: standing.
	TestFalse(TEXT("starts on his feet"), Footing.Update(false, 1.0 / 60.0, 0.5));

	// One frame over the limit is enough. There is no delay on the way down; the
	// delay is only on the way back up.
	TestTrue(TEXT("one frame over the limit and he is down"), Footing.Update(true, 1.0 / 60.0, 0.5));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameFootingRecoversAfterTheDelayTest,
	"StarForge.Core.ShipFrame.FootingRecoversAfterTheDelay",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameFootingRecoversAfterTheDelayTest::RunTest(const FString& Parameters)
{
	StarForge::ShipFrame::FFootingState Footing;
	Footing.Update(true, 1.0 / 60.0, 0.5);

	// 29 frames under the limit is 0.483 s: not yet.
	for (int32 i = 0; i < 29; ++i)
	{
		Footing.Update(false, 1.0 / 60.0, 0.5);
	}
	TestTrue(TEXT("still down short of the delay"), Footing.bDown);

	// 31 frames is 0.517 s: up. Not tested at exactly 30, because thirty sixtieths
	// summed in double may land a hair under a half and the test would be a coin toss.
	Footing.Update(false, 1.0 / 60.0, 0.5);
	Footing.Update(false, 1.0 / 60.0, 0.5);
	TestFalse(TEXT("up once the delay has passed"), Footing.bDown);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipFrameFootingRelapseRestartsTheDelayTest,
	"StarForge.Core.ShipFrame.FootingRelapseRestartsTheDelay",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipFrameFootingRelapseRestartsTheDelayTest::RunTest(const FString& Parameters)
{
	StarForge::ShipFrame::FFootingState Footing;
	Footing.Update(true, 1.0 / 60.0, 0.5);

	// Twenty frames of calm, one frame over, twenty more of calm. Forty frames of calm
	// in total would have stood him up; the relapse in the middle has to have reset
	// the clock, or a manoeuvre flickering at the limit stands him up mid-flicker.
	for (int32 i = 0; i < 20; ++i)
	{
		Footing.Update(false, 1.0 / 60.0, 0.5);
	}
	Footing.Update(true, 1.0 / 60.0, 0.5);
	for (int32 i = 0; i < 20; ++i)
	{
		Footing.Update(false, 1.0 / 60.0, 0.5);
	}
	TestTrue(TEXT("still down: the relapse restarted the delay"), Footing.bDown);

	// Eleven more make 31 since the relapse: up.
	for (int32 i = 0; i < 11; ++i)
	{
		Footing.Update(false, 1.0 / 60.0, 0.5);
	}
	TestFalse(TEXT("up once the delay has passed since the relapse"), Footing.bDown);

	return true;
}
