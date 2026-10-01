#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFTestStation.generated.h"

class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class USFDockingPortComponent;
class USFInteriorVolumeComponent;
class USFShipFrameComponent;

/**
 * The second reference frame: a boxed room that does not move.
 *
 * It carries the same USFShipFrameComponent the ship does. The name lies by half for a
 * station, and it is kept anyway: renaming the facade would touch every file in the
 * project for the sake of a word, and the component's contract -- "where is this frame,
 * and how do I convert into it" -- is exactly as true here. Spec 8 says so explicitly.
 *
 * No simulating body. Since F1 its panels are physics colliders, so a hull stops at its walls,
 * but each is its own kinematic collider and nothing on it moves under physics. The frame
 * component derives velocity and acceleration as differences of its owner's transform
 * precisely so that an owner which is not a rigid body is still a full reference frame: the
 * knockdown of spec 6 works on the station
 * through the same code, and reads zero, because a station that does not move cannot
 * knock anyone off his feet.
 *
 * Assembled in the constructor from engine cubes for the same reason the ship is: the
 * loop map is empty by design and the game mode spawns this from StaticClass(), so
 * there is no placed actor on which anything could be authored.
 */
UCLASS()
class STARFORGE_API ASFTestStation : public AActor
{
	GENERATED_BODY()

public:
	ASFTestStation();

	virtual void BeginPlay() override;

	USFShipFrameComponent* GetFrameComponent() const { return Frame; }

	/** The station's half of the airlock, at the outboard mouth of the tunnel. */
	USFDockingPortComponent* GetDockingPort() const { return DockingPort; }

private:
	/**
	 * Builds one wall out of an engine cube. Duplicated from ASFTestShip deliberately:
	 * the two hulls share a shape, not a life -- one has a rigid body, posts, a turret
	 * and a flight profile, and the other never will -- and a common base class would
	 * make every later change to one of them a change to both.
	 */
	UStaticMeshComponent* AddBoxPanel(FName Name, const FVector& RelativeLocation, const FVector& RelativeScale);

	/** Same as the ship's: a shape attached with no collision, purely to read direction by. */
	UStaticMeshComponent* AddOrientationMarker(
		FName Name, UStaticMesh* Mesh, const FVector& RelativeLocation, double UniformScale);

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFShipFrameComponent> Frame;

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFDockingPortComponent> DockingPort;

	/** The room, out to the walls' inner faces. Whoever walks in rides the station. */
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFInteriorVolumeComponent> Interior;

	/** The tunnel's bore, from the port plane to the -Y wall's inner face. See the constructor. */
	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFInteriorVolumeComponent> TunnelInterior;

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<UPointLightComponent> InteriorLight;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Panels;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> OrientationMarkers;
};
