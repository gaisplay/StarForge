#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SFCombatGameMode.generated.h"

class ASFTestShip;

/**
 * The combat rig: the player's ship at the origin with a person in its cabin, and a pirate
 * 1000 m ahead and 150 m above, nose toward him. No station.
 *
 * A third rig rather than a change to either of the first two, for the reason the loop rig gives:
 * every figure this project has recorded was taken on those, and changing what they spawn would
 * devalue all of them at once.
 *
 * The pirate is ASFTestShip, crewed Automated -- the same class the player flies, with the pursuit
 * operator in its pilot's seat and the automation in its gunner's. There is no pirate class, and
 * spec 4's claim that a ship cannot tell who holds its posts is what that absence tests.
 *
 * Assembled in code on an empty map, like both other rigs, and it reports the states it read back
 * rather than the ones it attempted.
 */
UCLASS()
class STARFORGE_API ASFCombatGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASFCombatGameMode();

	virtual void StartPlay() override;

	/** Puts back the sf.ShipProfile StartPlay found. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TObjectPtr<ASFTestShip> Ship;

	UPROPERTY()
	TObjectPtr<ASFTestShip> Pirate;

	int32 ProfileBeforeRig = 1;
	bool bRestoreProfile = false;
};
