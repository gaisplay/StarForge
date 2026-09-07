#pragma once

#include "CoreMinimal.h"
#include "ThrusterDef.h"

/**
 * The one place SI meets Unreal units.
 *
 * StarForgeCore works in metres, kilograms, newtons and radians; Unreal works in
 * centimetres with mass in kilograms. Every conversion between the two lives here
 * and nowhere else, because a factor of a hundred applied twice -- or not at all --
 * does not crash or log. The ship simply flies wrong, and the search for why
 * starts in the solver, which is innocent.
 */
namespace StarForge::Units
{
	/** Unreal centimetres in one metre. */
	inline constexpr double CmPerMetre = 100.0;

	STARFORGECORE_API double MetresToCm(double Metres);
	STARFORGECORE_API double CmToMetres(double Cm);

	/**
	 * Newtons to Unreal force units.
	 *
	 * A newton is a kilogram metre per second squared. Mass is already in
	 * kilograms, so only the length changes: one factor of a hundred.
	 */
	STARFORGECORE_API double NewtonsToUu(double Newtons);

	/**
	 * Newton-metres to Unreal torque units.
	 *
	 * A newton-metre carries length twice -- once in the force, once in the lever
	 * arm -- so the factor is a hundred squared.
	 */
	STARFORGECORE_API double NewtonMetresToUu(double NewtonMetres);

	/** Chaos reports the inertia tensor in kilogram centimetres squared. */
	STARFORGECORE_API FVector InertiaUuToSi(const FVector& InertiaUu);

	/** A whole wrench, with the two different factors its halves need. */
	STARFORGECORE_API FWrench WrenchToUu(const FWrench& Si);
}
