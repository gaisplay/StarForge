#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SFPlaceGameMode.generated.h"

class ASFTestShip;
class USFPlaceDataAsset;

/**
 * The place rig: one place, assembled from data on an empty map.
 *
 * At its defaults it builds what the loop rig builds -- a station, a ship already docked to it
 * and a person in the cabin -- and that sameness is the acceptance, not a convenience. The loop
 * rig writes those three things in its own constructor; this one reads them out of an asset. If
 * the figures differ by so much as the hatch crossing, the difference is in the data path and
 * nowhere else.
 *
 * A FOURTH rig rather than a change to any of the first three. Every figure this project has
 * recorded was taken on L_FrameSpike, L_Loop or L_Combat, and moving one default on any of them
 * devalues all of those at once.
 *
 * Before anything is spawned the place is MEASURED (spec 10.4) and the measurement is printed,
 * whether it fits or not. A place that does not fit is not built at all: no carriers, no ship,
 * no pawn, and so nothing that could undock. That ordering is the whole point -- past the
 * engine's border a hull vanishes the frame its physics body comes on, with a clean log.
 */
UCLASS()
class STARFORGE_API ASFPlaceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASFPlaceGameMode();

	virtual void StartPlay() override;

private:
	/**
	 * Resolved in the constructor by path, guarded, exactly as ASFTestShip resolves its ship
	 * data. The map is empty by design, so there is no placed actor on which this could be
	 * authored; and the asset does not exist the first time this file is compiled, so a
	 * constructor that insisted on it could not be built even once.
	 */
	UPROPERTY()
	TObjectPtr<USFPlaceDataAsset> PlaceData;

	UPROPERTY()
	TObjectPtr<ASFTestShip> Ship;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedCarriers;
};
