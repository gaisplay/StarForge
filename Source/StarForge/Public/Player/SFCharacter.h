#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SFCharacter.generated.h"

class UCameraComponent;
class USFShipboardMovementComponent;

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
class STARFORGE_API ASFCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	explicit ASFCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	USFShipboardMovementComponent* GetShipboardMovement() const;

	/** Degrees of view rotation per second of held arrow key. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Input")
	double LookRateDegPerSec = 90.0;

private:
	void PollMovementInput();
	void PollLookInput(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<UCameraComponent> Camera;
};
