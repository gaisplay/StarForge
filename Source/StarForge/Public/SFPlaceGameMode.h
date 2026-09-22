#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Place.h"
#include "World/SFPlaceDataAsset.h"
#include "SFPlaceGameMode.generated.h"

class ASFTestShip;

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
 *
 * The measurement is in InitGame and the assembly in StartPlay, and the split is not tidiness.
 * In standalone -game the engine logs the local player in, calls PostLogin, and spawns his pawn
 * BEFORE StartPlay runs, so a rig that decides about the pawn in StartPlay decides too late.
 * InitGame is the one hook that precedes the login.
 */
UCLASS()
class STARFORGE_API ASFPlaceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASFPlaceGameMode();

	/** Measures the place. Runs before the local player logs in; assembles nothing. */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	virtual void StartPlay() override;

	/**
	 * Answers nothing while the place stands refused, which is what actually stops the pawn: the
	 * engine spawns a default pawn only through this hook (GameModeBase.cpp 1291), and it asks at
	 * login, long before StartPlay.
	 */
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

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

	/**
	 * The place InitGame measured, and the measurement itself. StartPlay assembles from these
	 * rather than re-reading the asset and the console: reading twice is two places, and the size
	 * line would then describe one of them while the world was built from the other.
	 */
	FName PlaceName;
	ESFPlaceKind PlaceKind = ESFPlaceKind::OpenSpace;
	FName PlaceSystemId;
	FVector PlaceOriginMetres = FVector::ZeroVector;
	double PlaceRadiusMetres = 0.0;
	bool bPlaceFromAsset = false;

	UPROPERTY()
	TArray<FSFPlaceCarrier> PlaceCarriers;

	StarForge::Place::FPlaceFit PlaceFit;

	/** False until InitGame has run, so the pawn hook can tell "refused" from "not yet asked". */
	bool bPlaceMeasured = false;

	bool bPlaceFits = false;
};
