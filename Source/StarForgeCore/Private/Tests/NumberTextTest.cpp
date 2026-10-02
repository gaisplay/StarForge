#include "Misc/AutomationTest.h"
#include "NumberText.h"

namespace
{
	// A value no parse below produces, so "left untouched" is checked rather than assumed.
	constexpr double Sentinel = -12345.678;
	constexpr int32 WholeSentinel = -424242;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNumberTextAcceptsNumbersTest,
	"StarForge.Core.Parse.AcceptsNumbers",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FNumberTextAcceptsNumbersTest::RunTest(const FString& Parameters)
{
	struct FCase { const TCHAR* Text; double Expected; };
	const FCase Cases[] = {
		{ TEXT("0"), 0.0 },
		{ TEXT("-1.5"), -1.5 },
		{ TEXT("+2"), 2.0 },
		{ TEXT(".5"), 0.5 },
		{ TEXT("5."), 5.0 },
		{ TEXT("4.3e10"), 4.3e10 },
		{ TEXT("45000000000"), 45000000000.0 },
		{ TEXT("2E-3"), 2e-3 },
		{ TEXT("1e+2"), 100.0 },
		{ TEXT("  7.25  "), 7.25 },
	};

	for (const FCase& Case : Cases)
	{
		double Value = Sentinel;
		const bool bParsed = StarForge::Parse::TryParseFiniteNumber(Case.Text, Value);
		TestTrue(FString::Printf(TEXT("'%s' is a number"), Case.Text), bParsed);
		TestEqual(FString::Printf(TEXT("'%s' reads as %.17g"), Case.Text, Case.Expected), Value, Case.Expected, 0.0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNumberTextRefusesWhatIsNotANumberTest,
	"StarForge.Core.Parse.RefusesWhatIsNotANumber",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FNumberTextRefusesWhatIsNotANumberTest::RunTest(const FString& Parameters)
{
	// The first five are what FDefaultValueHelper::ParseDouble takes and converts to zero -- the
	// reason this function exists. The rest are each one rule of the grammar.
	const TCHAR* Refused[] = {
		TEXT("."), TEXT("-"), TEXT("+"), TEXT("e10"), TEXT("-e"),
		TEXT(""), TEXT("   "), TEXT("1e"), TEXT("1e+"), TEXT("43abc"), TEXT("1,5"), TEXT("0x10"),
		TEXT("1.2.3"), TEXT("--1"), TEXT("inf"), TEXT("nan"), TEXT("1 2"), TEXT("5f"),
		TEXT("1e999"), TEXT("-1e999"),
	};

	for (const TCHAR* Text : Refused)
	{
		double Value = Sentinel;
		TestFalse(FString::Printf(TEXT("'%s' is refused"), Text),
			StarForge::Parse::TryParseFiniteNumber(Text, Value));
		TestEqual(FString::Printf(TEXT("'%s' leaves the output untouched"), Text), Value, Sentinel, 0.0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FNumberTextWholeNumbersTest,
	"StarForge.Core.Parse.WholeNumbers",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FNumberTextWholeNumbersTest::RunTest(const FString& Parameters)
{
	struct FCase { const TCHAR* Text; int32 Expected; };
	const FCase Accepted[] = {
		{ TEXT("0"), 0 },
		{ TEXT("3"), 3 },
		{ TEXT("+5"), 5 },
		{ TEXT("-1"), -1 },
		{ TEXT(" 23 "), 23 },
		{ TEXT("007"), 7 },
		{ TEXT("2147483647"), MAX_int32 },
		{ TEXT("-2147483648"), MIN_int32 },
	};

	for (const FCase& Case : Accepted)
	{
		int32 Value = WholeSentinel;
		TestTrue(FString::Printf(TEXT("'%s' is a whole number"), Case.Text),
			StarForge::Parse::TryParseWholeNumber(Case.Text, Value));
		TestEqual(FString::Printf(TEXT("'%s' reads as %d"), Case.Text, Case.Expected), Value, Case.Expected);
	}

	// "-", "+" and "." are what IsNumeric took and Atoi turned into thruster 0.
	const TCHAR* Refused[] = {
		TEXT("-"), TEXT("+"), TEXT("."), TEXT(""), TEXT("3.0"), TEXT("3."), TEXT("1e2"), TEXT("0x1"),
		TEXT("12a"), TEXT("2147483648"), TEXT("-2147483649"), TEXT("99999999999999999999"),
	};

	for (const TCHAR* Text : Refused)
	{
		int32 Value = WholeSentinel;
		TestFalse(FString::Printf(TEXT("'%s' is refused"), Text),
			StarForge::Parse::TryParseWholeNumber(Text, Value));
		TestEqual(FString::Printf(TEXT("'%s' leaves the output untouched"), Text), Value, WholeSentinel);
	}

	return true;
}
