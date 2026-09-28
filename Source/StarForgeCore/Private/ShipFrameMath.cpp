#include "ShipFrameMath.h"

namespace StarForge::ShipFrame
{
	namespace
	{
		/**
		 * Every component finite. An infinity is not a NaN, and here it would come back as a
		 * relative speed of infinity -- which the caller's guard would refuse, correctly, but
		 * only by accident of the comparison rather than by this function's contract.
		 */
		bool IsFinite(const FVector& V)
		{
			return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
		}

		bool IsFinite(const FCarrierMotion& Motion)
		{
			return IsFinite(Motion.OriginMetres)
				&& IsFinite(Motion.LinearVelocityMetresPerSec)
				&& IsFinite(Motion.AngularVelocityRadPerSec);
		}

		/** v + w x r, the velocity of the one point of a carrier the caller asked about. */
		FVector PointVelocityMetresPerSec(const FCarrierMotion& Motion, const FVector& PointMetres)
		{
			return Motion.LinearVelocityMetresPerSec
				+ FVector::CrossProduct(Motion.AngularVelocityRadPerSec, PointMetres - Motion.OriginMetres);
		}
	}

	FTransform WorldToLocal(const FTransform& ShipWorld, const FTransform& OccupantWorld)
	{
		return OccupantWorld.GetRelativeTransform(ShipWorld);
	}

	FTransform LocalToWorld(const FTransform& ShipWorld, const FTransform& OccupantLocal)
	{
		return OccupantLocal * ShipWorld;
	}

	FTransform CarryOccupant(
		const FTransform& PrevShipWorld,
		const FTransform& CurrShipWorld,
		const FTransform& OccupantWorld)
	{
		return LocalToWorld(CurrShipWorld, WorldToLocal(PrevShipWorld, OccupantWorld));
	}

	FVector ComputeFeltAcceleration(
		const FVector& ShipLinearAccel,
		const FVector& ShipAngularVelocityRadPerSec,
		const FVector& OccupantLocalOffset)
	{
		const FVector& Omega = ShipAngularVelocityRadPerSec;
		const FVector Centripetal =
			FVector::CrossProduct(Omega, FVector::CrossProduct(Omega, OccupantLocalOffset));

		return ShipLinearAccel + Centripetal;
	}

	bool ExceedsGravPlateLimit(const FVector& FeltAccelMetersPerSecSq)
	{
		return FeltAccelMetersPerSecSq.Size() > GravPlateLimitMetersPerSecSq;
	}

	FVector ComputeUncompensatedAcceleration(const FVector& FeltAccelMetersPerSecSq)
	{
		const FVector Absorbed = FeltAccelMetersPerSecSq.GetClampedToMaxSize(GravPlateLimitMetersPerSecSq);
		return -(FeltAccelMetersPerSecSq - Absorbed);
	}

	bool FFootingState::Update(bool bOverLimit, double DeltaSeconds, double RecoverySeconds)
	{
		if (bOverLimit)
		{
			bDown = true;
			SecondsUnderLimit = 0.0;
			return bDown;
		}

		if (bDown)
		{
			SecondsUnderLimit += FMath::Max(0.0, DeltaSeconds);
			if (SecondsUnderLimit >= RecoverySeconds)
			{
				bDown = false;
				SecondsUnderLimit = 0.0;
			}
		}

		return bDown;
	}
}

namespace StarForge::ShipFrame
{
	FQuat ApplyLookStep(
		const FQuat& CurrentView,
		const FVector& DeckUp,
		double YawDegrees,
		double PitchDegrees,
		double MaxPitchDegrees)
	{
		const FVector Up = DeckUp.GetSafeNormal();
		if (Up.IsNearlyZero())
		{
			// No deck to stand on means no axes to turn about; leaving the view alone
			// is the only honest answer.
			return CurrentView;
		}

		// Yaw about the deck normal, in world space, so turning on the spot follows the
		// floor rather than the world's vertical.
		FQuat View = FQuat(Up, FMath::DegreesToRadians(YawDegrees)) * CurrentView;

		// Pitch about the view's own right axis, recomputed after the yaw so the two
		// halves of one step do not fight each other. It is projected back into the
		// deck plane first: that is what stops a pitch step from leaking roll, and it
		// re-squares the axis every frame instead of letting error accumulate.
		FVector Right = FVector::VectorPlaneProject(View.GetRightVector(), Up).GetSafeNormal();
		if (Right.IsNearlyZero())
		{
			Right = View.GetRightVector();
		}

		// How far above the deck plane the view already points, and how far it may go.
		// Clamping the destination rather than the step means holding the key parks the
		// view at the zenith instead of walking it over the top.
		const double CurrentPitch = FMath::RadiansToDegrees(
			FMath::Asin(FMath::Clamp(FVector::DotProduct(View.GetForwardVector(), Up), -1.0, 1.0)));

		const double AllowedPitch =
			FMath::Clamp(CurrentPitch + PitchDegrees, -MaxPitchDegrees, MaxPitchDegrees) - CurrentPitch;

		// Negative because a positive rotation about the right axis pitches the nose
		// down, while a positive PitchDegrees means looking up.
		View = FQuat(Right, -FMath::DegreesToRadians(AllowedPitch)) * View;

		return View.GetNormalized();
	}

	bool RelativeSpeedAtPointMetresPerSec(
		const FCarrierMotion& A,
		const FCarrierMotion& B,
		const FVector& PointMetres,
		double& OutSpeedMetresPerSec)
	{
		// Negated, so a NaN fails the guard instead of falling through it, and Out is left
		// untouched on every refusal.
		if (!IsFinite(A)) { return false; }
		if (!IsFinite(B)) { return false; }
		if (!IsFinite(PointMetres)) { return false; }

		OutSpeedMetresPerSec =
			(PointVelocityMetresPerSec(A, PointMetres) - PointVelocityMetresPerSec(B, PointMetres)).Size();

		return true;
	}

	// FIXED-STEP IDENTITY. At a fixed step no longer than the physics maximum, a run of steps of one
	// kind reads bit for bit what dividing both differences by the frame read, so every rig line
	// recorded before 2026-09-27 stands, except on the frame or two either side of a switch between
	// simulated and kinematic. Three things make it exact rather than close:
	//  - FMath::Min(dt, max) hands back dt itself whenever dt <= max;
	//  - the kinematic spacing is (dt - dt/2) + dt/2, and each operation in it is exact: halving a
	//    double, dt - dt/2 (Sterbenz), and dt/2 + dt/2, which is a doubling;
	//  - both divisions are written the way the frame component always wrote them -- the velocity
	//    component by component, the acceleration through FVector's operator/, which multiplies by
	//    the reciprocal (Vector.h 324-328) and so differs from a component-wise division in the last
	//    bit. Swapping either form for the other moves every figure.
	// It holds under /fp:precise, which the editor target compiles with (VCToolChain.cs 1322-1346,
	// BuildSettingsVersion.V7), and every rig runs in the editor binary. /fp:fast, the default for a
	// Game target, licenses the compiler to reassociate the spacing.

	bool EstimateStepVelocity(const FFrameStep& Step, double MaxPhysicsStepSeconds, FFrameVelocity& Out)
	{
		// Negated, so a NaN fails the guard instead of falling through it.
		if (!(Step.FrameSeconds > 0.0))
		{
			return false;
		}

		// Both ends, and each half of that is a switch the docking component makes. Simulating at
		// the end alone is the release's let-go, whose travel is a snap. Simulating at the start
		// alone is the capture's first frame, whose travel is the run-in's first eased step: judged
		// by its start it would be stamped at its end, the next kinematic middle half a frame
		// later, and a capture from rest would read about twice its own 6d/T^2 -- 33.99 m/s2 at
		// 60 fps against a 17.58 peak for the tests' 2.93 m (the rig's standoff is 2.928 m, peak
		// 17.57), on every capture.
		//
		// Judged correctly, that first frame is, at a fixed step, the one frame spaced by less than
		// the frame -- half of it -- and so the one that reads more than dividing by the frame did.
		// The run-in starts from rest whatever the hull was doing, so a capture begun at speed steps
		// the velocity at the frame's start, and the half-frame spacing reads the step at twice the
		// old figure. That is the step, not the
		// spacing: an impulse has no finite reading at any spacing, and only a run-in that starts
		// from the hull's own velocity removes it.
		const bool bSimulated = Step.bSimulatingAtStart && Step.bSimulatingAtEnd;

		const double DivisorSeconds = (bSimulated && MaxPhysicsStepSeconds > 0.0)
			? FMath::Min(Step.FrameSeconds, MaxPhysicsStepSeconds)
			: Step.FrameSeconds;

		Out.VelocityMetresPerSec = FVector(
			Step.DisplacementMetres.X / DivisorSeconds,
			Step.DisplacementMetres.Y / DivisorSeconds,
			Step.DisplacementMetres.Z / DivisorSeconds);
		Out.StampOffsetSeconds = bSimulated ? 0.0 : -0.5 * Step.FrameSeconds;

		return true;
	}

	bool EstimateStepAcceleration(
		const FFrameVelocity& Previous,
		const FFrameVelocity& Current,
		double CurrentFrameSeconds,
		FVector& OutAccelMetresPerSecSq)
	{
		// Left to right, as the identity above needs.
		const double SpacingSeconds =
			(CurrentFrameSeconds + Current.StampOffsetSeconds) - Previous.StampOffsetSeconds;

		// Two estimates made by EstimateStepVelocity over positive frames are never closer than half
		// the later frame -- a kinematic step after a simulated one -- so a spacing of zero or less
		// means the two are out of order or were stamped by different rules. Divided by, it would put
		// an infinity, or an acceleration of the wrong sign, straight into the knockdown. The caller
		// keeps the figure it had, as it does for a frame of no length.
		if (!(SpacingSeconds > 0.0))
		{
			return false;
		}

		// On a simulated step clamped by the physics maximum the velocity changed by a h, not a dt,
		// so this reads a h / dt there: an under-read, which can miss a knockdown that frame deserved
		// and cannot invent one. The spacing stays dt_n by the fix's definition -- both stamps are the
		// ends of their frames on the game clock.
		OutAccelMetresPerSecSq = (Current.VelocityMetresPerSec - Previous.VelocityMetresPerSec) / SpacingSeconds;

		return true;
	}
}
