#pragma once

#include "CoreMinimal.h"

/**
 * Where a camera has to stand to see the whole of an object, and how a view's two fields
 * relate to each other.
 *
 * Unit-agnostic in length, like the frame maths: a distance comes back in whatever unit the
 * radius went in, and the game passes centimetres. Angles are radians. A field of view is the
 * FULL angle, edge to edge, which is what UCameraComponent::FieldOfView holds (in degrees).
 */
namespace StarForge::View
{
	/**
	 * The vertical field of a view with the given horizontal field and aspect (width over
	 * height): 2 atan(tan(H / 2) / A).
	 *
	 * The aspect divides the half-angle TANGENTS, not the angles, because both fields are cut
	 * out of one flat image plane and the aspect is a ratio of lengths on it. 90 degrees at 16:9
	 * is 58.7 degrees vertical, where dividing the angle would say 50.6. It is the conversion the
	 * engine makes itself when the local player keeps the vertical field, its default
	 * (CameraStackTypes.cpp), so the field this returns is the one on screen.
	 *
	 * No guard: FitSphereDistance is where out-of-range input is refused.
	 */
	STARFORGECORE_API double VerticalFov(double HorizontalFov, double AspectRatio);

	/**
	 * How far from a sphere's centre a camera looking at that centre must stand for the sphere
	 * to just fit the view: R / sin(F / 2), where F is the narrower of the two fields.
	 *
	 * The sine, not the tangent. Seen from a finite distance a sphere's outline is the cone
	 * tangent to it, of half-angle asin(R / d), and that cone is wider than the sphere's
	 * centre-plane section. R / tan(F / 2) fits the section and clips the sphere.
	 *
	 * The narrower field, because a sphere is round: it fits the view when it fits the tighter
	 * of the two, and landscape and portrait differ only in which one that is.
	 *
	 * 0 for a radius that is not positive and finite, a horizontal field outside (0, pi), or an
	 * aspect that is not positive. There is no distance from which such a view shows the sphere,
	 * and zero is the one answer no caller can mistake for a fit.
	 */
	STARFORGECORE_API double FitSphereDistance(double Radius, double HorizontalFov, double AspectRatio);

	/**
	 * A chase camera's pose, in the frame the centre is given in: behind the centre along -X,
	 * raised by Elevation toward +Z, at FitSphereDistance from it, and looking straight at it.
	 *
	 * Location = Centre + d (-cos e, 0, sin e). Rotation: pitch -e, yaw 0, roll 0 -- down by
	 * exactly the elevation, so the view axis runs from the location through the centre and the
	 * camera's right stays the frame's +Y.
	 *
	 * The identity when d is 0. A pose "at distance zero" would put the camera at the centre of
	 * the object it is meant to show, and the identity is what a caller that forgot to check d
	 * would have had anyway.
	 */
	STARFORGECORE_API FTransform ChaseCameraPose(
		const FVector& Centre,
		double Radius,
		double HorizontalFov,
		double AspectRatio,
		double Elevation);
}
