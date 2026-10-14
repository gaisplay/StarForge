#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Place.h"
#include "World/SFPlaceDataAsset.h"
#include "SFPlaceGameMode.generated.h"

class ASFTestShip;
class IConsoleVariable;

/**
 * The place rig: one place, assembled from data on an empty map.
 *
 * At its defaults it builds what the loop rig builds -- a station, a ship already docked to it
 * and a person in the cabin -- and that sameness is the acceptance, not a convenience. The loop
 * rig writes those three things in its own StartPlay; this one reads them out of an asset. If
 * the figures differ by so much as the hatch crossing, the difference is in the data path and
 * nowhere else.
 *
 * A FOURTH rig rather than a change to any of the first three. Every figure this project has
 * recorded was taken on L_FrameSpike, L_Loop or L_Combat, and moving one default on any of them
 * devalues all of those at once.
 *
 * Before anything is spawned the place is MEASURED (spec 10.4) and the measurement is printed,
 * whether it fits or not. A place that does not fit is not built at all: no carriers, no ship,
 * no pawn, and so nothing that could undock. That ordering is the whole point. The border used
 * to delete a hull the frame its physics body came on, with one LogActor warning and nothing from
 * LogStarForge; since 2026-09-26 the project runs with WorldBounds.DisableChecks=1, nothing
 * deletes it there, and past 2^42 cm nothing has been measured, so this refusal is the only thing
 * standing at the border (Place.h).
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

	/** Puts back the sf.ShipProfile StartPlay found, and stops watching the place's variables. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Why the place stands refused, or None while it fits and before InitGame has measured it.
	 * For the panel: a refused place has no pawn, and all a HUD can say of that unprompted is that
	 * nobody is possessed -- in red, as if the rig were broken, over a refusal the control asked
	 * for. StarForge::Place::RefusalName gives the words the size line uses.
	 */
	StarForge::Place::EPlaceRefusal GetPlaceRefusal() const;

private:
	/**
	 * sf.PlaceOffsetM and sf.PlaceRadiusM are read once, in InitGame, and a change after it would
	 * do nothing in silence -- typed mid-run, it looks like a control that was performed. Said once
	 * per variable, as a warning.
	 */
	void OnPlaceVariableChanged(IConsoleVariable* Variable);

	/**
	 * Resolved in InitGame by path, guarded. The map is empty by design, so there is no placed
	 * actor on which this could be authored; and the asset may not exist, so nothing may insist
	 * on it. Not in the constructor: a static finder there is resolved once per process, and an
	 * editor started before the asset existed would keep the code-written place until it was
	 * restarted.
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

	/** How the player's ship begins, read with the rest of the place in InitGame. */
	UPROPERTY()
	FSFShipStart PlaceShipStart;

	/**
	 * Why sf.PlaceAsset refused the place, or empty. Held here and not in EPlaceRefusal because it
	 * is a fact about the engine's content, not about the size of a place -- the separation
	 * bApproachMeasured keeps beside EDockingRefusal.
	 */
	FString AssetRefusal;

	/** What sf.PlaceAsset asked for, printed back in both refusal lines. */
	FString RequestedAssetName;

	StarForge::Place::FPlaceFit PlaceFit;

	/** False until InitGame has run, so the pawn hook can tell "refused" from "not yet asked". */
	bool bPlaceMeasured = false;

	bool bPlaceFits = false;

	int32 ProfileBeforeRig = 1;
	bool bRestoreProfile = false;

	/** What the two variables read when the place was measured. */
	FString OffsetRawAtMeasure;
	FString RadiusRawAtMeasure;
	bool bOffsetChangeReported = false;
	bool bRadiusChangeReported = false;
	FDelegateHandle OffsetChangedHandle;
	FDelegateHandle RadiusChangedHandle;
};
