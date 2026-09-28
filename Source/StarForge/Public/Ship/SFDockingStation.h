#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SFDockingStation.generated.h"

class AActor;
class UWorld;
class USFDockingPortComponent;
class USFShipFrameComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USFDockingStation : public UInterface
{
	GENERATED_BODY()
};

/**
 * What makes an actor a docking station: it says so (station-kit spec 6.7, decision 2).
 *
 * An interface on the actor rather than a flag on the port, because two of the three searches ask
 * about the actor -- sf.BindFrame station wants its frame, sf.AimAt station wants it as a target --
 * and a flag on a port would, at boarding, call a ship with such a port a station. The interface
 * says what an actor IS; the rejected test by composition said what it LACKED, and boarding would
 * hand a target exactly the docking component that test demanded it not have.
 *
 * The methods are pure, after IAbilitySystemInterface, so a class that declares the interface and
 * forgets one does not build. A class with a port that forgets to declare it at all is caught by
 * USFDockingPortComponent::BeginPlay, which says so in the log. There is no honest default here --
 * unlike ISFStationOperator, where "I am not flying" is one.
 */
class STARFORGE_API ISFDockingStation
{
	GENERATED_BODY()

public:
	/**
	 * This station's ports IN BUILD ORDER (Port0, Port1, ...), replacing whatever Out held. The
	 * nearest-free-port rule breaks exact ties by this order (spec 7.5), so it must not depend on
	 * the order GetComponents happens to return.
	 */
	virtual void GetDockingPorts(TArray<USFDockingPortComponent*>& Out) const = 0;

	/** The frame sf.BindFrame station binds to. */
	virtual USFShipFrameComponent* GetStationFrame() const = 0;

	/**
	 * A port of this station changed hands or state: captured, docked, releasing, let go. Called
	 * directly by the port, so a station that does not tick still hears every change.
	 */
	virtual void OnDockingPortChanged(USFDockingPortComponent* Port) = 0;

	/**
	 * Every docking station in World other than Except, SORTED BY NAME. Never by the order the
	 * iterator walks the level: that order is what the old warning in ResolvePorts called choosing
	 * "by nothing".
	 */
	static TArray<AActor*> FindAll(const UWorld* World, const AActor* Except = nullptr);
};
