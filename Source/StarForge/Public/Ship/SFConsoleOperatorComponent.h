#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Ship/SFStationOperator.h"
#include "SFConsoleOperatorComponent.generated.h"

/**
 * An operator that flies from console variables.
 *
 * It exists so the headless flight checks keep working once the control loop moves
 * into the station, and it is not a stub: it is the first implementation of
 * ISFStationOperator that is not a person, which is the claim spec 4 rests on. If
 * the ship can be flown by this without noticing, it can be flown by automation.
 *
 * It takes the pilot station at BeginPlay and holds it for as long as
 * `sf.ConsoleOperator` stays on, yielding it the moment that variable goes to zero.
 * A person takes the seat by turning the variable off first; the seat does not swap
 * hands on its own, so "who is flying" always has a name rather than being an
 * unowned branch that steers.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFConsoleOperatorComponent : public UActorComponent, public ISFStationOperator
{
	GENERATED_BODY()

public:
	USFConsoleOperatorComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	virtual bool ProvideFlightIntent(StarForge::FFlightIntent& Out) override;
	virtual FString GetOperatorName() const override { return TEXT("console"); }

	/**
	 * Whether this instance answers the unnamed sf.* family. See ESFShipCrewing.
	 *
	 * True by default, so a hull nobody told anything behaves exactly as it did when there
	 * was only one ship in the world -- which is what keeps both existing rigs unchanged.
	 */
	void SetConsoleControlled(bool bIn) { bConsoleControlled = bIn; }

private:
	bool bConsoleControlled = true;

	/** Whether a refusal has already been heard from the holder below. */
	bool bAskedForSeat = false;

	/**
	 * Who refused the post the last time this component asked for it.
	 *
	 * The pair exists so a refusal is logged once per change of holder rather than
	 * once per frame. It governs refused asks only: a free seat is taken every frame
	 * until it is held, decided before this pair is consulted. That order is the
	 * whole point -- the pointer is weak, so a destroyed holder reads back as null,
	 * which is indistinguishable from an empty post, and a limiter asked to tell
	 * those apart would sit out an empty seat forever.
	 */
	TWeakObjectPtr<UObject> LastSeatHolder;
};
