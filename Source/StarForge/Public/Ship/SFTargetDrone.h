#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFTargetDrone.generated.h"

class UStaticMeshComponent;

/**
 * Something for the turret to track: a grey sphere on a scripted orbit.
 *
 * It exists because a fixed point cannot show that aiming tracks -- a barrel parked
 * on a stationary target is indistinguishable from a barrel that has stopped. The
 * orbit is kinematic and centred on an anchor's position in WORLD axes: it follows
 * the ship wherever it flies, but does not turn with it, so a rolling ship sees the
 * bearing sweep at the roll rate and the turret has to counter-rotate to hold it.
 *
 * No collision, no physics, no health. It is not a pirate and not a stage-4 target;
 * it is a moving point with a mesh on it so a person can see where the point is.
 *
 * The path is evaluated from the world clock on demand rather than read back from
 * this actor's tick, so whoever asks where the drone is gets this frame's answer
 * whatever the tick order. The mesh may trail the true point by a frame; nothing
 * aims at the mesh.
 */
UCLASS()
class STARFORGE_API ASFTargetDrone : public AActor
{
	GENERATED_BODY()

public:
	ASFTargetDrone();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Orbit radius about the anchor, METRES. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Drone")
	double OrbitRadiusMetres = 30.0;

	/** Rate along the orbit, degrees per second, from +X toward +Y seen from world +Z. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Drone")
	double OrbitRateDegPerSec = 10.0;

	/** Tilt of the orbit plane about world X, degrees. Zero is the world XY plane. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Drone")
	double OrbitTiltDegrees = 20.0;

	/** The actor whose position the orbit is centred on. Unset orbits the world origin. */
	void SetAnchor(AActor* InAnchor);

	/** Where the drone is at this instant, in world centimetres. */
	FVector EvaluatePathNow() const;

	/**
	 * False while sf.TargetDrone is 0: the drone is withdrawn and must be treated as
	 * absent by anyone tracking it. Absence is a case the turret has to report, and
	 * this is the switch that produces it.
	 */
	bool IsPresent() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<UStaticMeshComponent> Body;

	TWeakObjectPtr<AActor> Anchor;

	/** World time at BeginPlay; the orbit phase is measured from here. */
	double SpawnTimeSeconds = 0.0;

	/** Edge detection for the presence switch, so the log says it once, not sixty times a second. */
	bool bWasPresent = true;
};
