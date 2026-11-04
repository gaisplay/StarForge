#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ship/SFDockingStation.h"
#include "SFKitStation.generated.h"

class UPointLightComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USFDockingPortComponent;
class USFInteriorVolumeComponent;
class USFShipFrameComponent;

/** One face of an interior volume's envelope: which collider it is read from, and which side of its box. */
struct FSFKitFaceSource
{
	int32 Collider = INDEX_NONE;
	bool bMaxSide = false;
};

/**
 * An interior volume of a kit station and what the Station rig: line needs to judge it: the walls its
 * envelope is read from, face by face in the order XMin, XMax, YMin, YMax, ZMin, ZMax; the word the
 * line calls that envelope; and the axis sf.StationVolumeReach grows it along -- its block's port axis.
 * A face with several sources takes the tightest of them.
 */
struct FSFKitVolumeRecord
{
	int32 Volume = INDEX_NONE;
	const TCHAR* EnvelopeWord = TEXT("");
	FVector AuthoredCentreCm = FVector::ZeroVector;
	FVector AuthoredExtentCm = FVector::ZeroVector;
	FVector OutwardAxis = FVector::ZeroVector;
	TArray<FSFKitFaceSource> Faces[6];
};

/**
 * A station assembled from the kit's standard blocks (station-kit spec 6.1).
 *
 * Abstract, so it cannot be picked as a place's carrier: a kit station is a berth, a pier core, and
 * whatever the kit grows next -- each a subclass whose constructor calls BuildBlock. Stations are
 * described by code (spec 10.2 of the second phase, owner's decision 12), so a new assembly costs a
 * class and a rebuild, and this is the one place its blocks are built.
 *
 * A shared base does not contradict SFTestStation.h's refusal of one: that refused a base for the
 * ship and the station, which share a shape and not a life. Kit stations share both. ASFTestStation
 * stays outside the hierarchy -- every recorded figure was taken on it -- and only declares the same
 * interface.
 *
 * No simulating body and no tick, like the rig station: every collider is its own kinematic body, and
 * the station hears about its ports through OnDockingPortChanged instead of looking.
 */
UCLASS(Abstract)
class STARFORGE_API ASFKitStation : public AActor, public ISFDockingStation
{
	GENERATED_BODY()

public:
	ASFKitStation();

	/** Says the Station rig: line once the station is in play (spec 6.8). */
	virtual void BeginPlay() override;

	/**
	 * The Station rig: line: blocks, ports, volumes and physical colliders, the box they make in the
	 * station's own frame, each volume against its envelope, and each port -- what reaches past its
	 * plane, which volume does, and who holds it. Boxes, positions, the collider count and what each
	 * holder reports are read back from the world; the counts of blocks, ports and volumes are the
	 * builder's own. Written at BeginPlay, on every port change after it, and whenever a control is
	 * applied. An upper-case word is a state that did not hold, and the whole line then goes out as an
	 * Error.
	 */
	void ReportStationRig();

	virtual void GetDockingPorts(TArray<USFDockingPortComponent*>& Out) const override;
	virtual USFShipFrameComponent* GetStationFrame() const override { return Frame; }
	virtual void OnDockingPortChanged(USFDockingPortComponent* Port) override;

	USFShipFrameComponent* GetFrameComponent() const { return Frame; }

protected:
	/**
	 * Builds one M01 berth block. AssemblyOriginCm is the block's origin in spec.py's assembly frame,
	 * in CENTIMETRES (B_K3core: -3000 and +3000 along X). The first block built fixes the actor's root
	 * at the centre of its hall (spec 6.2); every later block is placed relative to the first.
	 * Constructor only: it creates default subobjects.
	 */
	void BuildBlock(const FVector& AssemblyOriginCm);

private:
	/** A box collider between two corners, in the actor's frame: a visible cube, or a pane with no mesh. */
	int32 AddCollider(FName Name, const FVector& MinCm, const FVector& MaxCm, bool bVisible);

	/** An interior volume between two corners, recorded with its authored size and outward axis. */
	int32 AddVolume(FName Name, const FVector& MinCm, const FVector& MaxCm, const TCHAR* EnvelopeWord, const FVector& OutwardAxis);

	UStaticMeshComponent* AddMarker(FName Name, UStaticMesh* Mesh, const FVector& LocationCm);

	/** One port's sentence of the line; sets bInOutRed when anything in it did not hold. */
	FString DescribePort(const USFDockingPortComponent& Port, const FTransform& StationFrameCm, const FBox& StationBoxM,
		const TArray<const UPrimitiveComponent*>& Physical, const TArray<FBox>& VolumeBoxesM, bool& bInOutRed) const;

	/** Who holds the port and in what state; the release only while the holder is docked (spec 6.8). */
	FString DescribeHolder(const USFDockingPortComponent& Port, bool& bInOutRed) const;

	/** A volume's envelope, in metres in the station's frame, read face by face from its walls' boxes. */
	FBox EnvelopeMetres(const FSFKitVolumeRecord& Record, const FTransform& StationFrameCm) const;

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USFShipFrameComponent> Frame;

	UPROPERTY()
	TArray<TObjectPtr<UPrimitiveComponent>> Colliders;

	UPROPERTY()
	TArray<TObjectPtr<USFInteriorVolumeComponent>> Volumes;

	/** In build order: GetDockingPorts hands them out in this order, and ties are broken by it. */
	UPROPERTY()
	TArray<TObjectPtr<USFDockingPortComponent>> Ports;

	UPROPERTY()
	TArray<TObjectPtr<UPointLightComponent>> Lights;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Markers;

	/** Parallel to Volumes. */
	TArray<FSFKitVolumeRecord> VolumeRecords;

	/** Each block's origin (spec.py's frame B) in the actor's frame, in build order. */
	TArray<FVector> BlockOriginsCm;

	bool bHasFirstBlock = false;
	FVector FirstAssemblyOriginCm = FVector::ZeroVector;
};
