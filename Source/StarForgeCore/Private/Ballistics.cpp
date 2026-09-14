#include "Ballistics.h"

namespace StarForge::Ballistics
{
	namespace
	{
		/**
		 * Every component finite. FVector::ContainsNaN is not used because its name promises
		 * less than has to be checked here: an infinite velocity is not a NaN, and it would
		 * carry the projectile to infinity in one step.
		 */
		bool IsFinite(const FVector& V)
		{
			return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
		}
	}

	FVector ComputeMuzzleVelocity(const FVector& CarrierVelocity, const FVector& BarrelDirection, double MuzzleSpeed)
	{
		return CarrierVelocity + BarrelDirection.GetSafeNormal() * MuzzleSpeed;
	}

	bool StepProjectile(const FProjectileState& State, double DeltaSeconds, double MaxAgeSeconds, FProjectileStep& Out)
	{
		// Negated, so a NaN fails the guard instead of falling through it. Out is left untouched
		// on every refusal, the same contract ComputeBearing keeps.
		if (!(DeltaSeconds > 0.0)) { return false; }
		if (!(MaxAgeSeconds > 0.0)) { return false; }
		if (!IsFinite(State.Position) || !IsFinite(State.Velocity)) { return false; }
		if (!FMath::IsFinite(State.AgeSeconds)) { return false; }

		// Clipped to the life, so range is speed times life at any frame rate. See the header.
		const double Remaining = MaxAgeSeconds - State.AgeSeconds;
		if (!(Remaining > 0.0)) { return false; }
		const double Dt = FMath::Min(DeltaSeconds, Remaining);

		Out.SegmentStart = State.Position;
		Out.SegmentEnd = State.Position + State.Velocity * Dt;
		Out.NextState.Position = Out.SegmentEnd;
		Out.NextState.Velocity = State.Velocity;
		Out.NextState.AgeSeconds = State.AgeSeconds + Dt;

		// Negated again: an age that has gone non-finite reads as expired, so the projectile
		// dies rather than living forever. Failing safe here means failing quiet, and a
		// projectile that never expires is exactly the thing nobody would look for.
		Out.bExpired = !(Out.NextState.AgeSeconds < MaxAgeSeconds);

		return true;
	}
}
