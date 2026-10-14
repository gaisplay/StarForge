#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SFDockingPortComponent.generated.h"

class USFDockingComponent;

/**
 * A docking socket that knows who holds it, and decides nothing.
 *
 * Its +X points out of the hull and its +Z is the deck's up. Two ports mate when their
 * origins coincide, their +X oppose and their +Z agree -- three axes, because that is
 * what makes the two decks line up and keeps the gravity direction a person walks
 * through in the airlock inside the leftover tolerance (spec 8).
 *
 * It does not move, latch, refuse or decide; it is a place with an orientation, and
 * USFDockingComponent is what reads it. It knows one fact since the station kit: WHO HOLDS it,
 * from the moment a hull begins its capture to the moment it is let go -- because spec 9 of the
 * second phase makes occupancy a state of the node, and a station that does not tick has to hear
 * of every change. The state of the hold (capturing, docked, releasing) is not duplicated here:
 * the holder knows it, and the pair keeps one opinion about who is attached to whom.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFDockingPortComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USFDockingPortComponent();

	virtual void BeginPlay() override;

	/** Where this port is in the world right now. */
	FTransform GetPortTransform() const { return GetComponentTransform(); }

	/** The docking component of the hull that holds this port, from EnterCapture to LetGo; null when free. */
	USFDockingComponent* GetHolder() const { return Holder.Get(); }

	bool IsHeld() const { return Holder.IsValid(); }

	/**
	 * Called by a docking component on each of its four transitions -- the capture begins, the
	 * capture completes, the release begins, the hull is let go -- with itself, or with null when
	 * it lets go. Sets the holder and tells the owner, if the owner is a docking station.
	 *
	 * A destroyed holder leaves the weak pointer empty WITHOUT an event, and the station hears
	 * nothing. No run of the station kit's steps 1-2 can destroy a docked hull (nothing fires;
	 * spec 6.7 item 4), and a subscription no run reaches is mechanism nobody can test; the stage
	 * that makes a docked hull destructible adds it together with the run.
	 */
	void NotifyHolder(USFDockingComponent* HolderOrNull);

private:
	TWeakObjectPtr<USFDockingComponent> Holder;
};
