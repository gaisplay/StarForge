#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShipDamage.h"
#include "SFDamageableComponent.generated.h"

class USFShipDataAsset;
class USFShipFrameComponent;
class USFThrusterArrayComponent;

/**
 * What can be shot: six shield faces, a hull strength, and a place for every hit.
 *
 * The world point comes in and is turned into ship coordinates through USFShipFrameComponent
 * -- the facade spec 6 calls the only thing in this project that knows about world space.
 * Doing the inverse transform here instead would be a second answer to a question that
 * already has one, and the two would differ the first time anything is attached to anything.
 *
 * On zero hull strength the ship is POWERED DOWN, not destroyed: the thruster array is locked
 * with the same SetLocked docking uses, the pilot's post stands aside without being told why,
 * the hull coasts on its momentum, and a person inside stays aboard and stays bound. Nothing
 * is destroyed, on either ship. The handoff for this stage is explicit that a damaged
 * thruster, a powered-down ship and a hull taken by a pirate are all the same lock, and that
 * stage 4 must use it rather than grow a second one.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFDamageableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFDamageableComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Shield and hull figures (spec 9). Without it nothing here can be hurt, and it says so. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Damage")
	TObjectPtr<USFShipDataAsset> ShipData;

	/**
	 * Half-extents of the hull in centimetres, for classifying a hit into a face.
	 *
	 * Set by the owner from its own geometry rather than authored here, the same way the
	 * seats take their height from InteriorHalfExtentCm. A number typed twice is a number
	 * that will be right once.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Damage")
	FVector HalfExtentsCm = FVector(400.0, 400.0, 400.0);

	/**
	 * One hit of Damage at a world point, in centimetres. Classified into a face in ship axes,
	 * taken by that face's shield, and whatever the face cannot absorb goes to the hull.
	 *
	 * A hit that cannot be placed -- no frame, or a point or half-extent the maths refuses --
	 * is dropped with an error rather than put on a face nobody can defend.
	 */
	void ReceiveHit(const FVector& HitWorldCm, double Damage, AActor* Instigator);

	double GetHullStrength() const { return HullStrength; }
	double GetHullCapacity() const { return HullCapacity; }

	/** The charge on one face; zero for a value outside the six. */
	double GetFaceCharge(StarForge::Damage::EShipFace Face) const;

	/** True from the hit that took the hull to zero. See PowerDown for what that does and does not hold. */
	bool IsPoweredDown() const { return bPoweredDown; }

private:
	/**
	 * Damage that got past the shield, applied to the hull strength, which stops at zero.
	 * Returns true only on the hit that takes it from above zero to zero: the event, not the
	 * state, for the same reason ApplyHit reports a face broken only once.
	 *
	 * The place and the shooter reach this path and nothing here reads them yet. The hull path
	 * is where a hit's place decides something (spec 5: a hit near a thruster destroys it), and
	 * the task that adds that reads them here.
	 */
	bool ApplyToHull(double Damage, const FVector& ShipLocalCm, AActor* Instigator);

	/**
	 * Locks the thruster array and says so, once.
	 *
	 * The lock is NOT exclusive, and nothing here pretends it is. SetLocked is a single bool
	 * with no owner and no reason attached: USFDockingComponent::Undock calls SetLocked(false)
	 * unconditionally, so a ship powered down while docked gets its engines back the moment it
	 * undocks, while IsPoweredDown still reads true. Lock reasons are deferred by the stage-4
	 * plan (item 7 of its closing list), and no rig in this stage can damage a docked ship.
	 * Until they exist, whichever caller of SetLocked writes last is the one that wins.
	 */
	void PowerDown();

	UPROPERTY()
	TObjectPtr<USFShipFrameComponent> Frame;

	UPROPERTY()
	TObjectPtr<USFThrusterArrayComponent> Thrusters;

	StarForge::Damage::FShieldLimits Limits;
	StarForge::Damage::FShieldState Shields;

	double HullStrength = 0.0;
	double HullCapacity = 0.0;

	/** False until BeginPlay has read figures it can use; a hit before then is refused out loud. */
	bool bReady = false;

	bool bPoweredDown = false;
};
