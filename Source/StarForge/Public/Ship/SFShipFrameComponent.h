#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SFShipFrameComponent.generated.h"

/**
 * Marks its owner as a reference frame that occupants can ride.
 *
 * This component deliberately moves nothing. It answers "where is this frame
 * right now" and converts between world and frame coordinates; carrying
 * occupants is the movement component's job. Keeping the two apart means no
 * tick-order dependency exists between a ship and the people standing in it.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFShipFrameComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFShipFrameComponent();

	/** This frame's transform in world space, sampled at the moment of the call. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FTransform GetFrameTransform() const;

	/** Expresses a world transform in this frame's coordinates. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FTransform WorldToLocal(const FTransform& World) const;

	/** Expresses a frame-local transform in world space. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FTransform LocalToWorld(const FTransform& Local) const;

	/**
	 * World transform an occupant must take to keep its frame-local transform
	 * after this frame moved from PrevFrame to where it is now.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FTransform CarryOccupant(const FTransform& PrevFrame, const FTransform& OccupantWorld) const;

	/** Angular velocity of this frame in radians per second, world space. */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FVector GetAngularVelocityRadPerSec() const;

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Previous sample, used only to derive angular velocity for readouts. */
	FTransform LastSampledTransform;
	FVector AngularVelocityRadPerSec = FVector::ZeroVector;
	bool bHasSample = false;
};
