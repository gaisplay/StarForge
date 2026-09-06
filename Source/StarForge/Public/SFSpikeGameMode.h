#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SFSpikeGameMode.generated.h"

class ASFTestShip;

/**
 * Builds the measurement scene in code so the run reproduces from a clean
 * checkout with an empty map and no placed actors.
 */
UCLASS()
class STARFORGE_API ASFSpikeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASFSpikeGameMode();

	virtual void StartPlay() override;

private:
	UPROPERTY()
	TObjectPtr<ASFTestShip> Ship;
};
