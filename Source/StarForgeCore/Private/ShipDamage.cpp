#include "ShipDamage.h"

namespace StarForge::Damage
{
	namespace
	{
		/** Every component finite: an infinity is not a NaN, and it would win every comparison. */
		bool IsFinite(const FVector& V)
		{
			return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
		}
	}

	bool ClassifyFace(const FVector& HitShipLocal, const FVector& HalfExtents, EShipFace& OutFace)
	{
		if (!IsFinite(HitShipLocal)) { return false; }

		// Negated, so a NaN half-extent fails the guard instead of falling through it.
		if (!(HalfExtents.X > 0.0) || !(HalfExtents.Y > 0.0) || !(HalfExtents.Z > 0.0)) { return false; }

		const double NX = FMath::Abs(HitShipLocal.X) / HalfExtents.X;
		const double NY = FMath::Abs(HitShipLocal.Y) / HalfExtents.Y;
		const double NZ = FMath::Abs(HitShipLocal.Z) / HalfExtents.Z;

		// X before Y before Z: an axis wins a tie against every axis after it, so a later axis
		// has to beat each earlier one strictly. See the header.
		if (NX >= NY && NX >= NZ)
		{
			OutFace = HitShipLocal.X >= 0.0 ? EShipFace::XPos : EShipFace::XNeg;
		}
		else if (NY >= NZ)
		{
			OutFace = HitShipLocal.Y >= 0.0 ? EShipFace::YPos : EShipFace::YNeg;
		}
		else
		{
			OutFace = HitShipLocal.Z >= 0.0 ? EShipFace::ZPos : EShipFace::ZNeg;
		}
		return true;
	}

	bool ApplyHit(FShieldState& State, const FShieldLimits& Limits, EShipFace Face, double Damage, FHitOutcome& Out)
	{
		// Negated, so NaN is refused by the same line as a negative hit.
		if (!(Damage >= 0.0)) { return false; }

		const int32 Index = static_cast<int32>(Face);
		Out.Face = Face;

		const double Absorbed = FMath::Min(State.Charge[Index], Damage);
		Out.AbsorbedByShield = Absorbed;
		Out.PassedToHull = Damage - Absorbed;

		const bool bWasUp = State.Charge[Index] > 0.0;
		State.Charge[Index] -= Absorbed;

		// "Broken now" rather than "broken": this is an event, and the game module writes a line
		// for it. Reported on every hit once the face is down, it would write that line five
		// times a second for as long as the trigger is held, and the one moment worth reading
		// would be buried in it.
		Out.bFaceBrokenNow = bWasUp && State.Charge[Index] <= 0.0;

		// This face's timer only. Six timers, not one: a ship under continuous fire on one side
		// would otherwise have every shield frozen, and that reads as "shields do not regenerate".
		State.SecondsSinceHit[Index] = 0.0;

		return true;
	}

	void TickShields(FShieldState& State, const FShieldLimits& Limits, double DeltaSeconds)
	{
		// Negated, so a NaN step fails the guard. Let through, a NaN step turns every face's
		// charge and timer into NaN, and a non-finite limit corrupts the charges in a way that
		// depends on which limit it is and on its sign -- NaN is only one of the ways.
		if (!(DeltaSeconds > 0.0)) { return; }
		if (!FMath::IsFinite(Limits.FaceCapacity) || !FMath::IsFinite(Limits.RegenPerSecond)
			|| !FMath::IsFinite(Limits.RegenDelaySeconds))
		{
			return;
		}

		for (int32 i = 0; i < FaceCount; ++i)
		{
			const double Before = State.SecondsSinceHit[i];
			const double After = Before + DeltaSeconds;

			// The part of THIS STEP that lies past the delay, integrated -- not a test of the
			// timer once the step has been added to it. Tested that way, a single ten-second step
			// after a three-second delay regenerates for the whole ten (500 points) while a
			// thousand hundredth-second steps regenerate for seven (350). Forty-three per cent,
			// decided by the frame rate, in a quantity nobody would think to check. Written this
			// way the two agree up to the rounding of a thousand additions of 0.01, and the test
			// holds them to 1e-9.
			const double RegenSeconds = FMath::Max(0.0, After - FMath::Max(Before, Limits.RegenDelaySeconds));

			State.Charge[i] = FMath::Min(Limits.FaceCapacity, State.Charge[i] + Limits.RegenPerSecond * RegenSeconds);
			State.SecondsSinceHit[i] = After;
		}
	}

	const TCHAR* FaceName(EShipFace Face)
	{
		switch (Face)
		{
		case EShipFace::XPos: return TEXT("+X");
		case EShipFace::XNeg: return TEXT("-X");
		case EShipFace::YPos: return TEXT("+Y");
		case EShipFace::YNeg: return TEXT("-Y");
		case EShipFace::ZPos: return TEXT("+Z");
		case EShipFace::ZNeg: return TEXT("-Z");
		}
		return TEXT("?");
	}
}
