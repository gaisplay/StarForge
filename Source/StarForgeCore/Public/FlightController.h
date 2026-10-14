#pragma once

#include "CoreMinimal.h"
#include "ThrusterDef.h"

namespace StarForge
{
	/**
	 * What an operator asks the ship to do, in dimensionless axes.
	 *
	 * Deliberately unitless, because the meaning depends on the mode: with assist on
	 * these are fractions of the speed limits, with assist off they are fractions of
	 * the wrench the thrusters can produce. The operator fills the same structure
	 * either way and the controller interprets it -- which is why turning assist off
	 * is a different reading of the stick rather than a separate flight model.
	 */
	struct STARFORGECORE_API FFlightIntent
	{
		/** Ship-frame axes, each component in [-1, 1]. */
		FVector LinearAxis = FVector::ZeroVector;

		/**
		 * Angular command as an angular velocity vector: X roll, Y pitch, Z yaw.
		 * Each component in [-1, 1].
		 *
		 * Spec 4 names these pitch, yaw and roll in its prose. That is what they do,
		 * not the order they sit in -- the order here is the one FVector uses for
		 * angular velocity everywhere else, including what Chaos hands back.
		 */
		FVector AngularAxis = FVector::ZeroVector;

		bool bFlightAssistEnabled = true;
	};

	/** The envelope the assist holds the ship inside. */
	struct STARFORGECORE_API FFlightLimits
	{
		/** Metres per second, the length of the velocity the assist may ask for -- not a limit per axis. */
		double MaxSpeed = 200.0;

		/**
		 * Radians per second, as an angular velocity vector: X roll, Y pitch, Z yaw.
		 *
		 * Three numbers, not one. Spec 7 gives roll 180 degrees per second and pitch
		 * and yaw 120, and a single scalar silently picks one of them for all three.
		 */
		FVector MaxAngularSpeed = FVector(
			UE_DOUBLE_PI,
			2.0 * UE_DOUBLE_PI / 3.0,
			2.0 * UE_DOUBLE_PI / 3.0);

		/** How hard the assist corrects a linear velocity error, per second. */
		double LinearGain = 2.0;

		/** How hard the assist corrects an angular velocity error, per second. */
		double AngularGain = 3.0;
	};

	/** Ship motion as the controller needs it, all in the ship's own frame. */
	struct STARFORGECORE_API FRigidBodyState
	{
		/** Metres per second. */
		FVector LinearVelocity = FVector::ZeroVector;

		/** Radians per second. */
		FVector AngularVelocity = FVector::ZeroVector;
	};

	/**
	 * Turns an intent into the force and torque the thrusters should be asked for.
	 *
	 * The torque includes Euler's gyroscopic term, omega cross I omega: a body spinning
	 * about anything but a principal axis needs that torque to keep its spin, and the
	 * controller supplies it so the assist tracks the rate it was asked for. It is right
	 * only on a plant that has Euler dynamics, which is why ASFTestShip turns Chaos's
	 * gyroscopic integration on; without it the term is a disturbance, and an elongated
	 * hull asked for full roll and pitch settled at 180 / 76 / 58 deg/s.
	 *
	 * ArrayCapability is what the stick is a fraction of when assist is off, so it
	 * must be what the array can actually deliver on each axis: the allocator's
	 * GetAxisAchievable, not GetAxisCapability. Handing in the latter doubles the
	 * demand and kills the upper half of the stick's travel. It arrives as six
	 * numbers rather than as a reference to the solver, so this loop stays ignorant
	 * of how thrust is distributed.
	 */
	STARFORGECORE_API FWrench ComputeDemandedWrench(
		const FFlightIntent& Intent,
		const FRigidBodyState& State,
		const FShipMassProperties& Mass,
		const FFlightLimits& Limits,
		const FWrench& ArrayCapability);
}
