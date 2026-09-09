#pragma once

#include "CoreMinimal.h"
#include "FlightController.h"
#include "Ship/SFShipStationComponent.h"
#include "SFPilotStationComponent.generated.h"

class USFThrusterArrayComponent;

/**
 * The post that flies the ship.
 *
 * It owns the thruster array and runs its tick: ask the operator for an intent, turn
 * it into a demanded wrench, hand it to the thrusters. Spec 4 puts the loop here
 * rather than in the ship because the ship is not supposed to know it is being
 * flown -- only that a post commands its systems.
 *
 * It does not know who the operator is. That is what will let a crewman, the ship's
 * automation and a player share this one execution path instead of three.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFPilotStationComponent : public USFShipStationComponent
{
	GENERATED_BODY()

public:
	USFPilotStationComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** What was last asked of the ship, for the readout. */
	StarForge::FFlightIntent GetLastIntent() const { return LastIntent; }

	/**
	 * Who is flying, by name, or "unoccupied".
	 *
	 * The readout has to answer that question. An intent printed without one is a
	 * stick position with nobody's hand on it, which is exactly how a readout comes
	 * to look calm while nothing is happening.
	 */
	FString GetOperatorDisplayName() const;

	/**
	 * Whether the post actually commanded the ship on the last tick.
	 *
	 * Occupancy alone cannot answer it. An operator that declines -- because the rig
	 * is flying its scripted profile, or because he has stood up from the seat but
	 * still holds it -- leaves a zeroed intent behind, which is byte for byte what a
	 * centred stick on a ship at rest leaves behind. Without this flag the readout
	 * prints the two identically, and "the loop is not running" reads as "the loop is
	 * quiet" -- the same defect as a drift of zero printed with no measured time.
	 */
	bool IsOperatorFlying() const { return bOperatorFlying; }

	/**
	 * Whether the station has lost its way of telling the scripted profile is running.
	 *
	 * sf.ShipProfile is a debug variable, so a missing one must not ground the ship --
	 * the station goes on flying and assumes the rig is not flying itself. But that
	 * assumption is exactly the one that lets a scripted run and a commanded run be
	 * applied to the same hull in the same frame, so the doubt is published rather
	 * than swallowed: the display name says so, and the HUD colours the line for it.
	 */
	bool IsScriptedProfileStateUnknown() const { return bScriptedProfileUnknown; }

private:
	UPROPERTY()
	TObjectPtr<USFThrusterArrayComponent> Thrusters;

	StarForge::FFlightIntent LastIntent;

	/** Set only where an intent was actually provided; cleared on every other path. */
	bool bOperatorFlying = false;

	/** Refreshed at the top of every tick, ahead of any early return. */
	bool bScriptedProfileUnknown = false;
};
