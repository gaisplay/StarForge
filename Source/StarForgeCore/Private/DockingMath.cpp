#include "DockingMath.h"

namespace StarForge::Docking
{
	namespace
	{
		/**
		 * The unsigned angle between two unit vectors.
		 *
		 * The clamp is not defensive decoration. A dot product of two normalised vectors
		 * comes back at 1.0000000000000002 often enough, and Acos of that is NaN -- which
		 * propagates into the approach and compares false against every limit, so a
		 * refusal written as `Value > Limit` falls through every one of them and grants
		 * permission to dock on a figure nobody computed. CanDock's guards are negated
		 * against exactly that; the clamp is what keeps a rounding error from ever having
		 * to depend on them.
		 *
		 * It is not itself a filter for non-finite input and must not be read as one:
		 * Clamp(NaN, -1, 1) returns 1.0, whose Acos is zero -- perfectly aligned, the
		 * most permissive answer there is. A genuinely non-finite approach reaches CanDock
		 * by way of the distance or the carried speed, and is refused there.
		 */
		double AngleBetween(const FVector& A, const FVector& B)
		{
			return FMath::Acos(FMath::Clamp(FVector::DotProduct(A, B), -1.0, 1.0));
		}
	}

	FDockingApproach ComputeDockingApproach(
		const FTransform& ShipPortWorld,
		const FTransform& StationPortWorld,
		double RelativeSpeedMetresPerSec)
	{
		FDockingApproach Approach;

		Approach.Distance = FVector::Dist(ShipPortWorld.GetLocation(), StationPortWorld.GetLocation());
		Approach.SpeedMetresPerSec = RelativeSpeedMetresPerSec;

		// Mated means the two +X oppose, so the angle is taken against the station's
		// NEGATED forward. Measured against its forward instead, a perfect approach would
		// read 180 degrees and a ship reversing into the station at speed would read zero.
		Approach.ForwardAngleRad = AngleBetween(
			ShipPortWorld.GetUnitAxis(EAxis::X), -StationPortWorld.GetUnitAxis(EAxis::X));

		// The decks, on the other hand, agree rather than oppose: both +Z point away from
		// their own floor, and a person walking through the airlock has to keep his feet
		// on the same side of the world.
		Approach.UpAngleRad = AngleBetween(
			ShipPortWorld.GetUnitAxis(EAxis::Z), StationPortWorld.GetUnitAxis(EAxis::Z));

		return Approach;
	}

	EDockingRefusal CanDock(const FDockingApproach& ApproachSI, const FDockingLimits& Limits)
	{
		// Closed intervals throughout: exactly on a limit is inside it. A pilot who has
		// flown to precisely three metres should not be sent away from three metres.
		//
		// Each condition is the negation of "inside", not the plain `> Limit` that reads
		// the same, and the two differ exactly where this function must not be wrong. A
		// non-finite figure compares false against both, so `> Limit` falls through all
		// four guards and answers None -- permission to dock, granted on a question that
		// could not be evaluated. Negated, it fails the first condition instead. Both
		// paths in are reachable: the speed is carried from the caller's instrument
		// untouched, and a port location that is not finite carries into FVector::Dist --
		// as NaN if a coordinate is NaN, as +Infinity if one is merely infinite. The
		// negation refuses both; `> Limit` would refuse only the infinite one.
		if (!(ApproachSI.Distance <= Limits.DistanceMetres))
		{
			return EDockingRefusal::Distance;
		}
		if (!(ApproachSI.SpeedMetresPerSec <= Limits.SpeedMetresPerSec))
		{
			return EDockingRefusal::Speed;
		}
		if (!(ApproachSI.ForwardAngleRad <= Limits.ForwardAngleRad))
		{
			return EDockingRefusal::Forward;
		}
		if (!(ApproachSI.UpAngleRad <= Limits.UpAngleRad))
		{
			return EDockingRefusal::Up;
		}

		return EDockingRefusal::None;
	}

	FTransform TargetHullPose(const FTransform& StationPortWorld, const FTransform& ShipPortLocalToHull)
	{
		// The pose the ship's port must take: the station's port turned half a turn about
		// its own Z. That sends the station's +X onto its -X and leaves its +Z alone,
		// which is the mating condition written as a transform. The origin is untouched,
		// because a rotation with no translation composed on the left contributes none.
		const FTransform Flip(FQuat(FVector::UpVector, UE_DOUBLE_PI));
		const FTransform MatedPortWorld = Flip * StationPortWorld;

		// PortWorld = ShipPortLocalToHull * HullWorld, so HullWorld is the port's local
		// transform undone from the left. Written this way round and no other; see the
		// header.
		return ShipPortLocalToHull.Inverse() * MatedPortWorld;
	}

	FTransform CapturePose(const FTransform& Start, const FTransform& Target, double Alpha)
	{
		// Negated for the reason CanDock's guards are: a non-finite alpha is false against
		// both ends of the range and would fall through into the interpolation, freezing
		// the hull at a pose of NaNs for as long as it stays docked.
		if (!(Alpha > 0.0))
		{
			return Start;
		}
		if (Alpha >= 1.0)
		{
			return Target;
		}

		FTransform Pose;
		Pose.SetLocation(FMath::Lerp(Start.GetLocation(), Target.GetLocation(), Alpha));
		Pose.SetRotation(FQuat::Slerp(Start.GetRotation(), Target.GetRotation(), Alpha).GetNormalized());

		// Scale comes from the start and is never interpolated. Nothing in this project
		// scales a hull, and a capture that quietly rescaled one would move every wall,
		// every seat and every spawn point with it.
		Pose.SetScale3D(Start.GetScale3D());

		return Pose;
	}
}
