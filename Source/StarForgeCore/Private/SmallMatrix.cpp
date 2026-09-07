#include "SmallMatrix.h"

namespace StarForge
{
	FMatrix6 FMatrix6::Zero()
	{
		FMatrix6 Result;
		for (int32 Row = 0; Row < Size; ++Row)
		{
			for (int32 Col = 0; Col < Size; ++Col)
			{
				Result.M[Row][Col] = 0.0;
			}
		}
		return Result;
	}

	FMatrix6 FMatrix6::Identity()
	{
		FMatrix6 Result = Zero();
		for (int32 Index = 0; Index < Size; ++Index)
		{
			Result.M[Index][Index] = 1.0;
		}
		return Result;
	}

	FMatrix6 FMatrix6::operator*(const FMatrix6& Other) const
	{
		FMatrix6 Result = Zero();
		for (int32 Row = 0; Row < Size; ++Row)
		{
			for (int32 Col = 0; Col < Size; ++Col)
			{
				double Sum = 0.0;
				for (int32 Index = 0; Index < Size; ++Index)
				{
					Sum += M[Row][Index] * Other.M[Index][Col];
				}
				Result.M[Row][Col] = Sum;
			}
		}
		return Result;
	}

	bool FMatrix6::TryInvert(FMatrix6& Out) const
	{
		// Work on an augmented [A | I] and reduce the left half to the identity.
		double Work[Size][2 * Size];
		for (int32 Row = 0; Row < Size; ++Row)
		{
			for (int32 Col = 0; Col < Size; ++Col)
			{
				Work[Row][Col] = M[Row][Col];
				Work[Row][Size + Col] = (Row == Col) ? 1.0 : 0.0;
			}
		}

		for (int32 Pivot = 0; Pivot < Size; ++Pivot)
		{
			// Partial pivoting: without it a zero on the diagonal of an otherwise
			// perfectly invertible matrix would abort the whole solve.
			int32 BestRow = Pivot;
			for (int32 Row = Pivot + 1; Row < Size; ++Row)
			{
				if (FMath::Abs(Work[Row][Pivot]) > FMath::Abs(Work[BestRow][Pivot]))
				{
					BestRow = Row;
				}
			}

			if (FMath::Abs(Work[BestRow][Pivot]) < UE_DOUBLE_SMALL_NUMBER)
			{
				return false;
			}

			if (BestRow != Pivot)
			{
				for (int32 Col = 0; Col < 2 * Size; ++Col)
				{
					Swap(Work[Pivot][Col], Work[BestRow][Col]);
				}
			}

			const double PivotValue = Work[Pivot][Pivot];
			for (int32 Col = 0; Col < 2 * Size; ++Col)
			{
				Work[Pivot][Col] /= PivotValue;
			}

			for (int32 Row = 0; Row < Size; ++Row)
			{
				if (Row == Pivot)
				{
					continue;
				}

				const double Factor = Work[Row][Pivot];
				if (Factor == 0.0)
				{
					continue;
				}

				for (int32 Col = 0; Col < 2 * Size; ++Col)
				{
					Work[Row][Col] -= Factor * Work[Pivot][Col];
				}
			}
		}

		for (int32 Row = 0; Row < Size; ++Row)
		{
			for (int32 Col = 0; Col < Size; ++Col)
			{
				Out.M[Row][Col] = Work[Row][Size + Col];
			}
		}

		return true;
	}
}
