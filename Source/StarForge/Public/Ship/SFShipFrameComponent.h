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

	/**
	 * Angular velocity of this frame in radians per second, world space.
	 *
	 * Read from the body where the carrier has one and differenced from its transform
	 * where it has none. TickComponent says why the two cannot be the same read.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FVector GetAngularVelocityRadPerSec() const;

	/**
	 * Linear velocity of this frame in metres per second, world space.
	 *
	 * On a carrier with no body it is the mean over the last frame, which belongs to that frame's
	 * middle rather than its end; StarForge::ShipFrame::EstimateStepVelocity says why.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FVector GetLinearVelocityMetersPerSec() const { return LinearVelocityMetersPerSec; }

	/**
	 * Linear acceleration of this frame in metres per second squared, world space.
	 *
	 * The felt-acceleration figure of spec 6 is this plus the centripetal term from
	 * the angular velocity above, and the knockdown is decided on it. It is derived
	 * here, by differencing the transform on the time base of whatever moved it
	 * (TickComponent says what that is; sf.FeltTimeBase 0 undoes it), so that the
	 * movement component reads both halves of the figure off one component: the ship
	 * used to compute this one from its Chaos velocity, which meant the movement
	 * component would have had to know the test ship's class to read it.
	 *
	 * One component, but no longer one sampling. On a simulating hull the angular half
	 * is the body's instantaneous rate and this half is a second difference over two
	 * frames, so a collision reads its peak turn on the frame it happens and its felt
	 * acceleration a frame later. That difference is why the duel's `Footing LOST` line
	 * reports a turn in the hundreds of deg/s where the once-a-second flight readout,
	 * sampled between impacts, reports tens.
	 */
	UFUNCTION(BlueprintPure, Category = "StarForge|Frame")
	FVector GetLinearAccelMetersPerSecSq() const { return LinearAccelMetersPerSecSq; }

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Previous sample, used only to derive the velocities below. */
	FTransform LastSampledTransform;
	FVector AngularVelocityRadPerSec = FVector::ZeroVector;
	bool bHasSample = false;

	/** Which read produced the rate above, so a change of source can be said once. */
	bool bReadsFromBody = false;

	FVector LinearVelocityMetersPerSec = FVector::ZeroVector;
	FVector LinearAccelMetersPerSecSq = FVector::ZeroVector;

	/** Where in its frame the velocity above belongs, as an offset from that frame's end. */
	double LastVelocityStampOffsetSeconds = 0.0;

	/** False until there is a velocity to difference; the first two ticks report zero acceleration. */
	bool bHasVelocitySample = false;

	/** Whether Chaos was simulating the root at the last sample: half of what makes a step simulated. */
	bool bLastSampleSimulating = false;

	/** The sf.FeltTimeBase the velocity above was estimated under, so no difference spans a change of it. */
	bool bLastVelocityOnTimeBase = true;

	/** Async physics or substepping has been reported for this carrier. */
	bool bWarnedPhysicsStepping = false;
};
