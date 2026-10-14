#include "DockingMath.h"

#include <limits>

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
		 * It is not a filter for non-finite input, so a dot product that is not finite is
		 * kept away from it: Clamp returns 1.0 for a NaN and for +Inf, whose Acos is zero --
		 * perfectly aligned, the most permissive answer there is. A port rotation gone NaN
		 * arrives here with its location still finite, so neither the distance nor the
		 * carried speed refuses it first; the angle goes out as NaN instead, and CanDock's
		 * negated guards refuse it.
		 */
		double AngleBetween(const FVector& A, const FVector& B)
		{
			const double Dot = FVector::DotProduct(A, B);
			if (!FMath::IsFinite(Dot))
			{
				return std::numeric_limits<double>::quiet_NaN();
			}
			return FMath::Acos(FMath::Clamp(Dot, -1.0, 1.0));
		}

		/**
		 * Steps for both searches below. They shrink the bracket by 2^-128 (bisection) and by
		 * (2/3)^128, about 3e-23 (the ternary search for the minimum). A bracket is the
		 * distance from the hull to the far side of the station plus the hull's radius, tens
		 * of metres on the rig, so either is far below a micrometre -- the limit is the double
		 * spacing of the positions themselves, not the step count. A fixed count keeps the
		 * answer deterministic.
		 */
		constexpr int32 SearchIterations = 128;

		bool IsFiniteVector(const FVector& V)
		{
			return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
		}

		bool IsFiniteBox(const FBox& Box)
		{
			return IsFiniteVector(Box.Min) && IsFiniteVector(Box.Max);
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
		// could not be evaluated. Negated, it fails the first condition instead. Every
		// path in is reachable: the speed is carried from the caller's instrument
		// untouched, a port location that is not finite carries into FVector::Dist --
		// as NaN if a coordinate is NaN, as +Infinity if one is merely infinite -- and a
		// port rotation that is not finite comes out of AngleBetween as two NaN angles.
		// The negation refuses all of them; `> Limit` would refuse only the infinite one.
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

	double ReleaseStandoff(
		const FVector& HullCentre, double HullRadius, const FVector& AwayDirection, const FBox& StationBounds)
	{
		const FVector Dir = AwayDirection.GetSafeNormal();

		// Negated for CanDock's reason: a NaN radius must not fall through into a search.
		if (Dir.IsZero() || !StationBounds.IsValid || !(HullRadius > 0.0))
		{
			return 0.0;
		}

		// Squared on both sides, so no square root is taken in the loops below.
		const double ClearSq = HullRadius * HullRadius;
		const auto GapSq = [&HullCentre, &Dir, &StationBounds](double T)
		{
			return StationBounds.ComputeSquaredDistanceToPoint(HullCentre + T * Dir);
		};

		// Past this the centre is further from the box's centre than the box's half-diagonal
		// plus the radius, so the sphere is clear here and at every t beyond: the bracket's
		// upper end is clear by construction, not by a guess at a large number.
		const double Beyond = FVector::Dist(HullCentre, StationBounds.GetCenter())
			+ StationBounds.GetExtent().Size() + HullRadius;

		// The distance from a point on a line to a convex box is convex in t, so the set of
		// t that are too close is a single interval, and bisection needs one point inside it
		// to start from. At every undock that point is t = 0 -- the hull starts flush on the
		// station. A hull that starts clear may still be heading through the station, so the
		// gap's minimum along the ray is found first, and only if even that is clear is the
		// answer zero.
		double Inside = 0.0;
		if (!(GapSq(0.0) < ClearSq))
		{
			double Lo = 0.0;
			double Hi = Beyond;
			for (int32 Step = 0; Step < SearchIterations; ++Step)
			{
				const double A = Lo + (Hi - Lo) / 3.0;
				const double B = Hi - (Hi - Lo) / 3.0;
				if (GapSq(A) < GapSq(B))
				{
					Hi = B;
				}
				else
				{
					Lo = A;
				}
			}
			Inside = 0.5 * (Lo + Hi);
			if (!(GapSq(Inside) < ClearSq))
			{
				return 0.0;
			}
		}

		// From inside the interval to beyond it the predicate "too close" is true and then
		// false, once. Hi is returned rather than the midpoint, so the answer is always a t
		// at which the sphere has been measured clear.
		double Lo = Inside;
		double Hi = Beyond;
		for (int32 Step = 0; Step < SearchIterations; ++Step)
		{
			const double Mid = 0.5 * (Lo + Hi);
			if (GapSq(Mid) < ClearSq)
			{
				Lo = Mid;
			}
			else
			{
				Hi = Mid;
			}
		}
		return Hi;
	}

	double ReleaseStandoff(
		const FVector& HullCentre, double HullRadius, const FVector& AwayDirection,
		const FTransform& StationFrame, const FBox& StationLocalBounds)
	{
		// NoScale on both: a rigid change of frame keeps every distance, so the standoff
		// measured in the station's frame is the standoff in the world's.
		return ReleaseStandoff(
			StationFrame.InverseTransformPositionNoScale(HullCentre),
			HullRadius,
			StationFrame.InverseTransformVectorNoScale(AwayDirection),
			StationLocalBounds);
	}

	double SmoothstepPeakAcceleration(double Distance, double Seconds)
	{
		// x = d * s(t/T) with s(a) = 3a^2 - 2a^3, so x'' = d * (6 - 12a) / T^2, largest in
		// magnitude at a = 0 and a = 1.
		if (Distance == 0.0)
		{
			return 0.0;
		}
		// No duration is a jump in one frame, which is what the docking component does with a
		// non-positive CaptureSeconds. Its acceleration is unbounded, and answering zero would
		// hide it from the grav-plate check that exists for it.
		if (!(Seconds > 0.0))
		{
			return TNumericLimits<double>::Max();
		}
		return 6.0 * Distance / (Seconds * Seconds);
	}

	double BoundsPastPortPlane(const FBox& Bounds, const FVector& PortOrigin, const FVector& PortOutwardAxis)
	{
		const FVector Axis = PortOutwardAxis.GetSafeNormal();

		// Before the box, so that an empty box cannot answer a calm zero to a port nobody could measure.
		// GetSafeNormal hands a NaN or infinite axis back with a NaN in it, which IsZero does not catch.
		if (Axis.IsZero() || !IsFiniteVector(Axis) || !IsFiniteVector(PortOrigin))
		{
			return std::numeric_limits<double>::quiet_NaN();
		}
		if (!Bounds.IsValid)
		{
			return 0.0;
		}

		// The support of a box along a unit axis: the centre's projection plus the half-extents, each
		// weighted by how much of the axis lies along it. A NaN anywhere in the box carries through,
		// and FMath::Max(0, NaN) answers the NaN, so a broken box stays red.
		const FVector Centre = Bounds.GetCenter();
		const FVector Extent = Bounds.GetExtent();
		const double Support = FVector::DotProduct(Centre - PortOrigin, Axis)
			+ FMath::Abs(Axis.X) * Extent.X
			+ FMath::Abs(Axis.Y) * Extent.Y
			+ FMath::Abs(Axis.Z) * Extent.Z;

		return FMath::Max(0.0, Support);
	}

	double BoxReachOutside(const FBox& Inner, const FBox& Envelope)
	{
		if (!Inner.IsValid || !Envelope.IsValid || !IsFiniteBox(Inner) || !IsFiniteBox(Envelope))
		{
			return std::numeric_limits<double>::quiet_NaN();
		}

		double Worst = 0.0;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Worst = FMath::Max(Worst, Envelope.Min[Axis] - Inner.Min[Axis]);
			Worst = FMath::Max(Worst, Inner.Max[Axis] - Envelope.Max[Axis]);
		}
		return Worst;
	}

	bool IsRedAtPrintedResolution(double Metres)
	{
		// Negated for CanDock's reason: a NaN fails every comparison, and "not under the threshold"
		// is the spelling that makes it red.
		return !(Metres < RedAtOrAboveMetres);
	}
}
