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

private:
	UPROPERTY()
	TObjectPtr<USFThrusterArrayComponent> Thrusters;

	StarForge::FFlightIntent LastIntent;
};
