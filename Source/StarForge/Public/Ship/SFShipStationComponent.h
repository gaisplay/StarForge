#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Ship/SFStationOperator.h"
#include "SFShipStationComponent.generated.h"

/**
 * A place on the ship with a role, a seat, and whoever is in it.
 *
 * This class knows nothing about what the station commands -- that belongs to the
 * subclass. It knows where the seat is, whether someone can reach it, and who is
 * sitting there. Keeping the two apart is what lets a gunner station reuse every
 * line of it without inheriting a single assumption about thrusters.
 */
UCLASS(ClassGroup = (StarForge), Abstract, meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFShipStationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFShipStationComponent();

	/** Where the operator sits, in the ship's frame, in METRES. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Station")
	FVector SeatLocationMetres = FVector::ZeroVector;

	/** How close an occupant must be to take the seat, in METRES. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Station")
	double InteractRadiusMetres = 2.0;

	/** What this station is called, for the readout and the log. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Station")
	FString StationName = TEXT("station");

	/**
	 * Seats an operator. Fails if the station is already taken -- two operators on
	 * one post would each believe they were flying, and the last one to be asked
	 * each frame would win silently.
	 */
	bool TryOccupy(TScriptInterface<ISFStationOperator> NewOperator);

	void Vacate();

	bool IsOccupied() const { return Operator.GetObject() != nullptr; }
	TScriptInterface<ISFStationOperator> GetOperator() const { return Operator; }

	/** The seat in the ship's frame, in centimetres, which is what the engine wants. */
	FVector GetSeatLocationCm() const;

	/**
	 * The interact radius, in centimetres. Each authored-in-metres quantity crosses
	 * the unit boundary in exactly one place; a second raw multiply elsewhere is how
	 * a factor of a hundred ends up applied twice without anything crashing or
	 * logging.
	 */
	double GetInteractRadiusCm() const;

	/** True when a world-space point is close enough to the seat to take it. */
	bool IsWithinReach(const FVector& OccupantWorldLocation) const;

protected:
	UPROPERTY()
	TScriptInterface<ISFStationOperator> Operator;
};
