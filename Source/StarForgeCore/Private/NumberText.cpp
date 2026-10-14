#include "NumberText.h"

namespace StarForge::Parse
{
	namespace
	{
		bool IsDigit(TCHAR C)
		{
			return C >= TEXT('0') && C <= TEXT('9');
		}

		bool IsSign(TCHAR C)
		{
			return C == TEXT('+') || C == TEXT('-');
		}
	}

	bool TryParseFiniteNumber(const FString& Text, double& OutValue)
	{
		const FString Trimmed = Text.TrimStartAndEnd();
		const int32 Length = Trimmed.Len();
		int32 Index = 0;

		if (Index < Length && IsSign(Trimmed[Index]))
		{
			++Index;
		}

		int32 MantissaDigits = 0;
		while (Index < Length && IsDigit(Trimmed[Index]))
		{
			++Index;
			++MantissaDigits;
		}
		if (Index < Length && Trimmed[Index] == TEXT('.'))
		{
			++Index;
			while (Index < Length && IsDigit(Trimmed[Index]))
			{
				++Index;
				++MantissaDigits;
			}
		}
		if (MantissaDigits == 0)
		{
			return false;
		}

		if (Index < Length && (Trimmed[Index] == TEXT('e') || Trimmed[Index] == TEXT('E')))
		{
			++Index;
			if (Index < Length && IsSign(Trimmed[Index]))
			{
				++Index;
			}
			int32 ExponentDigits = 0;
			while (Index < Length && IsDigit(Trimmed[Index]))
			{
				++Index;
				++ExponentDigits;
			}
			if (ExponentDigits == 0)
			{
				return false;
			}
		}

		if (Index != Length)
		{
			return false;
		}

		// The grammar above is a subset of what Atod reads, so on a string that passed it Atod is a
		// conversion and no longer a guess.
		const double Value = FCString::Atod(*Trimmed);

		// One guard covers infinity and NaN both: FMath::IsFinite tests that the exponent is not all
		// ones (GenericPlatformMath.h 587), and a NaN's is.
		if (!FMath::IsFinite(Value))
		{
			return false;
		}

		OutValue = Value;
		return true;
	}

	bool TryParseWholeNumber(const FString& Text, int32& OutValue)
	{
		const FString Trimmed = Text.TrimStartAndEnd();
		const int32 Length = Trimmed.Len();
		int32 Index = 0;

		bool bNegative = false;
		if (Index < Length && IsSign(Trimmed[Index]))
		{
			bNegative = Trimmed[Index] == TEXT('-');
			++Index;
		}

		if (Index == Length)
		{
			return false;
		}

		// Accumulated in 64 bits and checked against int32 on every digit, so a value too large for
		// the type is refused instead of wrapping into some other thruster's index.
		int64 Magnitude = 0;
		for (; Index < Length; ++Index)
		{
			if (!IsDigit(Trimmed[Index]))
			{
				return false;
			}
			Magnitude = Magnitude * 10 + (Trimmed[Index] - TEXT('0'));
			if (Magnitude > static_cast<int64>(MAX_int32) + 1)
			{
				return false;
			}
		}

		const int64 Value = bNegative ? -Magnitude : Magnitude;
		if (Value < MIN_int32 || Value > MAX_int32)
		{
			return false;
		}

		OutValue = static_cast<int32>(Value);
		return true;
	}
}
