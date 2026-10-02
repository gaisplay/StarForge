#pragma once

#include "CoreMinimal.h"

/**
 * Does a place fit in the world?
 *
 * A place (spec 5) is a continuous volume the ship flies through on its own, with no
 * assembly and no screen. It has two ceilings, and it has to clear both.
 *
 * THE ENGINE CEILING bounds how far from the world origin any point of the place may lie.
 * It is 2^42 = 4.398046511104e12 cm, and it is both quoted and measured. It is the engine's
 * HALF_WORLD_MAX (UE_LARGE_WORLD_MAX * 0.5, EngineDefines.h 41-57), which is written as a
 * product, so a search for the literal never finds it. The world-size spike of 2026-09-17 flew
 * the loop rig out along +X: a simulating hull worked at 4.30e12 cm and was gone at 4.50e12 cm,
 * with 2^42 between them (docs/superpowers/notes/evidence/2026-09-17-worldsize-*.log).
 *
 * The number is no longer where the engine deletes the hull. The deletion was
 * AActor::CheckStillInWorld, and since 2026-09-26 the project runs with
 * WorldBounds.DisableChecks=1 (spec 4). It is also what destroyed a hull silently 10.49 km
 * BELOW the origin, at KillZ, which this predicate never knew about. So 2^42 is now the edge of
 * MEASURED territory. Inside it the instruments read 0.000 cm at 60 and 400 fps; past it nothing
 * deletes a hull and nothing has been measured. That is why the ceiling stays.
 *
 * What makes it worth a whole file is HOW the border failed: the project's own instruments said
 * nothing. The engine wrote one LogActor warning, and LogStarForge wrote no error. An
 * instrument that looks calm over a broken measurement is the one thing spec 2 forbids, and
 * spec 10.4 turns the finding into this predicate. With the checks off, not even the engine
 * speaks at the border, and this predicate is the only thing standing at it.
 *
 * THE CROSSING CEILING bounds the place's OWN SIZE: a place nobody can cross inside the
 * owner's budget is a place nobody flies through. Spec 5 derives it as D <= v*T, where D is
 * the place's diameter, v the slowest hull class's superlight speed and T the budget. The two
 * ceilings therefore bound DIFFERENT lengths, and each refusal names which.
 *
 * Length is METRES throughout and every field says so. That is not house style here: the
 * engine ceiling was measured in centimetres, and a factor of a hundred applied once too
 * often or once too seldom neither crashes nor logs -- it moves the border of the world.
 * Time is seconds, and the field that holds it says that too.
 *
 * This is a pure function of numbers, which is why it lives in a module with no engine in it:
 * a mistake here does not crash and does not log, it surfaces as "the ship vanished and the
 * log is clean" weeks later.
 */
namespace StarForge::Place
{
	/**
	 * The two ceilings, as numbers a caller passes rather than constants a caller reads.
	 *
	 * The defaults are the project's, derived below. They are not authored per place: a
	 * ceiling is a property of the engine and of the owner's budget, and twelve places each
	 * carrying their own idea of where the world ends would be twelve different worlds. The
	 * parameter exists for the tests, which have to be able to ask about a border without
	 * writing forty billion into every fixture.
	 */
	struct STARFORGECORE_API FPlaceLimits
	{
		/**
		 * 2^42 cm expressed in metres: 4398046511104 / 100. HALF_WORLD_MAX, quoted and measured;
		 * see the file comment.
		 */
		double EngineRadiusMetres = 4.398046511104e10;

		/**
		 * The slowest hull class's superlight speed. Spec 6 puts the large hulls barely above
		 * light and says the multipliers for the other classes ARE NOT SET IN THE SPEC, so the
		 * figure here is light speed itself -- an external physical quantity, not a project
		 * constant -- and it is the one spec 5 does its own arithmetic with.
		 *
		 * It is the SLOWEST class on purpose: the place that binds is the one the slowest ship
		 * cannot cross, and sizing the world by the fastest hull would leave the big ones
		 * crawling across places nobody measured them for.
		 */
		double SlowestHullSpeedMetresPerSec = 2.998e8;

		/**
		 * Spec 6: a flight from one edge of the map to the other is "a minute to a minute and a
		 * half", and spec 5 does its arithmetic with 90 s. In SECONDS, and that is load-bearing
		 * rather than stylistic: this project has twice paid for a threshold written in frames.
		 */
		double CrossingBudgetSeconds = 90.0;
	};

	/**
	 * Which condition refused, or None.
	 *
	 * A bool would be the answer thrown away, the same reasoning that gave docking four
	 * refusals instead of one: a designer told "no" moves the wrong thing. The order of the
	 * enumerators is the order they are tested in, and that order is deliberate -- see
	 * MeasurePlace.
	 */
	enum class EPlaceRefusal : uint8
	{
		None,

		/** The limits themselves are unusable, so nothing was decided about the place. */
		LimitsNotUsable,

		/** The origin or the radius is not a finite number. */
		NotFinite,

		/** A radius of zero or less is not a volume. */
		RadiusNotPositive,

		/** The far edge of the place lies past the engine's border. */
		EngineReach,

		/** The slowest hull cannot cross the place inside the budget. */
		CrossingTime
	};

	/**
	 * Everything the check measured, whether the place fitted or not.
	 *
	 * Returned in full rather than behind a bool because the assembly prints ONE LINE PER
	 * PLACE, fitting or not (spec 10.4: a line that appears only in trouble cannot be told
	 * apart from an instrument that never ran). A bool would make the caller recompute all six
	 * figures to print them -- two copies of one piece of arithmetic, which eventually
	 * disagree, quietly.
	 *
	 * Margins are signed: negative is how far over the ceiling the place went. One number, not
	 * two, so a reader cannot be shown a margin and an overshoot that do not add up.
	 */
	struct STARFORGECORE_API FPlaceFit
	{
		EPlaceRefusal Refusal = EPlaceRefusal::None;

		/** |origin| + radius: how far from the world origin the furthest point of the place lies. */
		double ReachMetres = 0.0;

		double EngineCeilingMetres = 0.0;

		/** EngineCeilingMetres - ReachMetres. Negative is over. */
		double EngineMarginMetres = 0.0;

		/** Twice the radius: what a ship crossing the place has to cover. */
		double DiameterMetres = 0.0;

		/** SlowestHullSpeedMetresPerSec * CrossingBudgetSeconds -- a DIAMETER, not a radius. */
		double CrossingCeilingMetres = 0.0;

		/** CrossingCeilingMetres - DiameterMetres. Negative is over. */
		double CrossingMarginMetres = 0.0;

		/** DiameterMetres / SlowestHullSpeedMetresPerSec, against CrossingBudgetSeconds. */
		double CrossingSeconds = 0.0;
	};

	/**
	 * Measures a place against both ceilings and reports everything it measured.
	 *
	 * OriginFromWorldOriginMetres is where the place's own origin sits in world coordinates;
	 * RadiusMetres is half its extent. Both in metres, as the names say.
	 *
	 * The engine ceiling is tested against the REACH and the crossing ceiling against the
	 * DIAMETER, because spec 5 derives them as bounds on different lengths. Spec 10.4 states
	 * the check in one line -- the sum against the smaller ceiling -- and taken literally that
	 * is stricter than what was derived: a kilometre-wide place twenty billion metres out would
	 * be refused "because it cannot be crossed", when crossing it takes seven microseconds. A
	 * refusal with the wrong reason is worse than no refusal, because it sends whoever reads it
	 * to fix the wrong thing. "The smaller ceiling binds" still holds in the only sense that
	 * matters: a place violating either is refused.
	 *
	 * The order of the tests is the order a person can act on, which is the rule CanDock
	 * follows. Unusable limits first, because a broken world must not be blamed on the place
	 * inside it. Then the arithmetic guards. Then the engine, then the crossing: the engine
	 * ceiling is a hard property of the world that no decision moves, while the crossing
	 * ceiling is a budget; told the first a designer MOVES the place, told the second he
	 * SHRINKS it, and shrinking a place that is in the wrong half of the galaxy is wasted work.
	 *
	 * The ceilings, the reach, the diameter and the crossing time are filled in on every path
	 * where they can be computed, including refusals. The place's own figures are left at zero
	 * only when the input they come from is the thing that was refused.
	 */
	STARFORGECORE_API FPlaceFit MeasurePlace(
		const FVector& OriginFromWorldOriginMetres,
		double RadiusMetres,
		const FPlaceLimits& Limits);

	/** "fits", "limits not usable", "not a finite place", "radius not positive", "engine reach", "crossing time". */
	STARFORGECORE_API const TCHAR* RefusalName(EPlaceRefusal Refusal);
}
