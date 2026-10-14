#pragma once

#include "CoreMinimal.h"

/**
 * Docking geometry: is the approach inside the window, and where does the hull have to
 * be for the two ports to mate.
 *
 * Length is unit-agnostic and follows whatever the caller passes, the same convention
 * the frame maths uses; the game passes centimetres. Angles are radians. The two fields
 * that name a unit -- DistanceMetres and SpeedMetresPerSec -- are SI, and the crossing
 * happens in the caller, once, as it does everywhere else in this module.
 *
 * A docking port is a socket whose +X points out of the hull and whose +Z is the deck's
 * up. Two ports mate when their origins coincide, their +X oppose and their +Z agree.
 * Three axes, not one: that is what makes the two decks line up, so the gravity
 * direction a person walks through in the airlock changes only by the leftover
 * tolerance (spec 8).
 */
namespace StarForge::Docking
{
	/**
	 * The approach window. Ship data, not constants: spec 8 gives 3 m, 1 m/s, 10 and 10
	 * degrees as starting tolerances, and the component authors them as properties.
	 */
	struct STARFORGECORE_API FDockingLimits
	{
		double DistanceMetres = 3.0;
		double SpeedMetresPerSec = 1.0;
		double ForwardAngleRad = 10.0 * UE_DOUBLE_PI / 180.0;
		double UpAngleRad = 10.0 * UE_DOUBLE_PI / 180.0;
	};

	/** What the approach is, measured. */
	struct STARFORGECORE_API FDockingApproach
	{
		/**
		 * Between the two port origins, in the length unit the transforms came in. The
		 * caller converts to metres before handing this to CanDock; the field is not
		 * named for a unit precisely because it does not have one until then.
		 */
		double Distance = 0.0;

		/**
		 * Carried, not computed. Two poses at one instant cannot say how fast they are
		 * closing; the caller has a velocity instrument and passes what it reads.
		 */
		double SpeedMetresPerSec = 0.0;

		/** Between the ship port's +X and the station port's -X. Zero is nose-on. */
		double ForwardAngleRad = 0.0;

		/** Between the two +Z. Zero is decks parallel. */
		double UpAngleRad = 0.0;
	};

	/**
	 * Which condition refused, or None.
	 *
	 * A bool would be the whole answer thrown away: a pilot refused without being told
	 * why corrects the wrong thing, and the readout that shows him a bool agrees with
	 * him. Same reasoning that gave the turret three states instead of an angle.
	 */
	enum class EDockingRefusal : uint8
	{
		None,
		Distance,
		Speed,
		Forward,
		Up
	};

	/**
	 * The approach between two ports, measured. Nothing in it is refused here: a figure that
	 * cannot be computed comes back not finite, and CanDock refuses it. That includes the
	 * angles, which come back NaN for a port rotation that is not finite rather than the zero
	 * -- nose-on, decks parallel -- that clamping its dot product would make of it.
	 */
	STARFORGECORE_API FDockingApproach ComputeDockingApproach(
		const FTransform& ShipPortWorld,
		const FTransform& StationPortWorld,
		double RelativeSpeedMetresPerSec);

	/**
	 * The first condition of ApproachSI that falls outside Limits, in the order
	 * distance, speed, forward, up -- the order a pilot can act on. Told his nose is
	 * fifteen degrees off at forty metres he corrects an alignment that will be wrong
	 * again on arrival; told he is too far, he closes, and the alignment then becomes a
	 * real question.
	 *
	 * Distance in ApproachSI must already be metres. The name says so because this is
	 * the one function in the file that would be silently wrong with centimetres in it:
	 * every approach would pass the angle checks and fail the distance one by a factor
	 * of a hundred, which reads exactly like a pilot who is simply too far away.
	 */
	STARFORGECORE_API EDockingRefusal CanDock(const FDockingApproach& ApproachSI, const FDockingLimits& Limits);

	/**
	 * Where the hull has to be for its port to mate with the station's.
	 *
	 * ShipPortLocalToHull is the port's transform relative to the hull, so the identity
	 * this inverts is PortWorld = ShipPortLocalToHull * HullWorld -- in Unreal's order,
	 * where A * B means A first and then B, and ChildLocal * ParentWorld is the child's
	 * world. Swap the two and the result is still a transform, still plausible, and
	 * wrong by a rotation and the length of the port's offset from the pivot. The test
	 * that mates the ports afterwards is what catches it.
	 */
	STARFORGECORE_API FTransform TargetHullPose(
		const FTransform& StationPortWorld, const FTransform& ShipPortLocalToHull);

	/**
	 * A pose along the run-in: 0 is Start, 1 is Target, between is linear in position
	 * and slerped in rotation.
	 *
	 * Alpha is clamped, and both ends are returned rather than interpolated to. The last
	 * frame of a capture is the pose the attachment freezes for as long as the ship stays
	 * docked, and nothing corrects it afterwards, so a hull left a float's width short
	 * would stay short.
	 *
	 * How alpha runs with time is the caller's business and deliberately not this
	 * function's: the component eases it, because a linear run-in starts and stops
	 * instantly and a frame that differentiates position twice reads that as a very
	 * large acceleration.
	 */
	STARFORGECORE_API FTransform CapturePose(const FTransform& Start, const FTransform& Target, double Alpha);

	/**
	 * How far the hull has to be eased out along AwayDirection before it is clear of the
	 * station: the smallest t >= 0 such that a sphere of HullRadius about
	 * HullCentre + t * AwayDirection keeps at least HullRadius from StationBounds at t AND
	 * at every larger t. Zero when the whole ray is already clear, when the direction is
	 * zero, and when StationBounds is invalid -- a station with nothing physical in it has
	 * nothing to be cleared of.
	 *
	 * Why this exists (finding F3 of checkpoint 8): docked, the hull's port face lies flush
	 * on the tunnel's end faces, and a hull let go there and thrust sideways drags that face
	 * across them. Chaos keeps a collision margin on the dynamic hull, and the edge-edge
	 * contact it reports as a panel passes the rounded trailing edge is a false penetration
	 * it resolves in one step -- a jump of a centimetre that knocks a standing man down. A
	 * hull that starts clear has nothing to drag across.
	 *
	 * A sphere rather than the box, so the answer does not depend on which way the pilot
	 * turns first. "Every larger t" and not merely "at t", because a ray that starts clear
	 * can still pass through the station; the answer is then the far side, not zero.
	 *
	 * StationBounds is axis-aligned in the frame the hull is given in; for a station that is
	 * not aligned with that frame, the overload below.
	 *
	 * AwayDirection is normalised here, so its length is irrelevant. Lengths are
	 * unit-agnostic like the rest of this file; the game passes metres, because the answer
	 * is compared against DistanceMetres and turned into an acceleration.
	 */
	STARFORGECORE_API double ReleaseStandoff(
		const FVector& HullCentre, double HullRadius, const FVector& AwayDirection, const FBox& StationBounds);

	/**
	 * The same standoff, with the station's box given in the station's own frame and the
	 * hull in the world's. The box above is axis-aligned in whatever frame the hull is given
	 * in, and a station's panels are axis-aligned in the station's frame, not the world's: a
	 * world-aligned box round a yawed station grows with the yaw, and so would the standoff,
	 * for the same geometry. Round the rig's station yawed three degrees it asks 3.26 m where
	 * 2.93 is enough, which is outside the 3.0 m window. A place asset can yaw its station
	 * (FSFPlaceCarrier::Rotation), so the hull is taken into the station's frame instead and
	 * the box is left tight.
	 *
	 * StationFrame's scale is ignored: StationLocalBounds must be in the hull's lengths,
	 * measured along the frame's rotated axes from its origin. The answer is a length along
	 * AwayDirection, the same in either frame.
	 */
	STARFORGECORE_API double ReleaseStandoff(
		const FVector& HullCentre, double HullRadius, const FVector& AwayDirection,
		const FTransform& StationFrame, const FBox& StationLocalBounds);

	/**
	 * The peak acceleration of a smoothstep path of Distance covered in Seconds: 6d/T^2,
	 * reached at both ends of the path. A positive distance in a non-positive or non-finite
	 * duration is a jump in one frame and answers the largest double, so it reads as over
	 * any limit; zero distance answers zero. Distance's unit per second squared comes out.
	 */
	STARFORGECORE_API double SmoothstepPeakAcceleration(double Distance, double Seconds);
}
