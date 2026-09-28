#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "SFInteriorVolumeComponent.generated.h"

class ASFCharacter;
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
 *
 * Entering binds only while the carrier he is bound to and this one move together where he
 * stands (see TryHandover). A hull striking another brings its volume onto whoever stands at
 * the struck wall, and a handover then would put him on a carrier moving at the speed of the
 * collision. That is the one thing this class does know about motion, and it is asked of the
 * two frames, not of docking.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFInteriorVolumeComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	USFInteriorVolumeComponent();

	virtual void BeginPlay() override;

	/** Ticks only while somebody inside this volume has been refused a handover (see TryHandover). */
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

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

	/**
	 * How fast two carriers move relative to each other at a world point, in metres per second:
	 * |(vA + wA x rA) - (vB + wB x rB)|, each r from that frame's origin to the point. Read from
	 * the frame components' own velocities, so a docked hull attached to a still station reads the
	 * station's zero.
	 *
	 * The arithmetic itself is StarForge::ShipFrame::RelativeSpeedAtPointMetresPerSec, where it can
	 * be tested; this reads the two frames, converts the one length through StarForge::Units, and
	 * returns a NaN where that function refuses, which the caller's negated comparison treats as a
	 * refusal. WorldPointCm is centimetres, as its name says, and the result is not.
	 */
	static double RelativeSpeedAtMetresPerSec(
		const USFShipFrameComponent& A, const USFShipFrameComponent& B, const FVector& WorldPointCm);

private:
	/**
	 * Binds Character to this volume's frame unless he is bound to another carrier that moves
	 * relative to this one, where he stands, faster than the handover limit. A first binding --
	 * nothing to hand him from -- is not a handover and always goes through. A refusal is said
	 * once per episode and retried every tick while he stays in the volume.
	 */
	void TryHandover(ASFCharacter* Character);

	/**
	 * Whether another interior volume of this volume's owner still holds Character in an open refused
	 * episode. The REFUSED line promises silence "until he leaves <carrier>'s volume", naming the
	 * carrier and not the box, so a carrier built of several boxes says it once for all of them
	 * (station-kit spec 6.4, 6.7 item 8).
	 */
	bool IsRefusedByAnotherVolumeOfThisCarrier(const ASFCharacter* Character) const;

	/** An accepted handover closes the episode on every volume of the owner, not only this one. */
	void ForgetRefusalOnEveryVolumeOfThisCarrier(const ASFCharacter* Character);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

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

	/** Inside this volume, refused, and not yet told again: the open episodes. */
	TArray<TWeakObjectPtr<ASFCharacter>> RefusedOccupants;
};
