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
		const FFlightLimits& Limits)
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

			const FVector TargetAngular = AngularAxis * Limits.MaxAngularSpeed;
			const FVector AngularError = TargetAngular - State.AngularVelocity;
			Demand.Torque = Mass.TransformByInertia(AngularError * Limits.AngularGain);
		}
		else
		{
			// The stick asks for thrust directly. Nothing holds a speed, so the ship
			// keeps accelerating and keeps whatever spin it is given -- the same pipe
			// below, a different reading of the same numbers above.
			Demand.Force = LinearAxis * Mass.Mass * Limits.MaxSpeed * Limits.LinearGain;
			Demand.Torque = Mass.TransformByInertia(
				AngularAxis * Limits.MaxAngularSpeed * Limits.AngularGain);
		}

		// Euler's gyroscopic term. Present in both modes: it is a property of a
		// spinning rigid body, not of the assist.
		const FVector AngularMomentum = Mass.TransformByInertia(State.AngularVelocity);
		Demand.Torque += FVector::CrossProduct(State.AngularVelocity, AngularMomentum);

		return Demand;
	}
}
