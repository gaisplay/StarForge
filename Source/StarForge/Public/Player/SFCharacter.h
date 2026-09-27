#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Ship/SFStationOperator.h"
#include "SFCharacter.generated.h"

class ASFTestShip;
class UCameraComponent;
class USFShipboardMovementComponent;
class USFShipFrameComponent;
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

	/**
	 * Rides the given frame from now on. Called by an interior volume on entry and by
	 * sf.BindFrame; nothing else has business changing it.
	 */
	void BindFrame(USFShipFrameComponent* Frame);

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

	/** F, from the pilot's seat: docks or undocks. Says so from anywhere else. */
	void PollDockInput();

	/** sf.BindFrame, acted on when the value changes. The negative control's door. */
	void PollBindFrameCommand();

	/** V on its edge and sf.View on its change, and the return to the eyes when the watched ship is gone. */
	void PollViewInput();

	/**
	 * The one place V and sf.View both land. True shows the chase camera of his ship, false his
	 * own eyes. Only the view target changes: the pawn stays possessed, because spec 4 says
	 * possession never changes, and so every key he holds still walks, looks or flies him.
	 */
	void SetChaseView(bool bChase);

	/** Whose chase camera V shows; null when there is no ship to look at. The order is in the .cpp. */
	ASFTestShip* FindShipToWatch() const;

	/** Tries to take the nearest station within reach, or leaves the current one. */
	void ToggleSeat();

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TObjectPtr<USFShipStationComponent> SeatedStation;

	/** Edge detection for the interact key: held is not pressed. */
	bool bInteractWasDown = false;

	/** Edge detection for the dock key: held is not pressed. */
	bool bDockWasDown = false;

	/** Last value seen on sf.BindFrame, so it acts on a change and not on a level. */
	FString LastBindFrameCommand;

	/** Edge detection for the view key: held is not pressed. */
	bool bViewWasDown = false;

	/** Last value seen on sf.View, so it acts on a change and not on a level. Starts at its default. */
	int32 LastViewCommand = 0;

	/** The ship whose chase camera is on screen. Weak: watching a ship is not keeping it alive. */
	TWeakObjectPtr<ASFTestShip> WatchedShip;

	/**
	 * Beside the pointer, not folded into it: a weak pointer to a destroyed ship and one that was
	 * never set both read as null, and only the first is a view that has to be taken back.
	 */
	bool bWatchingShip = false;
};
