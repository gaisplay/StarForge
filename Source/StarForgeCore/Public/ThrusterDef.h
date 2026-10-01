#pragma once

#include "CoreMinimal.h"

namespace StarForge
{
	/**
	 * A thruster described by geometry rather than by tuning coefficients.
	 *
	 * Everything a ship can do follows from where its thrusters are and which way
	 * they push. Nothing here is a feel knob: change the layout and the handling
	 * changes with it, including after a thruster is shot away.
	 *
	 * SI throughout -- metres, newtons, seconds.
	 */
	struct STARFORGECORE_API FThrusterDef
	{
		/** Position in the ship's own frame, metres. */
		FVector Location = FVector::ZeroVector;

		/** Unit vector along which this thruster pushes the ship. */
		FVector Direction = FVector::ForwardVector;

		/** Thrust at full throttle, newtons. */
		double MaxThrust = 0.0;

		/**
		 * The time constant of the spool's first-order lag, seconds: a throttle stepped from
		 * zero reaches 63% of full after one ResponseTime and 99.9% only after about 6.9 of
		 * them, so it is not a time to full. Consumed by the engine bridge, not here.
		 */
		double ResponseTime = 0.05;
	};

	/**
	 * Mass properties as the solver needs them.
	 *
	 * Chaos reports inertia as a diagonal in principal axes plus the rotation into
	 * those axes, so both halves are kept: using the diagonal without the rotation
	 * would quietly mispredict every turn on a ship whose principal axes are not
	 * aligned with its model.
	 */
	struct STARFORGECORE_API FShipMassProperties
	{
		/** Kilograms. */
		double Mass = 1.0;

		/** Centre of mass in the ship's frame, metres. */
		FVector CentreOfMass = FVector::ZeroVector;

		/** Principal moments of inertia, kg m^2. */
		FVector InertiaDiagonal = FVector(1.0, 1.0, 1.0);

		/** Rotation from the ship's frame into the principal axes. */
		FQuat InertiaRotation = FQuat::Identity;

		/** Applies the inertia tensor to an angular vector expressed in the ship's frame. */
		FVector TransformByInertia(const FVector& AngularShipFrame) const
		{
			const FVector InPrincipal = InertiaRotation.UnrotateVector(AngularShipFrame);
			const FVector Scaled = InPrincipal * InertiaDiagonal;
			return InertiaRotation.RotateVector(Scaled);
		}
	};

	/**
	 * A force and a torque acting together, both in the ship's frame.
	 *
	 * Six numbers is the natural size of everything in this system: what a pilot
	 * asks for, what one thruster contributes, and what the whole array achieves.
	 */
	struct STARFORGECORE_API FWrench
	{
		/** Newtons. */
		FVector Force = FVector::ZeroVector;

		/** Newton-metres. */
		FVector Torque = FVector::ZeroVector;

		/** Index 0-2 are the force axes, 3-5 the torque axes. */
		double Component(int32 Index) const
		{
			return (Index < 3) ? Force[Index] : Torque[Index - 3];
		}

		FWrench operator+(const FWrench& Other) const
		{
			FWrench Result;
			Result.Force = Force + Other.Force;
			Result.Torque = Torque + Other.Torque;
			return Result;
		}

		FWrench operator-(const FWrench& Other) const
		{
			FWrench Result;
			Result.Force = Force - Other.Force;
			Result.Torque = Torque - Other.Torque;
			return Result;
		}

		FWrench operator*(double Scale) const
		{
			FWrench Result;
			Result.Force = Force * Scale;
			Result.Torque = Torque * Scale;
			return Result;
		}
	};

	/** What one thruster contributes at full throttle, in the ship's frame. */
	STARFORGECORE_API FWrench ComputeThrusterWrench(
		const FThrusterDef& Thruster,
		const FVector& CentreOfMass);
}
