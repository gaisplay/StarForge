#include "Ship/SFShipFrameComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Ship/SFTestShip.h"
#include "ShipFrameMath.h"
#include "StarForgeLog.h"
#include "Units.h"

namespace
{
	/** The owner's root when Chaos is simulating it, and nullptr when nothing is. */
	const UPrimitiveComponent* GetSimulatingRoot(const AActor* Owner)
	{
		const UPrimitiveComponent* Root = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
		return (Root && Root->IsSimulatingPhysics()) ? Root : nullptr;
	}

	/**
	 * The negative control for finding F4, declared here because this is the file that reads it.
	 *
	 * 0 divides both differences of the frame's position by the frame, as every run before
	 * 2026-09-27 did, the way sf.DockRelease 0 brings back letting go at the docked pose. It must
	 * bring the F4 knockdowns back -- an uncapped headless sf.Dock 2 cycle trips again under it --
	 * or the time base is not what removed them. Read live every tick.
	 */
	TAutoConsoleVariable<int32> CVarFeltTimeBase(
		TEXT("sf.FeltTimeBase"), 1,
		TEXT("1: the felt acceleration is differenced on the motion's own time base -- a simulated step over the physics step, a kinematic one between the middles of its frames.\n")
		TEXT("0: both differences divided by the frame, as before 2026-09-27 (the negative control for finding F4; over-reads a kinematic carrier under a variable step)."),
		ECVF_Default);

	/**
	 * Written for the one knockdown F4's fix left on a let-go, felt 20.00 where the release peaks
	 * at 17.58, which lay outside the estimator's model. Its per-frame lines found the cause: in
	 * six uncapped releases of twelve the last eased step was declined by the scene component's
	 * 1e-4 cm dead band, and the path lost its last deceleration. Closed on 2026-09-28 by
	 * TeleportDetachedRootExactly and the tick the let-go now waits (SFDockingComponent.cpp).
	 * Kept for the next figure the model does not explain.
	 */
	TAutoConsoleVariable<int32> CVarFeltDebug(
		TEXT("sf.FeltDebug"), 0,
		TEXT("1: every tick of every ship's frame, one line with everything the felt acceleration was estimated from -- frame, clock, flags, root, displacement, velocity and its stamp, spacing, acceleration, and the body's own velocity -- so any step can be recomputed by hand.\n")
		TEXT("0: nothing. Diagnostic only: a line per ship per frame. The station is never logged; it does not move. Read live."),
		ECVF_Default);

	/** What one tick's linear estimate used, held for sf.FeltDebug to print; the estimate reads none of it. */
	struct FFeltDebugStep
	{
		bool bEstimated = false;
		bool bSimulated = false;
		bool bAccelNew = false;
		FVector DisplacementMetres = FVector::ZeroVector;
		double DivisorSeconds = 0.0;
		double SpacingSeconds = 0.0;
	};

	/**
	 * The value last announced in each world, shared by every frame component in it, so a change
	 * is one line per world rather than one per carrier. A world starts from the default, so a
	 * normal run says nothing and a world begun with the control already at 0 says so in its own
	 * log. Kept per world and not once for the process: PIE runs several game worlds in one
	 * process, their frame components tick in turn, and a single slot reset on every change of
	 * world would repeat the 0 warning on every one of those turns -- twice a frame, for good.
	 */
	TMap<TWeakObjectPtr<UWorld>, int32> GAnnouncedFeltTimeBase;

	void AnnounceFeltTimeBase(UWorld* World, int32 Value)
	{
		const TWeakObjectPtr<UWorld> Key(World);
		int32* Announced = GAnnouncedFeltTimeBase.Find(Key);
		if (!Announced)
		{
			// Worlds that have gone are forgotten before a new one is remembered, so the map holds
			// the live ones and no more.
			for (auto It = GAnnouncedFeltTimeBase.CreateIterator(); It; ++It)
			{
				if (!It.Key().IsValid())
				{
					It.RemoveCurrent();
				}
			}
			Announced = &GAnnouncedFeltTimeBase.Add(Key, 1);
		}

		if (Value == *Announced)
		{
			return;
		}
		*Announced = Value;

		if (Value != 0)
		{
			UE_LOG(LogStarForge, Display,
				TEXT("Felt acceleration: sf.FeltTimeBase %d -- differenced on the motion's own time base: ")
				TEXT("a simulated step over the physics step, a kinematic one between the middles of its frames."),
				Value);
		}
		else
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("Felt acceleration: sf.FeltTimeBase 0 -- divided by the frame, as before 2026-09-27. It over-reads ")
				TEXT("a kinematic carrier under a variable step by (1 + dt_prev/dt)/2, so a knockdown on a docking ")
				TEXT("release or capture may be false. The negative control for finding F4."));
		}
	}
}

USFShipFrameComponent::USFShipFrameComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

FTransform USFShipFrameComponent::GetFrameTransform() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorTransform() : FTransform::Identity;
}

FTransform USFShipFrameComponent::WorldToLocal(const FTransform& World) const
{
	return StarForge::ShipFrame::WorldToLocal(GetFrameTransform(), World);
}

FTransform USFShipFrameComponent::LocalToWorld(const FTransform& Local) const
{
	return StarForge::ShipFrame::LocalToWorld(GetFrameTransform(), Local);
}

FTransform USFShipFrameComponent::CarryOccupant(const FTransform& PrevFrame, const FTransform& OccupantWorld) const
{
	return StarForge::ShipFrame::CarryOccupant(PrevFrame, GetFrameTransform(), OccupantWorld);
}

FVector USFShipFrameComponent::GetAngularVelocityRadPerSec() const
{
	return AngularVelocityRadPerSec;
}

void USFShipFrameComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const FTransform Current = GetFrameTransform();

	// A simulating hull is asked for its own angular velocity, and only a carrier that
	// has no body has its rotation differenced.
	//
	// Differencing a transform divides that transform's error by the frame time, and the
	// transform an actor hands back is not its body's: every physics sync round-trips it
	// through an FRotator, quaternion to rotator and back
	// (SceneComponent.cpp 3314-3341, whose own comment insists the quaternion must come
	// from the rotator), and FQuat::Rotator() answers exactly +/-90 of pitch for anything
	// within 0.081 deg of gimbal lock (UnrealMath.cpp 654-669). That is a fixed error in
	// position, so dividing it by a frame makes it a rate that grows with the frame rate:
	// 4.9 deg/s at 60 fps and 32.4 at 400, which is the shape of this project's worst
	// defects and the reason no threshold in seconds can bound it.
	//
	// Measured on the spike rig under full pitch, one frame in every half turn: the
	// instrument read 152.1 deg/s where the hull turned 120.0, three metres of radius put
	// the felt acceleration over the 19.62 grav-plate ceiling, and the occupant was
	// knocked down eleven times at 400 fps and not once at 60.
	const UPrimitiveComponent* SimulatingRoot = GetSimulatingRoot(GetOwner());

	// Which of the two reads is running says nothing in any other line, and a frame changes
	// over mid-run: a hull leaves simulation to dock and rejoins it to undock. Said on the
	// first tick and on every change, so any rate in a log can be traced to the read that
	// produced it -- the question that cost a whole round when the differenced rate was
	// blamed on the thruster spool.
	const bool bFromBody = (SimulatingRoot != nullptr);
	if (!bHasSample || bFromBody != bReadsFromBody)
	{
		const AActor* Owner = GetOwner();
		UE_LOG(LogStarForge, Display, TEXT("Frame [%s]: angular velocity %s."),
			Owner ? *Owner->GetActorNameOrLabel() : TEXT("none"),
			bFromBody
				? TEXT("read from the simulating body")
				: TEXT("differenced from the transform -- no simulating body"));
	}
	bReadsFromBody = bFromBody;

	if (SimulatingRoot)
	{
		AngularVelocityRadPerSec = SimulatingRoot->GetPhysicsAngularVelocityInRadians();
	}

	const int32 FeltTimeBase = CVarFeltTimeBase.GetValueOnGameThread();
	AnnounceFeltTimeBase(GetWorld(), FeltTimeBase);
	const bool bOnTimeBase = (FeltTimeBase != 0);

	// The linear half below assumes physics steps once, by min(dt, MaxPhysicsDeltaTime), inside
	// the frame's own dt. Async physics steps on its own clock and hands the game thread an
	// interpolated pose; substepping takes several steps a frame. Either way dx is no longer one
	// semi-implicit step and nothing below can recover the acceleration from it. Said only on a
	// simulating carrier: a kinematic pose is written on the game thread against the game clock,
	// and neither setting touches it. Both are off in this project, so a normal run says nothing.
	if (bFromBody && !bWarnedPhysicsStepping)
	{
		const UPhysicsSettings* PhysicsConfig = UPhysicsSettings::Get();
		if (PhysicsConfig->bTickPhysicsAsync || PhysicsConfig->bSubstepping)
		{
			bWarnedPhysicsStepping = true;
			const AActor* Owner = GetOwner();
			UE_LOG(LogStarForge, Warning,
				TEXT("Frame [%s]: physics runs with %s, so a simulated step is not one physics step of the frame's own dt. ")
				TEXT("The felt acceleration differenced here is not trustworthy on this carrier, and neither is a knockdown decided on it."),
				Owner ? *Owner->GetActorNameOrLabel() : TEXT("none"),
				(PhysicsConfig->bTickPhysicsAsync && PhysicsConfig->bSubstepping) ? TEXT("bTickPhysicsAsync and bSubstepping")
					: PhysicsConfig->bTickPhysicsAsync ? TEXT("bTickPhysicsAsync")
					: TEXT("bSubstepping"));
		}
	}

	FFeltDebugStep FeltDebug;

	if (bHasSample && DeltaTime > UE_SMALL_NUMBER)
	{
		if (!SimulatingRoot)
		{
			FQuat Delta = Current.GetRotation() * LastSampledTransform.GetRotation().Inverse();

			// ToAxisAndAngle reports an angle in [0, 2pi], so a tick that turned the ship
			// by a hair short of a full revolution comes back as that near-full angle
			// about the opposite axis instead of the small one it really was. A quaternion
			// and its negation name the same rotation, so negating when W is negative
			// picks the shortest arc and pins the angle into [0, pi].
			if (Delta.W < 0.0)
			{
				Delta = -Delta;
			}

			FVector Axis;
			double Angle;
			Delta.ToAxisAndAngle(Axis, Angle);

			AngularVelocityRadPerSec = Axis * (Angle / DeltaTime);
		}

		// Metres from here on: the one crossing of the unit boundary for this
		// instrument. A second difference of position rather than a read of the body's
		// velocity, for two reasons. A body teleported while kinematic reports a velocity
		// of zero, so the docking capture and release would vanish from the instrument;
		// and GetPhysicsLinearVelocity is the centre of mass's, where the handover maths
		// composes w against the frame origin's.
		//
		// Differenced on every carrier, simulating or not, unlike the rotation above: the
		// round trip that costs the rotation 0.081 deg at gimbal lock is through an
		// FRotator, and a location passes through it as the doubles it already was.
		//
		// On the time base of whatever moved the carrier -- finding F4's fix, 2026-09-27.
		// Chaos integrates v += a h and then x += v h, so dx / h is v_n exactly and the
		// difference of two of those is a_n exactly -- at ANY step, fixed or variable, so
		// long as physics steps once in the frame, by the frame's own dt. Two things break
		// that, and divided by the frame each has knocked a man down:
		//  - a kinematic carrier, whose pose is written against accumulated time: the
		//    docking release and capture. dx / dt is then the frame's MEAN velocity, which
		//    belongs to the frame's middle, and the difference of two means divided by dt_n
		//    instead of the (dt_n + dt_{n-1}) / 2 between their middles over-reads by
		//    (1 + dt_{n-1}/dt_n) / 2. At about 420 fps by hand the owner was knocked down
		//    four times in one sf.Dock 2 cycle, at 20.2-23.7 m/s2, by a release that peaks
		//    at 17.6; headless it tripped in 12 of 12 runs on a variable step and 0 of 14
		//    on a fixed one, and every headless recipe here runs on a fixed one.
		//  - a frame longer than MaxPhysicsDeltaTime, 1/30 s by default: physics advances
		//    only h = min(dt, max) (ChaosScene.cpp 344), dx / dt under-reads v by h / dt,
		//    and the next normal frame reads the recovery as a spike -- at 200 m/s a 40 ms
		//    frame among 400 fps ones reads -833 and then +13 333 m/s2. Predicted first,
		//    then measured with sf.Hitch, which makes the long frame: under sf.FeltTimeBase 0
		//    one 50 ms stall at 200 m/s cruise, uncapped, read felt 1359.85 m/s2 and knocked
		//    a standing man down (1355.72 on the repeat, 530.14 at about 80 m/s); on this
		//    time base the same run reads footing intact
		//    (evidence/2026-09-27-f4fix-*hitch50-*-uncapped*.log).
		// So a simulated step is divided by h and stamped at its end, any other by the
		// frame and stamped at its middle, and the second difference is taken between the
		// stamps. A step counts as simulated only if the root simulated at both samples: the
		// capture's first step began simulating and its travel is an eased step, and a move
		// written into the tick that switches simulation on is a teleport. The release let go
		// that way until 2026-09-28. Since then it writes its last eased pose exactly
		// (TeleportDetachedRootExactly) and lets go on the next tick (bReleaseArrived in
		// USFDockingComponent::AdvanceRelease), which moves nothing, so the step that ends
		// simulating carries only what Chaos integrated from rest in it -- nothing at all in
		// sf.FeltDebug's record, unless the pilot's demand lands in the same tick (FFrameStep
		// in ShipFrameMath.h says what that reads) -- and the rule decides only where its
		// zero is stamped. At a fixed step no longer than the physics maximum every figure is
		// bit-identical to dividing by the frame except on the frame or two either side of
		// such a switch; ShipFrameMath.cpp says why.
		//
		// One of those frames reads MORE than it did, and it is the capture's first. The
		// run-in's smoothstep starts from rest whatever the hull was doing, so a capture
		// begun at speed steps the velocity inside that frame, and the spacing from the last
		// simulated end to the first kinematic middle is half the frame: the step reads at
		// twice what dividing by the frame read. From 2.93 m out, the approach that knocks a
		// standing man down falls from 0.47 to 0.31 m/s at 60 fps and from 0.068 to 0.044 at
		// 420. The step is real -- the capture does stop the hull within a frame -- and any
		// reading of it grows with the frame rate; the cure is a run-in that starts from the
		// hull's own velocity, which leaves nothing to read, not another spacing here.
		const FVector DeltaCm = Current.GetLocation() - LastSampledTransform.GetLocation();

		StarForge::ShipFrame::FFrameStep Step;
		Step.DisplacementMetres = FVector(
			StarForge::Units::CmToMetres(DeltaCm.X),
			StarForge::Units::CmToMetres(DeltaCm.Y),
			StarForge::Units::CmToMetres(DeltaCm.Z));
		Step.FrameSeconds = DeltaTime;

		// sf.FeltTimeBase 0 is the estimator as it was, and that is this one under the
		// assumption it was written on: every step one unclamped Chaos step of the frame.
		Step.bSimulatingAtStart = bOnTimeBase ? bLastSampleSimulating : true;
		Step.bSimulatingAtEnd = bOnTimeBase ? bFromBody : true;
		const double MaxPhysicsStepSeconds = bOnTimeBase ? UPhysicsSettings::Get()->MaxPhysicsDeltaTime : 0.0;

		StarForge::ShipFrame::FFrameVelocity Estimate;
		if (StarForge::ShipFrame::EstimateStepVelocity(Step, MaxPhysicsStepSeconds, Estimate))
		{
			// The divisor here and the spacing below repeat the two functions' own rules for the log
			// alone, because neither hands them back; keep them in step with ShipFrameMath.cpp.
			FeltDebug.bEstimated = true;
			FeltDebug.DisplacementMetres = Step.DisplacementMetres;
			FeltDebug.bSimulated = Step.bSimulatingAtStart && Step.bSimulatingAtEnd;
			FeltDebug.DivisorSeconds = (FeltDebug.bSimulated && MaxPhysicsStepSeconds > 0.0)
				? FMath::Min(Step.FrameSeconds, MaxPhysicsStepSeconds)
				: Step.FrameSeconds;

			// Never across a change of sf.FeltTimeBase: the previous stamp was set by the
			// other rule, and switching the control on in the middle of a release would read
			// a kinematic carrier at twice its acceleration for a frame -- over the limit at
			// the release's own peak. The figure already held stands for that frame.
			if (bHasVelocitySample && bOnTimeBase == bLastVelocityOnTimeBase)
			{
				StarForge::ShipFrame::FFrameVelocity Previous;
				Previous.VelocityMetresPerSec = LinearVelocityMetersPerSec;
				Previous.StampOffsetSeconds = LastVelocityStampOffsetSeconds;

				FeltDebug.SpacingSeconds =
					(static_cast<double>(DeltaTime) + Estimate.StampOffsetSeconds) - Previous.StampOffsetSeconds;

				FVector AccelMetersPerSecSq = FVector::ZeroVector;
				if (StarForge::ShipFrame::EstimateStepAcceleration(Previous, Estimate, DeltaTime, AccelMetersPerSecSq))
				{
					LinearAccelMetersPerSecSq = AccelMetersPerSecSq;
					FeltDebug.bAccelNew = true;
				}
			}

			LinearVelocityMetersPerSec = Estimate.VelocityMetresPerSec;
			LastVelocityStampOffsetSeconds = Estimate.StampOffsetSeconds;
			bLastVelocityOnTimeBase = bOnTimeBase;
			bHasVelocitySample = true;
		}
	}

	// Before the sample is replaced, so the start flag printed is the one this step was judged by.
	const AActor* FeltDebugOwner = GetOwner();
	if (CVarFeltDebug.GetValueOnGameThread() != 0 && FeltDebugOwner && FeltDebugOwner->IsA<ASFTestShip>())
	{
		const FVector RootCm = Current.GetLocation();
		const FVector DisplacementMicrometres = FeltDebug.DisplacementMetres * 1.0e6;

		FString BodyVelocity = TEXT("not simulating");
		if (SimulatingRoot)
		{
			const FVector BodyCmPerSec = SimulatingRoot->GetPhysicsLinearVelocity();
			BodyVelocity = FString::Printf(TEXT("(%.9f, %.9f, %.9f) m/s"),
				StarForge::Units::CmToMetres(BodyCmPerSec.X),
				StarForge::Units::CmToMetres(BodyCmPerSec.Y),
				StarForge::Units::CmToMetres(BodyCmPerSec.Z));
		}

		UE_LOG(LogStarForge, Display,
			TEXT("Felt debug [%s]: frame %llu, t %.6f s, dt %.6f ms, divisor %.6f ms, simulating %d -> %d, step %s, ")
			TEXT("root (%.9f, %.9f, %.9f) cm, dx (%.6f, %.6f, %.6f) um, v (%.9f, %.9f, %.9f) m/s, stamp %.6f ms, ")
			TEXT("spacing %.6f ms, a (%.6f, %.6f, %.6f) |a| %.6f m/s2 %s, body v %s, sf.FeltTimeBase %d"),
			*FeltDebugOwner->GetActorNameOrLabel(),
			GFrameCounter,
			GetWorld()->GetTimeSeconds(),
			static_cast<double>(DeltaTime) * 1000.0,
			FeltDebug.DivisorSeconds * 1000.0,
			bLastSampleSimulating ? 1 : 0,
			bFromBody ? 1 : 0,
			!FeltDebug.bEstimated ? TEXT("none") : FeltDebug.bSimulated ? TEXT("simulated") : TEXT("kinematic"),
			RootCm.X, RootCm.Y, RootCm.Z,
			DisplacementMicrometres.X, DisplacementMicrometres.Y, DisplacementMicrometres.Z,
			LinearVelocityMetersPerSec.X, LinearVelocityMetersPerSec.Y, LinearVelocityMetersPerSec.Z,
			LastVelocityStampOffsetSeconds * 1000.0,
			FeltDebug.SpacingSeconds * 1000.0,
			LinearAccelMetersPerSecSq.X, LinearAccelMetersPerSecSq.Y, LinearAccelMetersPerSecSq.Z,
			LinearAccelMetersPerSecSq.Size(),
			FeltDebug.bAccelNew ? TEXT("new") : TEXT("held"),
			*BodyVelocity,
			FeltTimeBase);
	}

	LastSampledTransform = Current;
	bLastSampleSimulating = bFromBody;
	bHasSample = true;
}
