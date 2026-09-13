#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SFDockingPortComponent.generated.h"

/**
 * A docking socket, and nothing else.
 *
 * Its +X points out of the hull and its +Z is the deck's up. Two ports mate when their
 * origins coincide, their +X oppose and their +Z agree -- three axes, because that is
 * what makes the two decks line up and keeps the gravity direction a person walks
 * through in the airlock inside the leftover tolerance (spec 8).
 *
 * It knows nothing about docking. It does not move, latch, refuse or decide; it is a
 * place with an orientation, and USFDockingComponent is what reads it. The separation
 * is the same one that keeps USFTurretComponent from knowing who is aiming: the station
 * has a port and no docking component at all, because the station never docks with
 * anything -- the moving side owns the mechanism.
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
};
