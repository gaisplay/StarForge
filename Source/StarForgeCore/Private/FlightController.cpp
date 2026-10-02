#include "FlightController.h"

namespace
{
	/**
	 * Clamps each component into [-1, 1]: a full deflection of one axis is one, whatever the others
	 * do. Per component, so an input beyond full travel on some axes but not others comes back
	 * pointing a different way; the assist's speed limit is applied to the vector's length below.
	 */
	FVector ClampAxes(const FVector& Axes)
	{
		return FVector(
			FMath::Clamp(Axes.X, -1.0, 1.0),
			FMath::Clamp(Axes.Y, -1.0, 1.0),
			FMath::Clamp(Axes.Z, -1.0, 1.0));
	}
}

namespace StarForge
{
	FWrench ComputeDemandedWrench(
		const FFlightIntent& Intent,
		const FRigidBodyState& State,
		const FShipMassProperties& Mass,
		const FFlightLimits& Limits,
		const FWrench& ArrayCapability)
	{
		const FVector LinearAxis = ClampAxes(Intent.LinearAxis);
		const FVector AngularAxis = ClampAxes(Intent.AngularAxis);

		FWrench Demand;

		if (Intent.bFlightAssistEnabled)
		{
			// The stick asks for a velocity, and the demand is whatever closes the gap.
			// Releasing it therefore asks for zero velocity, which is why the ship
			// stops instead of coasting.
			//
			// The speed limit is on the length, decided by the owner on 2026-10-01: spec 7's 200 m/s
			// is a speed, not a speed per axis. Scaled per axis, full stick on two axes asked for
			// 283 m/s and on three for 346, and the hand-flown fight of checkpoint 7 reached
			// 276 m/s on a diagonal. A deflection of one axis has length one or less and comes back
			// untouched, bit for bit (GetClampedToMaxSize returns the vector itself unless it is
			// longer), so every flight along one axis reads as before. This limits the speed asked
			// for, not the push towards it: from rest a diagonal still saturates both axes of the
			// array, 22.6 m/s2 on 50 t, until the demand is cut to what the array can deliver.
			const FVector TargetVelocity = LinearAxis.GetClampedToMaxSize(1.0) * Limits.MaxSpeed;
			Demand.Force = (TargetVelocity - State.LinearVelocity) * Mass.Mass * Limits.LinearGain;

			// Componentwise: each axis has its own ceiling.
			const FVector TargetAngular = AngularAxis * Limits.MaxAngularSpeed;
			const FVector AngularError = TargetAngular - State.AngularVelocity;
			Demand.Torque = Mass.TransformByInertia(AngularError * Limits.AngularGain);
		}
		else
		{
			// The stick asks for thrust directly, as a fraction of what this array can
			// actually deliver. Scaling by anything else -- mass times a speed limit,
			// say -- leaves the thrusters saturated across most of the stick travel,
			// which a gamepad hides and a physical stick does not.
			Demand.Force = LinearAxis * ArrayCapability.Force;
			Demand.Torque = AngularAxis * ArrayCapability.Torque;
		}

		// Euler gyroscopic term. Present in both modes: it is a property of a
		// spinning rigid body, not of the assist.
		const FVector AngularMomentum = Mass.TransformByInertia(State.AngularVelocity);
		Demand.Torque += FVector::CrossProduct(State.AngularVelocity, AngularMomentum);

		return Demand;
	}
}
