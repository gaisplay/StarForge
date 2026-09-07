#include "ThrustAllocator.h"

#include "SmallMatrix.h"

namespace StarForge
{
	FWrench ComputeThrusterWrench(const FThrusterDef& Thruster, const FVector& CentreOfMass)
	{
		FWrench Result;
		// Normalised deliberately: the layout arrives from an editable asset, and an
		// unnormalised direction would otherwise scale thrust by its own length with
		// nothing to warn anybody.
		Result.Force = Thruster.Direction.GetSafeNormal() * Thruster.MaxThrust;
		Result.Torque = FVector::CrossProduct(Thruster.Location - CentreOfMass, Result.Force);
		return Result;
	}

	bool FThrustAllocator::ComputeResponseRows(
		const TArray<FWrench>& InColumns,
		const TArray<int32>& Indices,
		double DampingLambda,
		TArray<FThrusterResponse>& OutRows)
	{
		OutRows.Reset();

		if (Indices.Num() == 0)
		{
			return false;
		}

		// Gram matrix B * B^T, always 6x6 however many thrusters there are. The
		// damping term keeps it invertible when the array is merely near-degenerate
		// rather than exactly so.
		FMatrix6 Gram = FMatrix6::Zero();
		for (int32 Row = 0; Row < 6; ++Row)
		{
			for (int32 Col = 0; Col < 6; ++Col)
			{
				double Sum = 0.0;
				for (int32 Index : Indices)
				{
					Sum += InColumns[Index].Component(Row) * InColumns[Index].Component(Col);
				}
				Gram.At(Row, Col) = Sum;
			}
			Gram.At(Row, Row) += DampingLambda;
		}

		FMatrix6 GramInverse;
		if (!Gram.TryInvert(GramInverse))
		{
			return false;
		}

		// Response = B^T * (B B^T + lambda I)^-1, one row per column in Indices.
		OutRows.SetNum(Indices.Num());
		for (int32 Slot = 0; Slot < Indices.Num(); ++Slot)
		{
			const FWrench& Column = InColumns[Indices[Slot]];
			for (int32 Col = 0; Col < 6; ++Col)
			{
				double Sum = 0.0;
				for (int32 Row = 0; Row < 6; ++Row)
				{
					Sum += Column.Component(Row) * GramInverse.At(Row, Col);
				}
				OutRows[Slot].Row[Col] = Sum;
			}
		}

		return true;
	}

	bool FThrustAllocator::Build(
		const TArray<FThrusterDef>& Thrusters,
		const FVector& CentreOfMass,
		double DampingLambda)
	{
		bBuilt = false;
		Columns.Reset();
		Response.Reset();
		Lambda = DampingLambda;

		if (Thrusters.Num() == 0)
		{
			return false;
		}

		for (const FThrusterDef& Thruster : Thrusters)
		{
			Columns.Add(ComputeThrusterWrench(Thruster, CentreOfMass));
		}

		TArray<int32> AllIndices;
		AllIndices.Reserve(Columns.Num());
		for (int32 Index = 0; Index < Columns.Num(); ++Index)
		{
			AllIndices.Add(Index);
		}

		if (!ComputeResponseRows(Columns, AllIndices, Lambda, Response))
		{
			Columns.Reset();
			return false;
		}

		// An array that cannot span all six axes produces a Gram matrix that only the
		// damping made invertible, and the solution it yields is meaningless. Detect
		// it by asking for each axis in turn and seeing whether anything comes back.
		for (int32 Axis = 0; Axis < 6; ++Axis)
		{
			double Probe[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
			Probe[Axis] = 1.0;

			TArray<double> Throttles;
			Throttles.SetNumZeroed(Columns.Num());
			for (int32 Index = 0; Index < Columns.Num(); ++Index)
			{
				double Sum = 0.0;
				for (int32 Col = 0; Col < 6; ++Col)
				{
					Sum += Response[Index].Row[Col] * Probe[Col];
				}
				Throttles[Index] = Sum;
			}

			const FWrench Achieved = ComputeAchieved(Throttles);
			if (FMath::Abs(Achieved.Component(Axis)) < 1e-6)
			{
				Columns.Reset();
				Response.Reset();
				return false;
			}
		}

		bBuilt = true;
		return true;
	}

	void FThrustAllocator::Solve(const FWrench& Desired, TArray<double>& OutThrottles) const
	{
		OutThrottles.Reset();
		OutThrottles.SetNumZeroed(Columns.Num());

		if (!bBuilt)
		{
			return;
		}

		FWrench Remaining = Desired;

		// Clamp and correct. Each pass solves for whatever is still missing, then
		// clamps the result into the physical range. Three passes is where the spec
		// settled: enough to recover a saturated request, few enough to stay a fixed
		// cost per frame.
		//
		// Each pass solves over the thrusters still free to move, which is what makes
		// three enough. Re-solving over all of them keeps re-proposing a negative
		// throttle for every opposed thruster; those clamp back to zero, so only half
		// the shortfall is ever recovered and the array converges to seven eighths of
		// whatever was asked for -- a ship permanently down an eighth of its thrust.
		for (int32 Iteration = 0; Iteration < 3; ++Iteration)
		{
			TArray<int32> Free;
			Free.Reserve(Columns.Num());
			for (int32 Index = 0; Index < Columns.Num(); ++Index)
			{
				// How much this thruster would help with what is still missing. A
				// thruster pinned at a bound that this would only push further past
				// it cannot contribute, and including it would waste the correction.
				double Alignment = 0.0;
				for (int32 Axis = 0; Axis < 6; ++Axis)
				{
					Alignment += Columns[Index].Component(Axis) * Remaining.Component(Axis);
				}

				const bool bPinnedClosed = OutThrottles[Index] <= 0.0 && Alignment < 0.0;
				const bool bPinnedOpen = OutThrottles[Index] >= 1.0 && Alignment > 0.0;

				if (!bPinnedClosed && !bPinnedOpen)
				{
					Free.Add(Index);
				}
			}

			if (Free.Num() == 0)
			{
				break;
			}

			// Nothing is pinned on the first pass of an unsaturated request, and the
			// cached rows are exactly the ones wanted. Otherwise pay for a 6x6.
			TArray<FThrusterResponse> FreeRows;
			const TArray<FThrusterResponse>* Rows = &Response;
			if (Free.Num() != Columns.Num())
			{
				if (!ComputeResponseRows(Columns, Free, Lambda, FreeRows))
				{
					// The survivors cannot span six axes. Keep what is already
					// allocated rather than discarding a partial answer.
					break;
				}
				Rows = &FreeRows;
			}

			for (int32 Slot = 0; Slot < Free.Num(); ++Slot)
			{
				const int32 Index = Free[Slot];

				double Delta = 0.0;
				for (int32 Col = 0; Col < 6; ++Col)
				{
					Delta += (*Rows)[Slot].Row[Col] * Remaining.Component(Col);
				}

				OutThrottles[Index] = FMath::Clamp(OutThrottles[Index] + Delta, 0.0, 1.0);
			}

			Remaining = Desired - ComputeAchieved(OutThrottles);
		}
	}

	double FThrustAllocator::GetAxisCapability(int32 Axis) const
	{
		double Total = 0.0;
		for (const FWrench& Column : Columns)
		{
			Total += FMath::Abs(Column.Component(Axis));
		}
		return Total;
	}

	double FThrustAllocator::GetAxisAchievable(int32 Axis) const
	{
		// Split rather than summed in absolute value: a thruster pointing the wrong
		// way for the direction being asked about contributes nothing to it, because
		// its throttle would have to go negative to help.
		double Positive = 0.0;
		double Negative = 0.0;
		for (const FWrench& Column : Columns)
		{
			const double Contribution = Column.Component(Axis);
			Positive += FMath::Max(0.0, Contribution);
			Negative += FMath::Max(0.0, -Contribution);
		}

		// The weaker direction, so the figure holds for a stick pushed either way.
		return FMath::Min(Positive, Negative);
	}

	FWrench FThrustAllocator::ComputeAchieved(const TArray<double>& Throttles) const
	{
		FWrench Total;
		const int32 Count = FMath::Min(Throttles.Num(), Columns.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Total = Total + Columns[Index] * Throttles[Index];
		}
		return Total;
	}
}
