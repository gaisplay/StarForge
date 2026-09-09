#include "FlightController.h"

namespace
{
	/** Clamps each component into [-1, 1] without changing the vector's direction. */
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
			const FVector TargetVelocity = LinearAxis * Limits.MaxSpeed;
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
