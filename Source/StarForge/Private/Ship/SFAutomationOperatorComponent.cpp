#include "Ship/SFAutomationOperatorComponent.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFGunnerStationComponent.h"
#include "Ship/SFTargetDrone.h"

namespace
{
	/**
	 * Whether the automation takes the gunner seat at startup and keeps it. On by
	 * default: a mechanic behind an off-by-default switch is a mechanic nobody has.
	 * Turned off so a person can take the gunner seat instead, the way sf.ConsoleOperator
	 * frees the pilot's.
	 */
	TAutoConsoleVariable<int32> CVarAutomationOperator(TEXT("sf.AutomationOperator"), 1,
		TEXT("1: the ship's automation takes the gunner seat at startup and holds it. 0: it leaves the seat free."),
		ECVF_Default);
}

USFAutomationOperatorComponent::USFAutomationOperatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Before physics. Being in the same group as the gunner station orders nothing
	// between the two, so BeginPlay adds the station's tick prerequisite explicitly --
	// that, not this line, is what puts this tick first, which is where a seat change
	// has to be visible in the same frame it happens rather than the next.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USFAutomationOperatorComponent::SetTargetDrone(ASFTargetDrone* InDrone)
{
	TargetDrone = InDrone;
}

void USFAutomationOperatorComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	USFGunnerStationComponent* Gunner =
		Owner ? Owner->FindComponentByClass<USFGunnerStationComponent>() : nullptr;

	if (!Gunner)
	{
		return;
	}

	// Makes the ordering the constructor's comment claims actually true, for the same
	// reason the console operator does it to the pilot station.
	Gunner->AddTickPrerequisiteComponent(this);

	bWantedSeatLastTick = CVarAutomationOperator.GetValueOnGameThread() != 0;
	if (bWantedSeatLastTick)
	{
		Gunner->TryOccupy(this);
	}
}

void USFAutomationOperatorComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	USFGunnerStationComponent* Gunner =
		Owner ? Owner->FindComponentByClass<USFGunnerStationComponent>() : nullptr;
	if (!Gunner)
	{
		return;
	}

	// A runtime switch, not a startup default: -ExecCmds runs after BeginPlay, and a
	// person at the console has to be able to turn this off to take the seat.
	const bool bWantsSeat = CVarAutomationOperator.GetValueOnGameThread() != 0;
	const UObject* Holder = Gunner->GetOperator().GetObject();
	const bool bHoldsSeat = Holder == this;

	if (bWantsSeat && !bHoldsSeat)
	{
		// A free post is asked for every frame until it is held -- the console
		// operator's lesson, learned when a rate limiter sat out an empty seat for the
		// rest of a session. A post somebody else holds is asked for once per turn of
		// the switch, so the refusal in TryOccupy is heard once rather than sixty times
		// a second, and heard at all rather than pre-empted by an IsOccupied check.
		if (!Holder || !bWantedSeatLastTick)
		{
			Gunner->TryOccupy(this);
		}
	}
	else if (!bWantsSeat && bHoldsSeat)
	{
		Gunner->Vacate();
	}

	bWantedSeatLastTick = bWantsSeat;
}

bool USFAutomationOperatorComponent::ProvideTurretIntent(StarForge::FTurretIntent& Out)
{
	// Gunning whenever asked: this operator has no "not at the post" state of its own.
	// What it may lack is a target, and that is said in the intent rather than by
	// declining, because declining reads as an empty seat and an empty seat is a
	// different fact.
	const ASFTargetDrone* Drone = TargetDrone.Get();
	Out.bHasTarget = Drone != nullptr && Drone->IsPresent();
	Out.TargetWorldLocation = Out.bHasTarget ? Drone->EvaluatePathNow() : FVector::ZeroVector;

	// Trigger held whenever there is something to point at. Whether the barrel is on
	// it is the turret's report, not this operator's decision; stage 4 gives the gunner
	// the judgement to wait for ON TARGET, and a muzzle to make it matter.
	Out.bFiring = Out.bHasTarget;

	return true;
}
