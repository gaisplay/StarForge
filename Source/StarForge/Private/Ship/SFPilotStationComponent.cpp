#include "Ship/SFPilotStationComponent.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFShipDataAsset.h"
#include "Ship/SFThrusterArrayComponent.h"
#include "StarForgeLog.h"

namespace
{
	/**
	 * True while the rig is flying its own scripted manoeuvre.
	 *
	 * sf.ShipProfile is declared by the rig, so it is reached from here by name. The
	 * lookup is cached and complains once if it fails, because the answer matters: a
	 * silent false would let the post fly during a scripted run.
	 *
	 * Failing closed is not the alternative -- that would make the ship permanently
	 * unflyable over a missing debug variable. So the failure is carried out through
	 * bOutKnown instead, and from there into the readout: the one line a person
	 * compares against last week's run has to say the instrument is not sure, or a
	 * reopened Critical prints as a perfectly calm green.
	 *
	 * The test lives on the station rather than on any one operator because it has to
	 * bind every operator -- console, player or automation. Put on one of them, the
	 * others walk straight past it, which is what happened: sf.ConsoleOperator 0 plus
	 * a player in the seat re-opened the failure through the player's door.
	 */
	bool IsScriptedProfileRunning(bool& bOutKnown)
	{
		static IConsoleVariable* Profile =
			IConsoleManager::Get().FindConsoleVariable(TEXT("sf.ShipProfile"));

		bOutKnown = Profile != nullptr;

		if (!Profile)
		{
			static bool bComplained = false;
			if (!bComplained)
			{
				bComplained = true;
				UE_LOG(LogStarForge, Error,
					TEXT("sf.ShipProfile not found; the pilot station cannot tell whether the rig is "
						 "flying itself and will assume it is not."));
			}
			return false;
		}

		return Profile->GetInt() != 0;
	}
}

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

FString USFPilotStationComponent::GetOperatorDisplayName() const
{
	const FString Name = IsOccupied() ? Operator->GetOperatorName() : TEXT("unoccupied");

	// The doubt rides along in the name because the name is what both readouts print,
	// and a degraded instrument that only says so once at startup is a degraded
	// instrument nobody reading the run will know about.
	return bScriptedProfileUnknown ? Name + TEXT(" (profile unknown)") : Name;
}

void USFPilotStationComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Read before anything can return early, so the readout tells the truth about the
	// instrument whether or not anybody is in the seat. The pointer behind it is
	// resolved once, so this costs an integer read per frame.
	bool bProfileKnown = true;
	const bool bScriptedProfile = IsScriptedProfileRunning(bProfileKnown);
	bScriptedProfileUnknown = !bProfileKnown;

	if (!Thrusters || !Thrusters->IsReady() || !Thrusters->ShipData || !IsOccupied())
	{
		// Nobody is flying, so the last thing anybody asked for is not what is being
		// asked for now. Left standing it would print as a live stick position on a
		// ship nobody is commanding.
		LastIntent = StarForge::FFlightIntent();
		bOperatorFlying = false;
		if (Thrusters)
		{
			Thrusters->ClearDemand();
		}
		return;
	}

	// Not while the rig flies itself, and regardless of who holds the seat. The
	// scripted profile writes the hull's linear AND angular velocity directly, so a
	// wrench applied in the same frame is fighting an assignment it cannot win -- and
	// the manoeuvre every recorded drift figure was measured against stops being that
	// manoeuvre. Nothing orders the two against each other in TG_PrePhysics either, so
	// which of them the frame ends on is not even decided. Declining is also the plain
	// truth: nobody in that seat is flying this ship.
	if (bScriptedProfile)
	{
		LastIntent = StarForge::FFlightIntent();
		bOperatorFlying = false;
		Thrusters->ClearDemand();
		return;
	}

	// An operator who is not flying says so rather than handing back a zeroed intent:
	// zero means "stick centred", which with assist on is a command to stop.
	StarForge::FFlightIntent Intent;
	if (!Operator->ProvideFlightIntent(Intent))
	{
		LastIntent = StarForge::FFlightIntent();
		bOperatorFlying = false;
		Thrusters->ClearDemand();
		return;
	}

	LastIntent = Intent;
	bOperatorFlying = true;

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		Intent,
		Thrusters->ReadState(),
		Thrusters->GetMassProperties(),
		Thrusters->ShipData->BuildLimits(),
		Thrusters->GetArrayCapability());

	Thrusters->ApplyDemand(Demand, DeltaTime);
}
