#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "SFInteriorVolumeComponent.generated.h"

class USFShipFrameComponent;

/**
 * The inside of one carrier, as a volume: whoever walks in is bound to its frame.
 *
 * Entering binds; leaving does NOT unbind, and that asymmetry is the design. The two
 * volumes abut across the ship's hatch plane with the wall's thickness between them --
 * ten centimetres in which neither claims anybody. Unbinding on exit would make a person
 * nobody's for those ten centimetres, which means standing in world coordinates beside a
 * ship entitled to leave at 400 m/s. Binding on entry alone means the last binding holds
 * until a new one replaces it, and spec 8's "leaving one is always entering the other"
 * is true to within the thickness of a wall.
 *
 * It knows nothing about docking, and must not: an airlock handover and a boarding
 * action are the same event, which is what spec 8 says this mechanism buys.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFInteriorVolumeComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	USFInteriorVolumeComponent();

	virtual void BeginPlay() override;

	/**
	 * The frame this volume hands out: its owner's, resolved on the first ask and kept.
	 *
	 * Resolved lazily because a volume is asked whose frame it is before its own BeginPlay
	 * has run. A pawn begins play before the components of actors the game mode spawns in
	 * StartPlay, so the character's first binding reaches this getter while OwnerFrame is
	 * still null -- and a null here does not fall through to a better answer, it falls
	 * through to the guess the volumes exist to replace.
	 */
	USFShipFrameComponent* GetOwnerFrame() const;

	/**
	 * Whether a world point is inside this box, computed rather than looked up.
	 *
	 * The engine's overlap bookkeeping is the right answer at any settled moment and an
	 * unreliable one at BeginPlay, where it depends on the order components registered.
	 * "Whose reference frame am I in" is not allowed to depend on that, so the character's
	 * first binding has this to fall back on.
	 */
	bool ContainsPoint(const FVector& WorldPoint) const;

private:
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY()
	mutable TObjectPtr<USFShipFrameComponent> OwnerFrame;
};
