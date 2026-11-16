#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SFPlaceDataAsset.generated.h"

/**
 * What kind of place this is (spec 8).
 *
 * One value, and that is the honest count. Spec 8 names three kinds and says they differ by
 * the CARRIER'S STATE OF MOTION rather than by decoration: open space stands still, an orbit
 * moves and accelerates, a surface turns with the planet. The last two break the docking
 * window and the felt-acceleration instrument, and spec 14 puts them in stage 7 for that
 * reason. An enumerator nothing can build is a value the log can print and nobody can reach,
 * so the other two arrive with the stage that can produce them -- one line each.
 *
 * The field exists at one value rather than not existing, because spec 10.3 lists the kind
 * among what a place carries, and the assembly line names it. What must not happen later is
 * the SHAPE of the data changing when the second kind arrives.
 */
UENUM()
enum class ESFPlaceKind : uint8
{
	OpenSpace
};

/**
 * One carrier standing in a place: a class and a pose, and nothing else.
 *
 * Spec 10.2 is the owner's decision and it is what makes this struct so thin: stations are
 * described by CODE, not by data, and the price is named -- a new kind of station costs a C++
 * class and a rebuild. What follows is the constraint that matters here: a generator may only
 * place carriers of classes that already exist, because a generator that produces a kind of
 * carrier is a generator that produces code.
 *
 * Position is in METRES and in the place's own frame, not the world's. The place's origin is
 * what moves; a carrier authored in world coordinates would have to be re-authored every time
 * the place moved, which is the same trap the docked pose avoids by being derived from the two
 * ports rather than written down.
 */
USTRUCT(BlueprintType)
struct STARFORGE_API FSFPlaceCarrier
{
	GENERATED_BODY()

	/**
	 * The actor class to spawn. AActor rather than a narrower base because the project has no
	 * common carrier base and inventing one would make ASFTestStation and ASFTestShip share a
	 * life they deliberately do not share. What actually makes an actor a carrier is a
	 * USFShipFrameComponent, and the assembly checks for one and says so when it is missing:
	 * an actor without the frame facade looks like a carrier on screen and is not one, and the
	 * only other way to notice is to try standing on it.
	 */
	UPROPERTY(EditAnywhere, Category = "Carrier")
	TSubclassOf<AActor> CarrierClass;

	/**
	 * The name the actor is spawned with, and therefore the name that appears in every log line
	 * about it -- including "Frame handover: Station -> Ship", which a person reads at the exact
	 * moment of the transition. "SFTestStation_0 -> SFTestShip_0" reads half as well.
	 */
	UPROPERTY(EditAnywhere, Category = "Carrier")
	FName CarrierName;

	/** In the PLACE's frame, in METRES. The mode converts to centimetres once, at the spawn. */
	UPROPERTY(EditAnywhere, Category = "Carrier")
	FVector LocationInPlaceMetres = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Carrier")
	FRotator Rotation = FRotator::ZeroRotator;
};

/**
 * How the player's ship begins on a place (station-kit spec 6.6).
 *
 * Docked is today's rig and the default, so every asset saved before this field existed reads
 * exactly as it did: a UDataAsset does not serialise what equals its class defaults, and the
 * default here is the old behaviour. Free exists for two controls that need a hull which is not
 * at a port: the latent-F1 reproductions, a hull standing against the rig station's wall
 * (N9, N9b), and the pier's all-ports-held control, a hull with nowhere to dock (N4).
 */
UENUM(BlueprintType)
enum class ESFShipStartMode : uint8
{
	Docked,
	Free
};

USTRUCT(BlueprintType)
struct STARFORGE_API FSFShipStart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Ship start")
	ESFShipStartMode Mode = ESFShipStartMode::Docked;

	/** Free only: where the hull's pivot starts, in the PLACE's frame, in METRES. */
	UPROPERTY(EditAnywhere, Category = "Ship start")
	FVector LocationInPlaceMetres = FVector::ZeroVector;

	/** Free only. */
	UPROPERTY(EditAnywhere, Category = "Ship start")
	FRotator Rotation = FRotator::ZeroRotator;

	/**
	 * Where the man is stood, from the cabin's spawn point, in the SHIP's axes and METRES, in either
	 * mode. Zero by default. There is no headless walking in this project -- no sf.* moves the pawn
	 * -- so a control that needs him against a cabin wall has to put him there.
	 */
	UPROPERTY(EditAnywhere, Category = "Ship start")
	FVector OccupantOffsetShipMetres = FVector::ZeroVector;

	/**
	 * Whether the player's ship spawns the rig's target drone, in either mode. True by default: the
	 * old behaviour, so an asset saved before this field existed reads as it did. The places with a
	 * kit station are written with it false (scripts/station_kit_places.py; owner's decision of
	 * 2026-11-16, station-kit spec 6.6). The drone orbits the ship at 30 m in world axes, and a ship
	 * docked at a berth has its centre 16 m from the block's face -- 12 m of boom and 4 m of its own
	 * half-width -- so that orbit runs through the block. Without the drone the gunner's automation
	 * starts with no target, and the turret says NO TARGET until sf.AimAt gives it one.
	 */
	UPROPERTY(EditAnywhere, Category = "Ship start")
	bool bTargetDrone = true;
};

/**
 * A place as data: where it is, how big it is, what kind it is and who stands in it.
 *
 * Spec 5: the map is empty and stays the same; what changes is the assembly. Both existing
 * rigs already prove that a world can be built from nothing by a game mode -- this is the same
 * trick with the description moved out of the mode's constructor and into something a person
 * can edit without a rebuild.
 *
 * The two ceilings a place is measured against are NOT here. A ceiling is a property of the
 * engine and of the owner's budget, not of a place, and twelve places each carrying their own
 * idea of where the world ends would be twelve different worlds. They live as derived defaults
 * on StarForge::Place::FPlaceLimits.
 */
UCLASS(BlueprintType)
class STARFORGE_API USFPlaceDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Appears in every line the assembly writes about this place. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Place")
	FName PlaceName = TEXT("OpenSpace");

	UPROPERTY(EditAnywhere, Category = "StarForge|Place")
	ESFPlaceKind Kind = ESFPlaceKind::OpenSpace;

	/**
	 * Which system this place belongs to (spec 10.3). Carried and printed; nothing reads it
	 * yet, and the stage that will is stage 8, where the graph groups places by system and
	 * routes between them.
	 *
	 * Left unnamed by default on purpose. The project has no named system, and authoring one
	 * here would be inventing fiction the spec does not carry -- which the assembly line would
	 * then print as though it meant something.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Place")
	FName SystemId;

	/** Where this place's origin sits in world coordinates, in METRES. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Place")
	FVector OriginFromWorldOriginMetres = FVector::ZeroVector;

	/**
	 * Half the place's extent, in METRES. 18 km by default, and that figure is derived rather
	 * than chosen: spec 6 works out that 90 s at spec 7's 400 m/s boost ceiling covers 36 km,
	 * so a place of exactly that diameter is one that can be crossed WITHIN THE SAME BUDGET IN
	 * ORDINARY SPACE. Which makes the rig's place the largest one the stage can build that
	 * still needs no transit to see the other side of.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Place")
	double RadiusMetres = 18000.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Place")
	TArray<FSFPlaceCarrier> Carriers;

	/** Docked by default: see FSFShipStart. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Place")
	FSFShipStart ShipStart;

	/**
	 * Replaces Carriers with the one the rig needs: an ASFTestStation at the place's origin.
	 *
	 * A button rather than four fields to fill by hand, for the same reason
	 * PopulateSymmetricLayout is one: a class path typed by hand does not announce a typo, it
	 * produces a place that assembles with no carriers at all and a rig line that reads
	 * "0 carriers" -- which looks like a mode that is not working rather than an asset that is
	 * not filled.
	 */
	UFUNCTION(CallInEditor, Category = "StarForge|Place")
	void PopulateOpenSpaceWithOneStation();

	/** "open space". Used by the assembly line, so it is asserted rather than trusted. */
	static const TCHAR* KindName(ESFPlaceKind InKind);
};
