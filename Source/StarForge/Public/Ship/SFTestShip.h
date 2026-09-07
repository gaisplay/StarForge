#pragma once

#include "CoreMinimal.h"
#include "FlightController.h"
#include "GameFramework/Actor.h"
#include "SFTestShip.generated.h"

class USFShipFrameComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UBoxComponent;
class USFThrusterArrayComponent;

/**
 * Throwaway measurement rig: a boxed room that flies a scripted manoeuvre at
 * the design's speed and rotation ceilings.
 *
 * Motion is driven by commanding the hull body's velocity every tick rather
 * than by integrating forces, so the manoeuvre stays the exact repeatable
 * worst case checkpoint 1 was measured against. The transform now comes from
 * Chaos rather than from setting it directly, which is the one variable this
 * task changes.
 */
UCLASS()
class STARFORGE_API ASFTestShip : public AActor
{
	GENERATED_BODY()

public:
	ASFTestShip();

	virtual void Tick(float DeltaSeconds) override;

	/** Boost ceiling from the design, converted to Unreal units: 400 m/s. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Profile")
	double PeakSpeedCms = 40000.0;

	/** Pitch and yaw ceiling from the design. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Profile")
	double PeakPitchYawRateDegPerSec = 120.0;

	/** Roll ceiling from the design. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Profile")
	double PeakRollRateDegPerSec = 180.0;

	/** Seconds for one full sweep of the manoeuvre profile. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Profile")
	double ProfilePeriodSeconds = 8.0;

	/**
	 * Interior half-extent in centimetres, measured from the pivot.
	 *
	 * Read-only on purpose. The six hull panels bake their transforms from this
	 * value in the constructor, while the spawn point recomputes from it on every
	 * call. Editing it on a placed instance would move the spawn without moving
	 * the walls, leaving the occupant inside a plate with no error raised. Change
	 * the default here in C++ instead.
	 */
	UPROPERTY(VisibleAnywhere, Category = "StarForge|Profile")
	double InteriorHalfExtentCm = 400.0;

	USFShipFrameComponent* GetFrameComponent() const { return Frame; }

	/** The simulated body. Task 5 applies thrust here. */
	UBoxComponent* GetHull() const { return Hull; }

	USFThrusterArrayComponent* GetThrusters() const { return Thrusters; }

	/** Where a passenger should be placed so he starts standing on the floor. */
	FVector GetInteriorSpawnWorldLocation() const;

	/** This tick's linear acceleration in m/s^2, for the grav-plate readout. */
	FVector GetLinearAccelMetersPerSecSq() const { return LinearAccelMetersPerSecSq; }

	/** This tick's linear speed in m/s, so the readout shows where in the profile we are. */
	double GetSpeedMetersPerSec() const { return PreviousVelocityCms.Size() / 100.0; }

	/** What was last asked of the ship, for the readout. */
	StarForge::FFlightIntent GetLastIntent() const { return LastIntent; }

private:
	/** Repeats the checkpoint manoeuvre by commanding velocities directly. */
	void TickScriptedProfile();

	/** Flies on thrust from the console intent. Needs the step for thruster spool-up. */
	void TickFlight(double DeltaSeconds);

	/** Builds floor and four walls out of engine cubes, so the rig needs no content. */
	UStaticMeshComponent* AddBoxPanel(FName Name, const FVector& RelativeLocation, const FVector& RelativeScale);

	/**
	 * Attaches an already-resolved shape to Hull with no collision, so it rides the
	 * ship purely as a visual orientation cue and cannot interfere with the walk test
	 * or the drift measurement. The mesh is resolved by the caller via a static
	 * ConstructorHelpers::FObjectFinder, same as the hull panels, because each marker
	 * needs a different asset and a single shared finder cannot serve more than one path.
	 */
	UStaticMeshComponent* AddOrientationMarker(
		FName Name, UStaticMesh* Mesh, const FVector& RelativeLocation, double UniformScale);

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<UBoxComponent> Hull;

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFShipFrameComponent> Frame;

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFThrusterArrayComponent> Thrusters;

	/**
	 * Lights the interior from the room's centre so the walk test is visible at all.
	 * Attached to Hull, so it travels and rotates with the ship like everything else
	 * on board.
	 */
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<UPointLightComponent> InteriorLight;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Panels;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> OrientationMarkers;

	double ElapsedSeconds = 0.0;
	FVector PreviousVelocityCms = FVector::ZeroVector;
	FVector LinearAccelMetersPerSecSq = FVector::ZeroVector;

	StarForge::FFlightIntent LastIntent;
};
