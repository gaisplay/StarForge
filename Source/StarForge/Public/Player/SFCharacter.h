#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Ship/SFStationOperator.h"
#include "SFCharacter.generated.h"

class UCameraComponent;
class USFShipboardMovementComponent;
class USFShipStationComponent;

/**
 * First-person occupant used by the measurement rig.
 *
 * Input is polled directly from the controller rather than routed through
 * Enhanced Input. The rig must run from a bare C++ build with no content at
 * all; a missing mapping context would present as "the character will not
 * move" and cost more time than the whole measurement is worth. Real input
 * arrives with the flight phase.
 */
UCLASS()
class STARFORGE_API ASFCharacter : public ACharacter, public ISFStationOperator
{
	GENERATED_BODY()

public:
	explicit ASFCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	USFShipboardMovementComponent* GetShipboardMovement() const;

	virtual bool ProvideFlightIntent(StarForge::FFlightIntent& Out) override;
	virtual FString GetOperatorName() const override { return TEXT("player"); }

	bool IsSeated() const { return SeatedStation != nullptr; }
	USFShipStationComponent* GetSeatedStation() const { return SeatedStation; }

	/** Degrees of view rotation per second of held arrow key. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Input")
	double LookRateDegPerSec = 90.0;

	/** Fraction of stick travel treated as centre. Physical sticks rest off-centre. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Input")
	double StickDeadZone = 0.1;

private:
	void PollMovementInput();
	void PollLookInput(float DeltaSeconds);
	void PollInteract();

	/** Tries to take the nearest station within reach, or leaves the current one. */
	void ToggleSeat();

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<USFShipStationComponent> SeatedStation;

	/** Edge detection for the interact key: held is not pressed. */
	bool bInteractWasDown = false;
};
