#pragma once

#include "CoreMinimal.h"

namespace StarForge
{
	/**
	 * A dense 6x6 matrix in double precision.
	 *
	 * Core stops at 4x4, and thrust allocation needs exactly six: three force axes
	 * and three torque axes. Six is also where it stops -- the matrix that has to be
	 * inverted is always 6x6 no matter how many thrusters a ship carries, because it
	 * is B times B transposed rather than B itself.
	 */
	struct STARFORGECORE_API FMatrix6
	{
		static constexpr int32 Size = 6;

		double M[Size][Size];

		static FMatrix6 Zero();
		static FMatrix6 Identity();

		double& At(int32 Row, int32 Col) { return M[Row][Col]; }
		double At(int32 Row, int32 Col) const { return M[Row][Col]; }

		FMatrix6 operator*(const FMatrix6& Other) const;

		/**
		 * Gauss-Jordan inversion with partial pivoting.
		 *
		 * Returns false and leaves Out untouched when the matrix is singular to
		 * working precision. Callers must check: a thruster layout that cannot
		 * produce all six axes lands here, and silently returning nonsense would
		 * surface much later as a ship that will not turn.
		 */
		bool TryInvert(FMatrix6& Out) const;
	};
}
