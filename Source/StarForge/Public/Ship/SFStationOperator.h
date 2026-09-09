#pragma once

#include "CoreMinimal.h"
#include "FlightController.h"
#include "UObject/Interface.h"
#include "SFStationOperator.generated.h"

UINTERFACE(MinimalAPI, NotBlueprintable)
class USFStationOperator : public UInterface
{
	GENERATED_BODY()
};

/**
 * Whoever is sitting at a station.
 *
 * The ship asks this for an intent and does not care what answers -- a player, a
 * crewman, or the ship's own automation. That indifference is the whole point of
 * spec 4: there is no such thing as an "AI ship", only a ship whose posts are held
 * by automation.
 *
 * One method per intent type, deliberately. A single generic intent carrying a
 * variant would be more flexible, but a new role needs new code either way, and this
 * way it needs a visible edit to this interface rather than a silent one somewhere
 * else.
 *
 * The defaults return false rather than an empty intent. An operator who cannot fly
 * says so; a zeroed FFlightIntent is indistinguishable from hands resting on the
 * stick, and the ship would brake to a stop believing it had been told to.
 */
class STARFORGE_API ISFStationOperator
{
	GENERATED_BODY()

public:
	/** Fills Out and returns true if this operator is flying. */
	virtual bool ProvideFlightIntent(StarForge::FFlightIntent& Out) { return false; }

	/** Human-readable name for the readout and the log. */
	virtual FString GetOperatorName() const { return TEXT("unnamed"); }
};
