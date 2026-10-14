#pragma once

#include "CoreMinimal.h"

/**
 * Numbers typed by a person -- console arguments and string console variables -- read strictly.
 *
 * The engine offers two readers, and neither is a check. FCString::Atod and Atoi cannot fail: they
 * answer zero for a string with no number in it, so "sf.KillThruster -" destroyed thruster 0 and
 * "sf.PlaceOffsetM=nonsense" moved the place by zero metres while announcing that it had moved it.
 * FDefaultValueHelper::ParseDouble does validate, but every part of its grammar is optional
 * (IsStringValidFloat, DefaultValueHelper.cpp 268-308): ".", "-", "e10" and "+e" all pass and then
 * convert to zero through the same Atod. A control that reports doing what it did not do is worse
 * than one that refuses, so these two refuse anything that is not a whole number written out.
 *
 * Both accept surrounding whitespace and nothing else around the number, and leave Out untouched on
 * every refusal.
 */
namespace StarForge::Parse
{
	/**
	 * A finite decimal number: an optional sign, then digits with at most one point and AT LEAST ONE
	 * DIGIT among them, then optionally e or E, an optional sign and at least one digit. "4.3e10",
	 * "-1.5", ".5" and "5." are numbers; ".", "-", "e10", "1e", "1e+", "0x10", "1,5", "inf" and "nan"
	 * are not. A well-formed number that overflows to infinity ("1e999") is refused too.
	 */
	STARFORGECORE_API bool TryParseFiniteNumber(const FString& Text, double& OutValue);

	/**
	 * A whole number in int32's range: an optional sign and at least one digit, nothing else. "3.0" is
	 * refused rather than truncated, and so is a value that does not fit.
	 */
	STARFORGECORE_API bool TryParseWholeNumber(const FString& Text, int32& OutValue);
}
