#pragma once

#include "CoreMinimal.h"
#include "ThrusterDef.h"

namespace StarForge
{
	/**
	 * Distributes a demanded force and torque across a redundant thruster array.
	 *
	 * More than six thrusters means infinitely many ways to produce any achievable
	 * wrench, so the solver picks the one with the least total thrust: a damped
	 * pseudo-inverse of the effectiveness matrix. That matrix depends only on where
	 * the thrusters are and where the centre of mass is, so it is assembled once in
	 * Build and reused every frame.
	 *
	 * A throttle is physically confined to [0, 1] -- a thruster cannot run backwards
	 * -- and the unconstrained solution routinely violates that on an array with
	 * opposed pairs. The pseudo-inverse is therefore recomputed over the thrusters
	 * still free to move; see Solve for why re-solving over all of them instead
	 * leaves the ship permanently short of thrust.
	 *
	 * Rebuild when the thruster set changes, which in practice means when one is
	 * destroyed. The ship's handling then degrades on its own, because the geometry
	 * really did change.
	 */
	class STARFORGECORE_API FThrustAllocator
	{
	public:
		/**
		 * Builds the effectiveness matrix and caches its damped pseudo-inverse.
		 *
		 * Returns false when the array cannot span all six axes -- one thruster, or
		 * a set that is collectively unable to yaw, lands here. Callers must check.
		 */
		bool Build(
			const TArray<FThrusterDef>& Thrusters,
			const FVector& CentreOfMass,
			double DampingLambda = 0.01);

		/** Throttles in [0, 1], one per thruster, for the requested wrench. */
		void Solve(const FWrench& Desired, TArray<double>& OutThrottles) const;

		/** The wrench a given set of throttles actually produces. */
		FWrench ComputeAchieved(const TArray<double>& Throttles) const;

		/**
		 * The most this array could possibly produce on one axis: the sum of what
		 * every thruster contributes to it, taken in absolute value.
		 *
		 * Index 0-2 are force axes, 3-5 torque axes. This is the honest scale for
		 * judging a parasitic component, and it is not the same as running every
		 * thruster at once -- a symmetric array at full throttle cancels its own
		 * torque to nothing, so measuring capability that way would compare a
		 * tolerance against zero.
		 */
		double GetAxisCapability(int32 Axis) const;

		int32 Num() const { return Columns.Num(); }
		bool IsBuilt() const { return bBuilt; }

	private:
		/** One row of a pseudo-inverse: how one thruster responds to each axis. */
		struct FThrusterResponse
		{
			double Row[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
		};

		/**
		 * Damped pseudo-inverse rows for the given subset of columns.
		 *
		 * Returns false when that subset cannot span six axes, which is a normal
		 * outcome once enough thrusters have saturated rather than an error.
		 */
		static bool ComputeResponseRows(
			const TArray<FWrench>& InColumns,
			const TArray<int32>& Indices,
			double DampingLambda,
			TArray<FThrusterResponse>& OutRows);

		/** Column i is what thruster i contributes at full throttle. */
		TArray<FWrench> Columns;

		/** Row i maps a demanded wrench to thruster i's throttle, nothing saturated. */
		TArray<FThrusterResponse> Response;

		/** Kept because Solve rebuilds the pseudo-inverse over its free set. */
		double Lambda = 0.01;

		bool bBuilt = false;
	};
}
