#include "Ship/SFPilotStationComponent.h"

#include "GameFramework/Actor.h"
#include "Ship/SFShipDataAsset.h"
#include "Ship/SFThrusterArrayComponent.h"
#include "StarForgeLog.h"

USFPilotStationComponent::USFPilotStationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Before physics, so the forces this posts asks for land in the same step that
	// integrates them rather than the next one.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	StationName = TEXT("pilot");
}

void USFPilotStationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Thrusters = Owner->FindComponentByClass<USFThrusterArrayComponent>();
	}

	if (!Thrusters)
	{
		// Loudly: a pilot station with nothing to command looks exactly like a ship
		// that will not respond, and the difference matters at three in the morning.
		UE_LOG(LogStarForge, Error,
			TEXT("Pilot station found no thruster array on its owner; the ship cannot be flown."));
	}
}

void USFPilotStationComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Thrusters || !Thrusters->IsReady() || !Thrusters->ShipData || !IsOccupied())
	{
		return;
	}

	// An operator who is not flying says so rather than handing back a zeroed intent:
	// zero means "stick centred", which with assist on is a command to stop.
	StarForge::FFlightIntent Intent;
	if (!Operator->ProvideFlightIntent(Intent))
	{
		return;
	}

	LastIntent = Intent;

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		Intent,
		Thrusters->ReadState(),
		Thrusters->GetMassProperties(),
		Thrusters->ShipData->BuildLimits(),
		Thrusters->GetArrayCapability());

	Thrusters->ApplyDemand(Demand, DeltaTime);
}
