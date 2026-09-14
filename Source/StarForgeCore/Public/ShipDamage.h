#pragma once

#include "CoreMinimal.h"
#include "ThrusterDef.h"

/**
 * Directional shields: six faces, each with its own charge, its own timer and its own
 * regeneration, and the classification that says which face a hit belongs to. And the one
 * question a hit on the hull answers beyond that: which thruster it broke.
 *
 * Units. Length in ClassifyFace is unit-agnostic: the point and the half-extents must be in
 * one unit, and the game passes centimetres. Length in FindThrusterHitBy is METRES, fixed
 * rather than following the caller, because it measures against FThrusterDef::Location and
 * that is declared in metres. Both are right where they stand: a face is decided by ratios, so
 * its unit cancels, while a kill radius is a distance compared with a distance and has to be in
 * the unit of the thing it is compared with. The parameter names carry the unit wherever it is
 * fixed. Time is seconds, and the names of the fields and parameters that hold it say so.
 * Charge and damage share one unit, which is not SI and has no name beyond points.
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
	 * Returns false and leaves OutFace untouched on a non-finite point, or on a half-extent that
	 * is not both finite and positive. Dividing by a zero half-extent produces an infinity that
	 * compares larger than everything and would answer confidently; dividing by an infinite one
	 * produces zero, and with all three infinite every point would read as the centre.
	 */
	STARFORGECORE_API bool ClassifyFace(const FVector& HitShipLocal, const FVector& HalfExtents, EShipFace& OutFace);

	/**
	 * One hit of Damage on Face. The face absorbs the smaller of its charge and the damage, the
	 * rest passes to the hull, and that face's timer, and no other, restarts from zero -- also
	 * when the face had nothing left to absorb.
	 *
	 * bFaceBrokenNow is an event rather than a state: true on the hit that takes the face from
	 * above zero to zero, false on a hit that finds it already at zero.
	 *
	 * Returns false, and touches neither State nor Out, when Face is not one of the six or Damage
	 * is not a finite non-negative number. A face outside the six would write past both arrays,
	 * a negative hit would recharge the face it landed on, and an infinite one would pass an
	 * infinity to the hull.
	 */
	STARFORGECORE_API bool ApplyHit(
		FShieldState& State, const FShieldLimits& Limits, EShipFace Face, double Damage, FHitOutcome& Out);

	/**
	 * Advances every face's timer by DeltaSeconds, and regenerates each face at RegenPerSecond
	 * for the part of this step that lies past RegenDelaySeconds since that face was hit, up to
	 * FaceCapacity. The part of the step, not the whole step once the timer is past the delay:
	 * the .cpp says why that difference is a frame rate.
	 *
	 * Writes nothing when DeltaSeconds is not a finite positive number or any limit is not finite.
	 */
	STARFORGECORE_API void TickShields(FShieldState& State, const FShieldLimits& Limits, double DeltaSeconds);

	/** "+X", "-X", "+Y", "-Y", "+Z" or "-Z". */
	STARFORGECORE_API const TCHAR* FaceName(EShipFace Face);

	/**
	 * The nearest surviving thruster within RadiusMetres of a hit, or none.
	 *
	 * Metres, and the names say so: FThrusterDef::Location is declared in metres, and this
	 * function is the one place in the file where the length unit is fixed rather than following
	 * the caller. Its neighbour ClassifyFace is unit-agnostic; both are right, and the difference
	 * is stated because the branch review of stage 3 caught precisely this drift on
	 * FDockingApproach::Distance -- centimetres on one side of an assignment and metres on the
	 * other.
	 *
	 * On the rig's layout the nearest point of the OUTER SKIN to any thruster is 2.1 m: the
	 * thrusters sit at (+-2, +-2, 0) and the +X and +Y faces at 4.1. A radius below that makes
	 * this function dead code that looks alive. Above 4.1 it would let a shot into the deck or
	 * the ceiling kill one, and that would destroy a negative control the geometry gives for
	 * free: all twenty-four sit on z = 0, so nothing fired at the roof can ever reach one.
	 *
	 * Ties go to the LOWEST SURVIVING INDEX, and ties are the normal case rather than the corner
	 * one: six thrusters share each corner's position exactly, so every hit near a corner is a
	 * six-way tie. The rule is the index, not the order AliveIndices lists them in.
	 *
	 * Thrusters holds every thruster the ship was built with, and AliveIndices the indices into
	 * it that still exist -- the asset's indices, which are what a log line names.
	 *
	 * Returns false and leaves OutIndex untouched when nothing surviving is in reach, and also
	 * when the call is refused: a point that is not finite, a radius that is not finite and
	 * positive, or a survivor index that is not an index into Thrusters. A bad survivor index
	 * refuses the whole call rather than being skipped, because it means the caller's bookkeeping
	 * of what is destroyed has come apart from the layout, and an answer over the rest would
	 * name a thruster from a list nobody can trust.
	 */
	STARFORGECORE_API bool FindThrusterHitBy(
		const TArray<FThrusterDef>& Thrusters,
		const TArray<int32>& AliveIndices,
		const FVector& HitShipLocalMetres,
		double RadiusMetres,
		int32& OutIndex);
}
