#include "ShipDamage.h"

namespace StarForge::Damage
{
	namespace
	{
		/**
		 * Every component finite. An infinity is not a NaN: in a point it wins every comparison,
		 * and in a half-extent it divides every coordinate on its axis down to zero.
		 */
		bool IsFinite(const FVector& V)
		{
			return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
		}
	}

	bool ClassifyFace(const FVector& HitShipLocal, const FVector& HalfExtents, EShipFace& OutFace)
	{
		if (!IsFinite(HitShipLocal)) { return false; }

		// Finite and positive on every axis. With every half-extent infinite, every point would
		// tie at zero and the sign of X alone would decide. The positive check is negated, so it
		// refuses a NaN on its own as well.
		if (!IsFinite(HalfExtents)) { return false; }
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
		// An EShipFace can be cast from any uint8, and an index outside the six would write past
		// both arrays.
		const int32 Index = static_cast<int32>(Face);
		if (!(Index >= 0 && Index < FaceCount)) { return false; }

		// Negated, so the comparison refuses a NaN as well as a negative hit, which would recharge
		// the face. An infinite hit passes the comparison and is refused by name: it would pass
		// an infinity to the hull.
		if (!(Damage >= 0.0) || !FMath::IsFinite(Damage)) { return false; }

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
		// And on every hit, including one that finds nothing left to absorb: restarted only on
		// absorption, a downed face under steady fire would count its delay down between shots
		// and recharge while it is still being hit.
		State.SecondsSinceHit[Index] = 0.0;

		return true;
	}

	void TickShields(FShieldState& State, const FShieldLimits& Limits, double DeltaSeconds)
	{
		// Negated, so a NaN step fails the comparison; let through, it turns every face's charge
		// and timer into NaN. An infinite step passes the comparison and is refused by name: it
		// sets every timer to infinity, the next step's Inf - Inf carries NaN into every charge,
		// and a face at NaN absorbs every hit whole and never reports breaking. A non-finite
		// limit corrupts the charges in a way that depends on which limit it is and on its sign
		// -- NaN is only one of the ways.
		if (!(DeltaSeconds > 0.0) || !FMath::IsFinite(DeltaSeconds)) { return; }
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

	bool FindThrusterHitBy(
		const TArray<FThrusterDef>& Thrusters,
		const TArray<int32>& AliveIndices,
		const FVector& HitShipLocalMetres,
		double RadiusMetres,
		int32& OutIndex)
	{
		// Refused by name. The reach comparison below would turn a non-finite point away as well --
		// every distance from it is NaN or infinite -- but only for as long as that comparison is
		// written the way round that fails on NaN, and this does not depend on it.
		if (!IsFinite(HitShipLocalMetres)) { return false; }

		// Negated, so a NaN radius is refused by the comparison. A zero radius would still reach a
		// thruster standing exactly on the point, and an infinite one would reach every thruster
		// on the ship.
		if (!(RadiusMetres > 0.0) || !FMath::IsFinite(RadiusMetres)) { return false; }

		// Before any thruster is read: an index past the end is a read past the array.
		for (int32 Index : AliveIndices)
		{
			if (!(Index >= 0 && Index < Thrusters.Num())) { return false; }
		}

		int32 Best = INDEX_NONE;
		double BestDistance = 0.0;
		for (int32 Index : AliveIndices)
		{
			const double Distance = FVector::Dist(Thrusters[Index].Location, HitShipLocalMetres);
			if (!(Distance <= RadiusMetres))
			{
				continue;
			}

			// Nearer wins; at an equal distance the lower index does, whatever order the survivors
			// arrived in. See the header for why equal distances are the ordinary case.
			if (Best == INDEX_NONE || Distance < BestDistance || (Distance == BestDistance && Index < Best))
			{
				Best = Index;
				BestDistance = Distance;
			}
		}

		if (Best == INDEX_NONE)
		{
			return false;
		}

		OutIndex = Best;
		return true;
	}
}
