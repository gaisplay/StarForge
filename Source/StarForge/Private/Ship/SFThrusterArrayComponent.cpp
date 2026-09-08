#include "Ship/SFThrusterArrayComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Ship/SFShipDataAsset.h"
#include "StarForgeLog.h"
#include "Units.h"

USFThrusterArrayComponent::USFThrusterArrayComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USFThrusterArrayComponent::BeginPlay()
{
	Super::BeginPlay();
	Rebuild();
}

UPrimitiveComponent* USFThrusterArrayComponent::ResolveBody() const
{
	AActor* Owner = GetOwner();
	return Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
}

bool USFThrusterArrayComponent::Rebuild()
{
	bReady = false;

	if (!ShipData)
	{
		UE_LOG(LogStarForge, Error, TEXT("Thruster array has no ship data asset; the ship will not fly."));
		return false;
	}

	UPrimitiveComponent* Body = ResolveBody();
	if (!Body || !Body->GetBodyInstance())
	{
		UE_LOG(LogStarForge, Error, TEXT("Thruster array found no physics body on the root component."));
		return false;
	}

	FBodyInstance* Instance = Body->GetBodyInstance();

	// Spec 9: the flight model is balanced by editing the asset, not by rebuilding
	// C++, so the asset's mass has to reach the body. Set before the read below and
	// not after: Chaos rescales the inertia tensor when mass changes, so reading
	// first would capture the values belonging to the old mass. Set through the
	// component rather than the body instance: only this path recomputes the mass
	// properties, and it is those recomputed values that are read back below.
	Body->SetMassOverrideInKg(NAME_None, static_cast<float>(ShipData->MassKg), true);

	// Chaos is authoritative here. The asset says what the ship weighs; Chaos works
	// out how that mass is distributed, and it is Chaos that integrates the motion.
	const FTransform MassSpace = Instance->GetMassSpaceLocal();

	MassProperties.Mass = Instance->GetBodyMass();
	MassProperties.CentreOfMass = MassSpace.GetLocation() / StarForge::Units::CmPerMetre;
	MassProperties.InertiaDiagonal = StarForge::Units::InertiaUuToSi(Instance->GetBodyInertiaTensor());
	MassProperties.InertiaRotation = MassSpace.GetRotation();

	const TArray<StarForge::FThrusterDef> Thrusters = ShipData->BuildThrusters();

	if (!Allocator.Build(Thrusters, MassProperties.CentreOfMass))
	{
		// Six axes needs opposed pairs: with throttles confined to [0, 1] an array
		// pushing only along the positive axes cannot torque without also shoving.
		UE_LOG(LogStarForge, Error,
			TEXT("Thruster layout of %d thrusters cannot span all six axes; the ship will not fly. ")
			TEXT("Check that every axis has an opposed pair."),
			Thrusters.Num());
		return false;
	}

	// Achievable, not the capability sum. The stick is scaled by this figure, and
	// the sum counts opposed thrusters that can never push the way being asked --
	// about twice the truth on a balanced array, which saturates the stick at half
	// travel and leaves its whole upper half doing nothing.
	for (int32 Axis = 0; Axis < 6; ++Axis)
	{
		const double Value = Allocator.GetAxisAchievable(Axis);
		if (Axis < 3)
		{
			Capability.Force[Axis] = Value;
		}
		else
		{
			Capability.Torque[Axis - 3] = Value;
		}
	}

	LastThrottles.Reset();
	LastThrottles.SetNumZeroed(Thrusters.Num());
	TargetThrottles.Reset();
	TargetThrottles.SetNumZeroed(Thrusters.Num());

	ResponseTimes.Reset();
	ResponseTimes.Reserve(Thrusters.Num());
	for (const StarForge::FThrusterDef& Thruster : Thrusters)
	{
		ResponseTimes.Add(Thruster.ResponseTime);
	}

	bReady = true;

	UE_LOG(LogStarForge, Display,
		TEXT("Thruster array ready: %d thrusters, %.0f kg, inertia (%.0f, %.0f, %.0f) kg m^2."),
		Thrusters.Num(), MassProperties.Mass,
		MassProperties.InertiaDiagonal.X, MassProperties.InertiaDiagonal.Y,
		MassProperties.InertiaDiagonal.Z);

	return true;
}

StarForge::FRigidBodyState USFThrusterArrayComponent::ReadState() const
{
	StarForge::FRigidBodyState State;

	const UPrimitiveComponent* Body = ResolveBody();
	if (!Body)
	{
		return State;
	}

	const FTransform ShipToWorld = Body->GetComponentTransform();

	// Velocities come back in world space and centimetres. The control loop thinks
	// in the ship own axes and in metres, because that is where the thrusters are.
	State.LinearVelocity =
		ShipToWorld.InverseTransformVectorNoScale(Body->GetPhysicsLinearVelocity())
		/ StarForge::Units::CmPerMetre;

	// Radians are unitless, so this one only changes frame.
	State.AngularVelocity =
		ShipToWorld.InverseTransformVectorNoScale(Body->GetPhysicsAngularVelocityInRadians());

	return State;
}

void USFThrusterArrayComponent::ClearDemand()
{
	LastDemand = StarForge::FWrench();
	LastAchieved = StarForge::FWrench();

	for (double& Throttle : TargetThrottles)
	{
		Throttle = 0.0;
	}

	// The spooled throttles are deliberately left alone: they describe where the
	// thrusters physically got to, and they wind down through the lag in ApplyDemand
	// on their own. Zeroing them here would claim they had already closed.
}

void USFThrusterArrayComponent::ApplyDemand(const StarForge::FWrench& DemandShipFrame, double DeltaSeconds)
{
	if (!bReady)
	{
		return;
	}

	UPrimitiveComponent* Body = ResolveBody();
	if (!Body)
	{
		return;
	}

	LastDemand = DemandShipFrame;
	Allocator.Solve(DemandShipFrame, TargetThrottles);

	// Spec 5: a thruster spools, it does not snap. Without this the ship changes
	// direction on the exact frame the stick moves, which reads as weightless no
	// matter how much mass the maths was given.
	for (int32 Index = 0; Index < LastThrottles.Num(); ++Index)
	{
		const double Tau = ResponseTimes.IsValidIndex(Index) ? ResponseTimes[Index] : 0.0;

		if (Tau <= UE_DOUBLE_SMALL_NUMBER || DeltaSeconds <= 0.0)
		{
			LastThrottles[Index] = TargetThrottles[Index];
			continue;
		}

		// First-order lag: one ResponseTime covers about 63 per cent of a step. The
		// clamp matters at low frame rates, where DeltaSeconds can exceed the time
		// constant and an unclamped factor would overshoot and oscillate.
		const double Alpha = FMath::Clamp(DeltaSeconds / Tau, 0.0, 1.0);
		LastThrottles[Index] += (TargetThrottles[Index] - LastThrottles[Index]) * Alpha;
	}

	LastAchieved = Allocator.ComputeAchieved(LastThrottles);

	// What the thrusters actually manage, not what was asked for. Applying the
	// demand instead would give the ship thrust it does not have.
	const StarForge::FWrench Uu = StarForge::Units::WrenchToUu(LastAchieved);

	const FTransform ShipToWorld = Body->GetComponentTransform();

	// AddForce with no bone applies at the centre of mass, so it adds no torque of
	// its own; the torque below is the whole of it.
	Body->AddForce(ShipToWorld.TransformVectorNoScale(Uu.Force));
	Body->AddTorqueInRadians(ShipToWorld.TransformVectorNoScale(Uu.Torque));
}
