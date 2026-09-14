#pragma once

#include "CoreMinimal.h"

/**
 * Directional shields: six faces, each with its own charge, its own timer and its own
 * regeneration, and the classification that says which face a hit belongs to.
 *
 * Units. Length in ClassifyFace is unit-agnostic: the point and the half-extents must be in
 * one unit, and the game passes centimetres. Time is seconds, and the names of the fields and
 * parameters that hold it say so. Charge and damage share one unit, which is not SI and has no
 * name beyond points.
 */
namespace StarForge::Damage
{
	/** The value of a face is its index into FShieldState's arrays. */
	enum class EShipFace : uint8
	{
		XPos,
		XNeg,
		YPos,
		YNeg,
		ZPos,
		ZNeg
	};

	constexpr int32 FaceCount = 6;

	/**
	 * The same three numbers for every face.
	 *
	 * Zeroed, not left uninitialised: a struct of plain doubles initialises nothing, and a limit
	 * read off the stack would be a shield of any size.
	 */
	struct STARFORGECORE_API FShieldLimits
	{
		double FaceCapacity = 0.0;
		double RegenPerSecond = 0.0;
		double RegenDelaySeconds = 0.0;
	};

	/**
	 * Per face, indexed by EShipFace.
	 *
	 * Zeroed, because a C array of doubles in a struct initialises nothing. A default-constructed
	 * state is every face empty and every face hit just now.
	 */
	struct STARFORGECORE_API FShieldState
	{
		double Charge[FaceCount] = {};
		double SecondsSinceHit[FaceCount] = {};
	};

	/** What one hit did. */
	struct STARFORGECORE_API FHitOutcome
	{
		EShipFace Face = EShipFace::XPos;
		double AbsorbedByShield = 0.0;
		double PassedToHull = 0.0;

		/** True only on the hit that takes the face from above zero to zero. See ApplyHit. */
		bool bFaceBrokenNow = false;
	};

	/**
	 * Which face of the hull a point in ship coordinates belongs to.
	 *
	 * The point is normalised by the half-extent of each axis and the largest normalised
	 * coordinate wins; on a cube that is simply the largest absolute coordinate, but the hull
	 * is not required to stay a cube and a raw comparison would quietly stop being right the
	 * day it is not.
	 *
	 * TIES ARE RESOLVED X BEFORE Y BEFORE Z, and a tie is not a theoretical case: a hit on an
	 * edge of a cube has two normalised coordinates equal to the last bit, and a hit on a corner
	 * has three. Without a stated rule the answer is decided by the order of the comparisons --
	 * which is to say by nothing.
	 * The sign is taken as "non-negative is the positive face". It decides anything only when the
	 * winning normalised coordinate is zero, which is the exact centre alone, and there it is
	 * likewise deterministic rather than pretty.
	 *
	 * A point at the exact centre ties at zero on all three and reads XPos. Degenerate and
	 * deliberate: there is no face under a point inside the hull, and refusing would force every
	 * caller to invent one.
	 *
	 * Returns false and leaves OutFace untouched on a non-finite point or a half-extent that is
	 * not positive (NaN is not) -- the latter because dividing by it produces an infinity that
	 * compares larger than everything and would answer confidently.
	 */
	STARFORGECORE_API bool ClassifyFace(const FVector& HitShipLocal, const FVector& HalfExtents, EShipFace& OutFace);

	/**
	 * One hit of Damage on Face. The face absorbs the smaller of its charge and the damage, the
	 * rest passes to the hull, and that face's timer, and no other, restarts from zero.
	 *
	 * bFaceBrokenNow is an event rather than a state: true on the hit that takes the face from
	 * above zero to zero, false on a hit that finds it already at zero.
	 *
	 * Returns false, and touches neither State nor Out, when Damage is negative or NaN. A
	 * negative hit would recharge the face it landed on.
	 */
	STARFORGECORE_API bool ApplyHit(
		FShieldState& State, const FShieldLimits& Limits, EShipFace Face, double Damage, FHitOutcome& Out);

	/**
	 * Advances every face's timer by DeltaSeconds, and regenerates each face at RegenPerSecond
	 * for the part of this step that lies past RegenDelaySeconds since that face was hit, up to
	 * FaceCapacity. The part of the step, not the whole step once the timer is past the delay:
	 * the .cpp says why that difference is a frame rate.
	 *
	 * Writes nothing when DeltaSeconds is not positive (NaN is not) or any limit is not finite.
	 */
	STARFORGECORE_API void TickShields(FShieldState& State, const FShieldLimits& Limits, double DeltaSeconds);

	/** "+X", "-X", "+Y", "-Y", "+Z" or "-Z". */
	STARFORGECORE_API const TCHAR* FaceName(EShipFace Face);
}
