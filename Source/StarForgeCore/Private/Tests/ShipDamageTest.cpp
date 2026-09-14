#include "Misc/AutomationTest.h"
#include "ShipDamage.h"

#include <limits>

namespace
{
	using namespace StarForge::Damage;

	/** The rig's hull: an 8 m cube, so the half-extents are 400 cm on every axis. */
	const FVector HullHalf(400.0, 400.0, 400.0);

	FShieldLimits MakeLimits()
	{
		FShieldLimits Limits;
		Limits.FaceCapacity = 1000.0;
		Limits.RegenPerSecond = 50.0;
		Limits.RegenDelaySeconds = 3.0;
		return Limits;
	}

	/** Every face full, nothing hit yet: a long time since, so regen is not gated. */
	FShieldState MakeFullShields()
	{
		FShieldState State;
		for (int32 i = 0; i < FaceCount; ++i)
		{
			State.Charge[i] = 1000.0;
			State.SecondsSinceHit[i] = 1000.0;
		}
		return State;
	}

	/** Every field equal, exactly. A NaN written anywhere compares unequal and fails it. */
	bool IsSameShieldState(const FShieldState& A, const FShieldState& B)
	{
		for (int32 i = 0; i < FaceCount; ++i)
		{
			if (!(A.Charge[i] == B.Charge[i]) || !(A.SecondsSinceHit[i] == B.SecondsSinceHit[i]))
			{
				return false;
			}
		}
		return true;
	}

	int32 IndexOf(EShipFace Face)
	{
		return static_cast<int32>(Face);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipDamageEachFaceIsItsOwnTest,
	"StarForge.Core.Damage.EachFaceIsItsOwn",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipDamageEachFaceIsItsOwnTest::RunTest(const FString& Parameters)
{
	// A swapped sign or a swapped axis does not look like a defect: hits on the nose drain the
	// stern, a number moves on every hit, and nobody notices until they start counting. Each hit
	// below lands 10 cm outside one face, is classified, is applied to a fresh set of full
	// shields, and has to drain that face and no other.
	struct FCase
	{
		FVector Point;
		EShipFace Face;
		const TCHAR* Name;
	};
	const FCase Cases[] =
	{
		{ FVector(410.0, 0.0, 0.0), EShipFace::XPos, TEXT("+X") },
		{ FVector(-410.0, 0.0, 0.0), EShipFace::XNeg, TEXT("-X") },
		{ FVector(0.0, 410.0, 0.0), EShipFace::YPos, TEXT("+Y") },
		{ FVector(0.0, -410.0, 0.0), EShipFace::YNeg, TEXT("-Y") },
		{ FVector(0.0, 0.0, 410.0), EShipFace::ZPos, TEXT("+Z") },
		{ FVector(0.0, 0.0, -410.0), EShipFace::ZNeg, TEXT("-Z") },
	};

	for (const FCase& Case : Cases)
	{
		const FString Where = FString::Printf(TEXT("the hit at (%.0f %.0f %.0f)"), Case.Point.X, Case.Point.Y, Case.Point.Z);

		// Started on a face other than the expected one, so a classification that reported
		// success without writing cannot pass.
		EShipFace Face = (Case.Face == EShipFace::ZNeg) ? EShipFace::XPos : EShipFace::ZNeg;
		TestTrue(Where + TEXT(" is classified"), ClassifyFace(Case.Point, HullHalf, Face));
		TestTrue(Where + TEXT(" belongs to ") + Case.Name, Face == Case.Face);
		TestTrue(Where + TEXT(" names its face ") + Case.Name, FCString::Strcmp(FaceName(Face), Case.Name) == 0);

		FShieldState State = MakeFullShields();
		FHitOutcome Outcome;
		TestTrue(Where + TEXT(" is applied"), ApplyHit(State, MakeLimits(), Face, 100.0, Outcome));
		TestTrue(Where + TEXT(" reports its face"), Outcome.Face == Case.Face);

		for (int32 i = 0; i < FaceCount; ++i)
		{
			const double Expected = (i == IndexOf(Case.Face)) ? 900.0 : 1000.0;
			TestEqual(Where + FString::Printf(TEXT(" leaves face %s at %.0f"), FaceName(static_cast<EShipFace>(i)), Expected),
				State.Charge[i], Expected, 0.0);
		}
	}

	// Normalised by each axis's half-extent, not compared raw. On a hull 16 m long and 4 m wide
	// the point (500, 190, 0) cm is 0.625 of the half-length out and 0.95 of the half-width: it
	// is on the side, and a raw comparison of 500 against 190 would put it on the bow.
	const FVector LongHull(800.0, 200.0, 200.0);
	EShipFace LongFace = EShipFace::ZNeg;
	TestTrue(TEXT("a hit on a long hull is classified"), ClassifyFace(FVector(500.0, 190.0, 0.0), LongHull, LongFace));
	TestTrue(TEXT("and belongs to the side it is nearer in proportion, +Y"), LongFace == EShipFace::YPos);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipDamageTiesResolveXBeforeYBeforeZTest,
	"StarForge.Core.Damage.TiesResolveXBeforeYBeforeZ",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipDamageTiesResolveXBeforeYBeforeZTest::RunTest(const FString& Parameters)
{
	// On an edge of the cube two normalised coordinates are not nearly equal but equal: 410/400
	// is one and the same double on both axes. Without a stated rule the face is whichever
	// comparison happens to be written first, and either answer looks right.
	struct FCase
	{
		FVector Point;
		EShipFace Face;
		const TCHAR* What;
	};
	const FCase Cases[] =
	{
		{ FVector(410.0, 410.0, 0.0), EShipFace::XPos, TEXT("the +X/+Y edge goes to +X") },
		{ FVector(410.0, 410.0, 410.0), EShipFace::XPos, TEXT("the +X/+Y/+Z corner goes to +X") },
		{ FVector(0.0, 410.0, 410.0), EShipFace::YPos, TEXT("the +Y/+Z edge goes to +Y") },
		{ FVector(-410.0, 410.0, 0.0), EShipFace::XNeg, TEXT("the -X/+Y edge goes to -X, the sign of the winning axis") },

		// The exact centre ties at zero on all three and, being non-negative on X, reads +X.
		{ FVector(0.0, 0.0, 0.0), EShipFace::XPos, TEXT("the centre goes to +X") },
	};

	for (const FCase& Case : Cases)
	{
		EShipFace Face = EShipFace::ZNeg;
		TestTrue(FString(Case.What) + TEXT(": classified"), ClassifyFace(Case.Point, HullHalf, Face));
		TestTrue(Case.What, Face == Case.Face);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipDamageOverflowGoesToHullExactlyTest,
	"StarForge.Core.Damage.OverflowGoesToHullExactly",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipDamageOverflowGoesToHullExactlyTest::RunTest(const FString& Parameters)
{
	// Exact, with a tolerance of zero: the absorbed amount is one of its two operands, and
	// 1600 - 1000 has no rounding in it.
	FShieldState State = MakeFullShields();
	const FShieldLimits Limits = MakeLimits();

	FHitOutcome First;
	TestTrue(TEXT("a hit larger than the face is applied"), ApplyHit(State, Limits, EShipFace::XPos, 1600.0, First));
	TestTrue(TEXT("and reports its face"), First.Face == EShipFace::XPos);
	TestEqual(TEXT("the face absorbs all it holds"), First.AbsorbedByShield, 1000.0, 0.0);
	TestEqual(TEXT("and the rest goes to the hull"), First.PassedToHull, 600.0, 0.0);
	TestEqual(TEXT("and the face is left at zero"), State.Charge[IndexOf(EShipFace::XPos)], 0.0, 0.0);
	TestTrue(TEXT("and that hit broke it"), First.bFaceBrokenNow);

	// "Broken now" is an event. The game writes a line for it, and a second hit on a face that
	// is already down must not write that line again.
	FHitOutcome Second;
	TestTrue(TEXT("a hit on the broken face is applied"), ApplyHit(State, Limits, EShipFace::XPos, 100.0, Second));
	TestEqual(TEXT("a broken face absorbs nothing"), Second.AbsorbedByShield, 0.0, 0.0);
	TestEqual(TEXT("and all of it goes to the hull"), Second.PassedToHull, 100.0, 0.0);
	TestFalse(TEXT("and that hit did not break it again"), Second.bFaceBrokenNow);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipDamageRegenWaitsForTheDelayTest,
	"StarForge.Core.Damage.RegenWaitsForTheDelay",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipDamageRegenWaitsForTheDelayTest::RunTest(const FString& Parameters)
{
	FShieldState State = MakeFullShields();
	const FShieldLimits Limits = MakeLimits();
	const int32 Face = IndexOf(EShipFace::ZPos);

	FHitOutcome Hit;
	TestTrue(TEXT("the hit is applied"), ApplyHit(State, Limits, EShipFace::ZPos, 500.0, Hit));
	TestEqual(TEXT("the face is down to 500"), State.Charge[Face], 500.0, 0.0);

	TickShields(State, Limits, 2.9);
	TestEqual(TEXT("2.9 s after the hit the face has not regenerated at all"), State.Charge[Face], 500.0, 0.0);

	// 2.9 + 0.2 is 3.1 only to the nearest double, so the 0.1 s past the delay is not exactly
	// 0.1. The tolerance is 1e-9 on a gain of 5 points; a step that regenerated the whole 0.2 s
	// would gain 10, and one that waited for its start to pass the delay would gain nothing.
	TickShields(State, Limits, 0.2);
	TestEqual(TEXT("3.1 s after the hit it has regenerated for the 0.1 s past the delay"),
		State.Charge[Face], 500.0 + 50.0 * 0.1, 1e-9);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipDamageRegenIsFrameRateIndependentTest,
	"StarForge.Core.Damage.RegenIsFrameRateIndependent",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipDamageRegenIsFrameRateIndependentTest::RunTest(const FString& Parameters)
{
	// Integrated regeneration against the form that tests the timer after adding the step. That
	// form gives one ten-second step all ten seconds (500 points) and a thousand hundredth-second
	// steps seven (350): forty-three per cent, decided by the frame rate. Integrated, both are
	// seven seconds, 350 points. The same form also fails RegenWaitsForTheDelay and
	// AHitResetsOnlyItsOwnFaceTimer, where one step crosses the delay; this is the test that
	// shows it as a frame rate.
	//
	// Not equality: 0.01 is not representable in binary, and the thousand steps were measured at
	// 4.4e-12 below the single step. The tolerance is 1e-9, some two hundred times that and
	// eleven orders of magnitude under the 150 points the other form puts between them.
	const FShieldLimits Limits = MakeLimits();
	const int32 Face = IndexOf(EShipFace::YNeg);

	FShieldState OneStep = MakeFullShields();
	FHitOutcome Hit;
	TestTrue(TEXT("the face is broken"), ApplyHit(OneStep, Limits, EShipFace::YNeg, 1000.0, Hit));
	FShieldState ManySteps = OneStep;

	TickShields(OneStep, Limits, 10.0);
	for (int32 i = 0; i < 1000; ++i)
	{
		TickShields(ManySteps, Limits, 0.01);
	}

	TestEqual(TEXT("one ten-second step regenerates for the seven seconds past the delay"),
		OneStep.Charge[Face], 350.0, 1e-9);
	TestEqual(TEXT("and a thousand hundredth-second steps come to the same charge"),
		ManySteps.Charge[Face], OneStep.Charge[Face], 1e-9);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipDamageRegenStopsAtCapacityTest,
	"StarForge.Core.Damage.RegenStopsAtCapacity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipDamageRegenStopsAtCapacityTest::RunTest(const FString& Parameters)
{
	FShieldState State = MakeFullShields();
	const FShieldLimits Limits = MakeLimits();
	const int32 Topped = IndexOf(EShipFace::XPos);
	const int32 Neighbour = IndexOf(EShipFace::XNeg);

	// +X is 10 points short and its delay has run out; -X is 500 short and hit just now.
	FHitOutcome Hit;
	TestTrue(TEXT("the first hit is applied"), ApplyHit(State, Limits, EShipFace::XPos, 10.0, Hit));
	TickShields(State, Limits, 3.0);
	TestTrue(TEXT("the second hit is applied"), ApplyHit(State, Limits, EShipFace::XNeg, 500.0, Hit));

	// 2.9 s at 50 per second is 145 points into a face with room for 10. The 135 over are lost:
	// not kept on +X above capacity, and not handed to -X, which is still inside its delay.
	TickShields(State, Limits, 2.9);
	TestEqual(TEXT("+X stops at capacity"), State.Charge[Topped], 1000.0, 0.0);
	TestEqual(TEXT("and none of the overflow reaches -X"), State.Charge[Neighbour], 500.0, 0.0);

	// And a very long step leaves every face at capacity exactly, full faces included.
	TickShields(State, Limits, 1000.0);
	for (int32 i = 0; i < FaceCount; ++i)
	{
		TestEqual(FString::Printf(TEXT("after a long step face %s is exactly at capacity"), FaceName(static_cast<EShipFace>(i))),
			State.Charge[i], 1000.0, 0.0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipDamageAHitResetsOnlyItsOwnFaceTimerTest,
	"StarForge.Core.Damage.AHitResetsOnlyItsOwnFaceTimer",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipDamageAHitResetsOnlyItsOwnFaceTimerTest::RunTest(const FString& Parameters)
{
	// Six timers, not one. With one, steady fire on the +Y side would hold every face's
	// regeneration off, and that reads as "shields do not regenerate" with no side to it.
	FShieldState State = MakeFullShields();
	const FShieldLimits Limits = MakeLimits();
	const int32 Waiting = IndexOf(EShipFace::XNeg);
	const int32 Struck = IndexOf(EShipFace::YPos);

	FHitOutcome Hit;
	TestTrue(TEXT("-X is hit"), ApplyHit(State, Limits, EShipFace::XNeg, 500.0, Hit));
	TickShields(State, Limits, 2.0);
	TestTrue(TEXT("+Y is hit two seconds later"), ApplyHit(State, Limits, EShipFace::YPos, 100.0, Hit));

	// After this step -X is four seconds from its hit, one past the delay; +Y is two, still
	// inside it.
	TickShields(State, Limits, 2.0);
	TestEqual(TEXT("-X's timer was not restarted by the hit on +Y"), State.SecondsSinceHit[Waiting], 4.0, 0.0);
	TestEqual(TEXT("so -X has regenerated for one second"), State.Charge[Waiting], 550.0, 0.0);
	TestEqual(TEXT("+Y's timer counts from its own hit"), State.SecondsSinceHit[Struck], 2.0, 0.0);
	TestEqual(TEXT("so +Y has not regenerated"), State.Charge[Struck], 900.0, 0.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShipDamageNonFiniteIsRefusedTest,
	"StarForge.Core.Damage.NonFiniteIsRefused",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FShipDamageNonFiniteIsRefusedTest::RunTest(const FString& Parameters)
{
	// Guards written as the negation of "valid", as in Ballistics and DockingMath: a NaN compares
	// false against every plain comparison, so a guard written the obvious way round lets it
	// through.
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const double Inf = std::numeric_limits<double>::infinity();

	// The fixture before the behaviour: a NaN that a compiler flag had quietly made finite would
	// make every refusal below pass for the wrong reason.
	TestTrue(TEXT("the NaN really is not finite"), !FMath::IsFinite(NaN));
	TestTrue(TEXT("and the infinity really is not finite"), !FMath::IsFinite(Inf));

	// Each fixture below is wrong in one respect only, so one guard stands in its way, and each
	// has its own sentinel, so a missing guard fails at its own fixture and not at every check
	// after it.

	// ClassifyFace. The sentinel is -Y, a face none of these fixtures would be classified as with
	// its guard gone.
	const auto ExpectClassifyRefused = [this](const TCHAR* What, const FVector& Point, const FVector& Half)
	{
		EShipFace Face = EShipFace::YNeg;
		TestFalse(FString(What) + TEXT(" is refused"), ClassifyFace(Point, Half, Face));
		TestTrue(FString(What) + TEXT(" leaves the face untouched"), Face == EShipFace::YNeg);
	};

	ExpectClassifyRefused(TEXT("a NaN in the point"), FVector(NaN, 0.0, 0.0), HullHalf);
	ExpectClassifyRefused(TEXT("an infinity in the point"), FVector(0.0, Inf, 0.0), HullHalf);
	ExpectClassifyRefused(TEXT("a minus infinity in the point"), FVector(0.0, 0.0, -Inf), HullHalf);
	ExpectClassifyRefused(TEXT("a zero half-extent"), FVector(410.0, 0.0, 0.0), FVector(0.0, 400.0, 400.0));
	ExpectClassifyRefused(TEXT("a negative half-extent"), FVector(0.0, 410.0, 0.0), FVector(400.0, -400.0, 400.0));
	ExpectClassifyRefused(TEXT("a NaN half-extent"), FVector(0.0, 0.0, 410.0), FVector(400.0, 400.0, NaN));

	// ApplyHit, on a face that is part-drained and past its delay, so a write to its charge or
	// its timer would show.
	const FShieldLimits Limits = MakeLimits();
	FShieldState Drained = MakeFullShields();
	for (int32 i = 0; i < FaceCount; ++i)
	{
		Drained.Charge[i] = 400.0;
		Drained.SecondsSinceHit[i] = 5.0;
	}

	const auto ExpectHitRefused = [this, &Drained, &Limits](const TCHAR* What, double Damage)
	{
		FShieldState State = Drained;
		FHitOutcome Out;
		Out.Face = EShipFace::YNeg;
		Out.AbsorbedByShield = 7.0;
		Out.PassedToHull = 7.0;
		Out.bFaceBrokenNow = true;

		TestFalse(FString(What) + TEXT(" is refused"), ApplyHit(State, Limits, EShipFace::XPos, Damage, Out));
		TestTrue(FString(What) + TEXT(" leaves the shields untouched"), IsSameShieldState(State, Drained));
		TestTrue(FString(What) + TEXT(" leaves the outcome untouched"),
			Out.Face == EShipFace::YNeg && Out.AbsorbedByShield == 7.0 && Out.PassedToHull == 7.0 && Out.bFaceBrokenNow);
	};

	ExpectHitRefused(TEXT("NaN damage"), NaN);
	ExpectHitRefused(TEXT("negative damage"), -100.0);

	// TickShields, on the same part-drained faces: a step that got through would at least move
	// every timer, whatever it did to the charges.
	const auto ExpectTickRefused = [this, &Drained](const TCHAR* What, const FShieldLimits& TickLimits, double DeltaSeconds)
	{
		FShieldState State = Drained;
		TickShields(State, TickLimits, DeltaSeconds);
		TestTrue(FString(What) + TEXT(" writes nothing"), IsSameShieldState(State, Drained));
	};

	ExpectTickRefused(TEXT("a NaN step"), Limits, NaN);
	ExpectTickRefused(TEXT("a negative step"), Limits, -0.5);

	// Refused as well, but this one cannot tell whether the guard is there: a zero step with no
	// guard writes back exactly what was there.
	ExpectTickRefused(TEXT("a zero step"), Limits, 0.0);

	FShieldLimits NanCapacity = Limits;
	NanCapacity.FaceCapacity = NaN;
	ExpectTickRefused(TEXT("a NaN capacity"), NanCapacity, 0.5);

	FShieldLimits InfiniteRate = Limits;
	InfiniteRate.RegenPerSecond = Inf;
	ExpectTickRefused(TEXT("an infinite regeneration rate"), InfiniteRate, 0.5);

	FShieldLimits NanDelay = Limits;
	NanDelay.RegenDelaySeconds = NaN;
	ExpectTickRefused(TEXT("a NaN delay"), NanDelay, 0.5);

	return true;
}
