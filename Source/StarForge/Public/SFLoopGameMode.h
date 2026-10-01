#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SFLoopGameMode.generated.h"

class ASFTestShip;
class ASFTestStation;

/**
 * The loop rig: a station, a ship already docked to it, and a person standing in the
 * ship's cabin.
 *
 * A second rig rather than a change to the first. L_FrameSpike does not move a single
 * default: every figure this project has recorded was taken on it, and changing what it
 * spawns would devalue all of them at once. Spec 8 says so in as many words.
 *
 * The scene is assembled in code for the reason the spike's is: the map is empty by
 * design, so there is no placed actor on which anything could be authored, and a clean
 * checkout reproduces the run exactly.
 */
UCLASS()
class STARFORGE_API ASFLoopGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASFLoopGameMode();

	virtual void StartPlay() override;

	/** Puts back the sf.ShipProfile StartPlay found, so the rig's 0 does not outlive its world. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TObjectPtr<ASFTestStation> Station;

	UPROPERTY()
	TObjectPtr<ASFTestShip> Ship;

	int32 ProfileBeforeRig = 1;
	bool bRestoreProfile = false;
};
