#include "Ship/SFShipFrameComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "ShipFrameMath.h"
#include "Units.h"

namespace
{
	/** The owner's root when Chaos is simulating it, and nullptr when nothing is. */
	const UPrimitiveComponent* GetSimulatingRoot(const AActor* Owner)
	{
		const UPrimitiveComponent* Root = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
		return (Root && Root->IsSimulatingPhysics()) ? Root : nullptr;
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

	if (SimulatingRoot)
	{
		AngularVelocityRadPerSec = SimulatingRoot->GetPhysicsAngularVelocityInRadians();
	}

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
		// velocity: Chaos at a fixed step integrates v += a dt and then x += v dt, so
		// (x_n - x_{n-1}) / dt is v_n exactly and the difference of two of those is a_n
		// exactly. It also works for an owner that is not a physics body, which the
		// docking station of stage 3 will not be.
		//
		// Differenced on every carrier, simulating or not, unlike the rotation above: the
		// round trip that costs the rotation 0.081 deg at gimbal lock is through an
		// FRotator, and a location passes through it as the doubles it already was.
		const FVector DeltaCm = Current.GetLocation() - LastSampledTransform.GetLocation();
		const FVector VelocityMetersPerSec(
			StarForge::Units::CmToMetres(DeltaCm.X) / DeltaTime,
			StarForge::Units::CmToMetres(DeltaCm.Y) / DeltaTime,
			StarForge::Units::CmToMetres(DeltaCm.Z) / DeltaTime);

		if (bHasVelocitySample)
		{
			LinearAccelMetersPerSecSq = (VelocityMetersPerSec - LinearVelocityMetersPerSec) / DeltaTime;
		}

		LinearVelocityMetersPerSec = VelocityMetersPerSec;
		bHasVelocitySample = true;
	}

	LastSampledTransform = Current;
	bHasSample = true;
}
