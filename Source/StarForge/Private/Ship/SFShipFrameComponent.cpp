#include "Ship/SFShipFrameComponent.h"

#include "GameFramework/Actor.h"
#include "ShipFrameMath.h"
#include "Units.h"

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

	if (bHasSample && DeltaTime > UE_SMALL_NUMBER)
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

		// Metres from here on: the one crossing of the unit boundary for this
		// instrument. A second difference of position rather than a read of the body's
		// velocity: Chaos at a fixed step integrates v += a dt and then x += v dt, so
		// (x_n - x_{n-1}) / dt is v_n exactly and the difference of two of those is a_n
		// exactly. It also works for an owner that is not a physics body, which the
		// docking station of stage 3 will not be.
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
