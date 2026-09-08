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
};
