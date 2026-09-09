#pragma once

#include "CoreMinimal.h"

namespace StarForge
{
	/**
	 * What a gunner asks of a turret.
	 *
	 * Spec 4 gives this two fields, a world position and a trigger. A third is added
	 * here: bHasTarget. A gunner with nothing to shoot at cannot say so through a
	 * position alone, because a zeroed position is the world origin, which is a legal
	 * place for a target to be -- the turret would swing to it and report itself on
	 * target, and the readout would look exactly as it does with a real target there.
	 * The same reasoning put "return false" on the operator interface instead of an
	 * empty intent.
	 *
	 * The position is in whatever length unit the caller uses consistently; the game
	 * passes centimetres, as it does to the frame maths.
	 */
	struct STARFORGECORE_API FTurretIntent
	{
		FVector TargetWorldLocation = FVector::ZeroVector;
		bool bHasTarget = false;
		bool bFiring = false;
	};
}

/**
 * Turret aiming. Unit-agnostic in length; angles in radians.
 *
 * Yaw is about the mount's Z, positive from +X toward +Y; pitch is elevation above
 * the mount's XY plane, positive toward +Z. Those are the engine's own conventions
 * for FRotator, which is why AimToRotator lives in this file and under test: the
 * barrel the game turns by that rotator has to point where these numbers say.
 */
namespace StarForge::Turret
{
	/** The stops and the slew rate. A mount without a yaw stop is not this mount. */
	struct STARFORGECORE_API FTurretLimits
	{
		/** Yaw travel each way from straight ahead, radians. At most pi. */
		double MaxYawRad = 2.0 * UE_DOUBLE_PI / 3.0;

		/** Radians; negative is below the mount's plane. */
		double MinPitchRad = -UE_DOUBLE_PI / 6.0;
		double MaxPitchRad = UE_DOUBLE_PI / 3.0;

		/** Radians per second, on each axis independently. */
		double TraverseRateRadPerSec = UE_DOUBLE_PI / 2.0;

		/** Within this of the bearing on both axes counts as on target. Radians. */
		double OnTargetToleranceRad = 0.5 * UE_DOUBLE_PI / 180.0;
	};

	/** Where the barrel points, in the mount's frame. */
	struct STARFORGECORE_API FTurretAim
	{
		double YawRad = 0.0;
		double PitchRad = 0.0;
	};

	/**
	 * Three answers, not one. A turret sitting at its stop is at the same angle whether
	 * the target is just inside the stop or far beyond it, so an angle alone cannot say
	 * whether the barrel is on the target or merely as close as it can get. CannotBear
	 * is that second case, said out loud.
	 */
	enum class EAimState : uint8
	{
		OnTarget,
		Slewing,
		CannotBear
	};

	struct STARFORGECORE_API FAimStep
	{
		FTurretAim Aim;
		EAimState State = EAimState::Slewing;
	};

	/**
	 * The mount-frame yaw and pitch of a world position. Returns false, and leaves
	 * OutBearing untouched, when the target sits on the mount itself: there is no
	 * direction to a point you are standing on, and any angle returned for it would be
	 * a fabrication.
	 */
	STARFORGECORE_API bool ComputeBearing(
		const FTransform& MountWorld,
		const FVector& TargetWorld,
		FTurretAim& OutBearing);

	/** True when the bearing lies inside the stops on both axes. */
	STARFORGECORE_API bool CanBear(const FTurretAim& Bearing, const FTurretLimits& Limits);

	/**
	 * One tick of the turret's approach to a bearing.
	 *
	 * Each axis moves toward the bearing by at most the traverse rate times
	 * DeltaSeconds, and never past its stop. A bearing outside the stops is approached
	 * to the nearest stop, and the state says CannotBear for the whole of that
	 * approach, not only on arrival -- the answer is about the target, not about how
	 * far the barrel has got.
	 *
	 * Yaw always moves through zero, because the stops make the reachable arc a single
	 * interval containing zero. With MaxYawRad at exactly pi that is still the rule, so
	 * a target at +179 degrees seen from -179 is reached the long way round. A ring
	 * mount with no stop would need a different function; this one is not it.
	 */
	STARFORGECORE_API FAimStep StepAim(
		const FTurretAim& Current,
		const FTurretAim& Bearing,
		const FTurretLimits& Limits,
		double DeltaSeconds);

	/**
	 * The rotator that points a barrel along an aim: pitch and yaw in degrees, no
	 * roll. FRotator's constructor takes (Pitch, Yaw, Roll) in that order, and getting
	 * two of those swapped is a barrel that points somewhere the readout does not say.
	 */
	STARFORGECORE_API FRotator AimToRotator(const FTurretAim& Aim);
}
