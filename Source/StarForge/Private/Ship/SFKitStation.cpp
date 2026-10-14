#include "Ship/SFKitStation.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Ship/SFDockingPortComponent.h"
#include "Ship/SFInteriorVolumeComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "StarForgeLog.h"
#include "UObject/ConstructorHelpers.h"
#include "Units.h"

namespace
{
/**
 * Nested and named for the unity build: every file's anonymous namespace is one namespace in this
 * module, and SFTestShip.cpp already owns CubeEdgeCm and MarkerUniformScale at its top level. Inside
 * a function with `using namespace KitStation;` those two are therefore written qualified.
 */
namespace KitStation
{
	/** One engine cube is 100 cm on a side, so a mesh scale equals metres. */
	constexpr double CubeEdgeCm = 100.0;

	// The M01 berth block in spec.py's block frame B, in WHOLE CENTIMETRES: origin at the centre of the
	// bottom face, X along the berth face, -Y out of it, Z up. Every figure is spec.py's -- BLOCK, HALL,
	// HALL_WINDOW, BOOM_INNER, BOOM_WALL, BOOM_AXIS_X, HALL_FLOOR_Z -- or derived below. Whole
	// centimetres because control N6's tie is exact only if the ports are (station-kit spec 7.5), and
	// because the red threshold of the Station rig: line is derived from that grain (spec 6.8).
	constexpr double BlockHalfXCm = 3000.0;         // BLOCK x 60 [A]
	constexpr double BlockHalfYCm = 1200.0;         // BLOCK y 24 [A]
	constexpr double BlockHeightCm = 3200.0;        // BLOCK z 32 [A]
	constexpr double DeckZCm = 1200.0;              // HALL_FLOOR_Z: deck 4 [I]
	constexpr double HallTopZCm = 2000.0;           // HALL z 12..20 [A]
	constexpr double HallXNegCm = -1962.0;          // HALL x -19.62 [A]
	constexpr double HallXPosCm = -362.0;           // HALL x -3.62 [A]
	constexpr double HallBackYCm = 0.0;             // HALL y ..0 [A]
	constexpr double WallCm = 20.0;                 // BOOM_WALL 0.2, and the rig's panel thickness
	constexpr double WindowSillZCm = 1300.0;        // HALL_WINDOW sill_z [A]
	constexpr double WindowHeadZCm = 1900.0;        // HALL_WINDOW head_z [A]
	constexpr double BoomAxisXCm = -1162.0;         // BOOM_AXIS_X [I]
	constexpr double BoomHalfWidthCm = 100.0;       // BOOM_INNER 2.0 / 2
	constexpr double BoomInnerHeightCm = 250.0;     // BOOM_INNER 2.5
	constexpr double BoomLengthCm = 1200.0;         // BOOM_LEN 12 [I]

	// Derived, not authored.
	constexpr double FrontOuterYCm = -BlockHalfYCm;                         // -1200: the berth face
	constexpr double FrontInnerYCm = FrontOuterYCm + WallCm;                // -1180: inside the envelope (spec 6.3)
	constexpr double PortPlaneYCm = FrontOuterYCm - BoomLengthCm;           // -2400
	constexpr double BoomTopZCm = DeckZCm + BoomInnerHeightCm;             // 1450
	constexpr double PortZCm = DeckZCm + BoomInnerHeightCm * 0.5;           // 1325: floor + half the hatch, as both rig ports
	constexpr double DoorXNegCm = BoomAxisXCm - BoomHalfWidthCm;            // -1262
	constexpr double DoorXPosCm = BoomAxisXCm + BoomHalfWidthCm;            // -1062

	/** The hall's centre in B, which is the actor's root (spec 6.2): with the station at a place's origin, a pawn spawned there stands in Hall0. */
	const FVector HallCentreB(
		(HallXNegCm + HallXPosCm) * 0.5, (FrontInnerYCm + HallBackYCm) * 0.5, (DeckZCm + HallTopZCm) * 0.5);   // (-1162, -590, 1600)

	// spec.py sets neither the light nor the markers; spec 6.3 asks for them derived as the rig does it.
	constexpr double MarkerUniformScale = 0.8;       // the rig station's: the size says which carrier you are in
	constexpr double MarkerWallClearanceXCm = 140.0; // the rig: inner half-X 590 less its marker offset 450
	constexpr double MarkerWallClearanceYCm = 90.0;  // the rig: inner half-Y 390 less its marker offset 300
	constexpr double LightIntensityCandelas = 2000.0;// the rig's, with auto-exposure off
}
}

ASFKitStation::ASFKitStation()
{
	// Nothing to do per frame. The frame component ticks itself to sample the transform, which is
	// constant here, and the station hears about its ports through OnDockingPortChanged.
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	Frame = CreateDefaultSubobject<USFShipFrameComponent>(TEXT("Frame"));
}

void ASFKitStation::BuildBlock(const FVector& AssemblyOriginCm)
{
	using namespace KitStation;

	if (!bHasFirstBlock)
	{
		bHasFirstBlock = true;
		FirstAssemblyOriginCm = AssemblyOriginCm;
	}

	const int32 Block = BlockOriginsCm.Num();

	// The block's origin in the actor's frame: its offset from the first block, less the first hall's
	// centre, which is where the root is. Whole centimetres in, whole centimetres out.
	const FVector OriginCm = (AssemblyOriginCm - FirstAssemblyOriginCm) - HallCentreB;
	BlockOriginsCm.Add(OriginCm);

	const auto Name = [Block](const TCHAR* Part) { return FName(*FString::Printf(TEXT("Block%d%s"), Block, Part)); };
	const auto B = [&OriginCm](double X, double Y, double Z) { return OriginCm + FVector(X, Y, Z); };

	// The fifteen colliders of spec 6.3, in its table's order. The outer faces lie ON the block's
	// envelope, so two blocks side by side touch and do not overlap; the walked floors put their top
	// face on the deck line, which is the docked ship's cabin floor; the channel's walls stand outside
	// its 2.0 x 2.5 bore, as the rig tunnel's do; and the channel's end faces lie on the port plane.
	const int32 Base = AddCollider(Name(TEXT("Base")),
		B(-BlockHalfXCm, -BlockHalfYCm, 0.0), B(BlockHalfXCm, BlockHalfYCm, DeckZCm), true);
	const int32 Roof = AddCollider(Name(TEXT("Roof")),
		B(-BlockHalfXCm, -BlockHalfYCm, HallTopZCm), B(BlockHalfXCm, BlockHalfYCm, BlockHeightCm), true);
	const int32 SideXNeg = AddCollider(Name(TEXT("SideXNeg")),
		B(-BlockHalfXCm, -BlockHalfYCm, DeckZCm), B(HallXNegCm, BlockHalfYCm, HallTopZCm), true);
	const int32 SideXPos = AddCollider(Name(TEXT("SideXPos")),
		B(HallXPosCm, -BlockHalfYCm, DeckZCm), B(BlockHalfXCm, BlockHalfYCm, HallTopZCm), true);
	const int32 Back = AddCollider(Name(TEXT("Back")),
		B(HallXNegCm, HallBackYCm, DeckZCm), B(HallXPosCm, BlockHalfYCm, HallTopZCm), true);
	AddCollider(Name(TEXT("FrontSillXNeg")),
		B(HallXNegCm, FrontOuterYCm, DeckZCm), B(DoorXNegCm, FrontInnerYCm, WindowSillZCm), true);
	AddCollider(Name(TEXT("FrontSillXPos")),
		B(DoorXPosCm, FrontOuterYCm, DeckZCm), B(HallXPosCm, FrontInnerYCm, WindowSillZCm), true);
	const int32 FrontHead = AddCollider(Name(TEXT("FrontHead")),
		B(HallXNegCm, FrontOuterYCm, WindowHeadZCm), B(HallXPosCm, FrontInnerYCm, HallTopZCm), true);
	AddCollider(Name(TEXT("PaneXNeg")),
		B(HallXNegCm, FrontOuterYCm, WindowSillZCm), B(DoorXNegCm, FrontInnerYCm, WindowHeadZCm), false);
	AddCollider(Name(TEXT("PaneXPos")),
		B(DoorXPosCm, FrontOuterYCm, WindowSillZCm), B(HallXPosCm, FrontInnerYCm, WindowHeadZCm), false);
	AddCollider(Name(TEXT("PaneOverDoor")),
		B(DoorXNegCm, FrontOuterYCm, BoomTopZCm), B(DoorXPosCm, FrontInnerYCm, WindowHeadZCm), false);
	const int32 BoomFloor = AddCollider(Name(TEXT("BoomFloor")),
		B(DoorXNegCm, PortPlaneYCm, DeckZCm - WallCm), B(DoorXPosCm, FrontOuterYCm, DeckZCm), true);
	const int32 BoomCeiling = AddCollider(Name(TEXT("BoomCeiling")),
		B(DoorXNegCm, PortPlaneYCm, BoomTopZCm), B(DoorXPosCm, FrontOuterYCm, BoomTopZCm + WallCm), true);
	const int32 BoomWallXNeg = AddCollider(Name(TEXT("BoomWallXNeg")),
		B(DoorXNegCm - WallCm, PortPlaneYCm, DeckZCm), B(DoorXNegCm, FrontOuterYCm, BoomTopZCm), true);
	const int32 BoomWallXPos = AddCollider(Name(TEXT("BoomWallXPos")),
		B(DoorXPosCm, PortPlaneYCm, DeckZCm), B(DoorXPosCm + WallCm, FrontOuterYCm, BoomTopZCm), true);

	// The port on the channel's outboard mouth, on the port plane. A yaw of -90 takes local +X onto -Y
	// and leaves +Z alone, exactly the rig station's port (SFTestStation.cpp 140-143).
	USFDockingPortComponent* Port = CreateDefaultSubobject<USFDockingPortComponent>(FName(*FString::Printf(TEXT("Port%d"), Block)));
	Port->SetupAttachment(Root);
	Port->SetRelativeLocation(B(BoomAxisXCm, PortPlaneYCm, PortZCm));
	Port->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
	Ports.Add(Port);
	const FVector OutwardAxis(0.0, -1.0, 0.0);

	// The hall first, then the boom: the order the Station rig: line reads them in (spec 6.8). The
	// hall is taken by its clear depth, 11.8 m, because the front wall stands inside the envelope.
	const int32 Hall = AddVolume(FName(*FString::Printf(TEXT("Hall%d"), Block)),
		B(HallXNegCm, FrontInnerYCm, DeckZCm), B(HallXPosCm, HallBackYCm, HallTopZCm), TEXT("hall"), OutwardAxis);
	VolumeRecords[Hall].Faces[0].Add({SideXNeg, true});
	VolumeRecords[Hall].Faces[1].Add({SideXPos, false});
	VolumeRecords[Hall].Faces[2].Add({FrontHead, true});      // the front wall's hall-side face, -11.8
	VolumeRecords[Hall].Faces[3].Add({Back, false});
	VolumeRecords[Hall].Faces[4].Add({Base, true});
	VolumeRecords[Hall].Faces[5].Add({Roof, false});

	// The boom is exactly its channel and no wider: outside the channel is open space where a 48.9 m
	// ship stands beside the boom (rule 2, form 1). Built from the port plane, the constant the port
	// stands on, so a port and a channel that disagree show in the line (spec 6.4). Its +Y end is the
	// hall-side face of the front wall: the door through that wall belongs to the boom.
	const int32 Boom = AddVolume(FName(*FString::Printf(TEXT("Boom%d"), Block)),
		B(DoorXNegCm, PortPlaneYCm, DeckZCm), B(DoorXPosCm, FrontInnerYCm, BoomTopZCm), TEXT("channel"), OutwardAxis);
	VolumeRecords[Boom].Faces[0].Add({BoomWallXNeg, true});
	VolumeRecords[Boom].Faces[1].Add({BoomWallXPos, false});
	// Its -Y end is read from the channel colliders' OUTER ends, not from the port plane: the tightest
	// of the four, so a channel shorter than its port leaves the boom's volume sticking out (spec 6.8).
	const int32 ChannelEnds[] = { BoomFloor, BoomCeiling, BoomWallXNeg, BoomWallXPos };
	for (const int32 End : ChannelEnds)
	{
		VolumeRecords[Boom].Faces[2].Add({End, false});
	}
	VolumeRecords[Boom].Faces[3].Add({FrontHead, true});
	VolumeRecords[Boom].Faces[4].Add({BoomFloor, true});
	VolumeRecords[Boom].Faces[5].Add({BoomCeiling, false});

	// Light at the hall's centre. Its reach is no shorter than the distance to the port, because a
	// 12 m channel is darker than the rig's 2 m tunnel (spec 6.3); the hall's own far corner is nearer,
	// 10.7 m. 18.31 m, rounded up to the next whole metre: 1900 cm.
	const FVector HallCentreCm = B(HallCentreB.X, HallCentreB.Y, HallCentreB.Z);
	UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(Name(TEXT("Light")));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(HallCentreCm);
	Light->SetMobility(EComponentMobility::Movable);
	Light->IntensityUnits = ELightUnits::Candelas;
	Light->SetIntensity(static_cast<float>(LightIntensityCandelas));
	const double ToPortCm = FVector::Dist(HallCentreCm, Port->GetRelativeLocation());
	Light->AttenuationRadius = static_cast<float>(
		FMath::CeilToDouble(ToPortCm / StarForge::Units::CmPerMetre) * StarForge::Units::CmPerMetre);
	Light->SetCastShadows(false);
	Lights.Add(Light);

	// The rig station's three shapes at its size: sphere +X, cone +Y, cylinder -X. Walking in from the
	// boom is walking towards the cone, as walking out of the ship's hatch is on the rig.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	const double MarkerXCm = (HallXPosCm - HallXNegCm) * 0.5 - MarkerWallClearanceXCm;      // 660
	const double MarkerYCm = (HallBackYCm - FrontInnerYCm) * 0.5 - MarkerWallClearanceYCm;  // 500
	AddMarker(Name(TEXT("MarkerSphere")), SphereMesh.Object, HallCentreCm + FVector(MarkerXCm, 0.0, 0.0));
	AddMarker(Name(TEXT("MarkerCone")), ConeMesh.Object, HallCentreCm + FVector(0.0, MarkerYCm, 0.0));
	AddMarker(Name(TEXT("MarkerCylinder")), CylinderMesh.Object, HallCentreCm + FVector(-MarkerXCm, 0.0, 0.0));
}

int32 ASFKitStation::AddCollider(FName Name, const FVector& MinCm, const FVector& MaxCm, bool bVisible)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	const FVector CentreCm = (MinCm + MaxCm) * 0.5;
	const FVector SizeCm = MaxCm - MinCm;

	UPrimitiveComponent* Collider = nullptr;
	if (bVisible)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetRelativeScale3D(SizeCm / KitStation::CubeEdgeCm);
		if (CubeMesh.Succeeded())
		{
			Mesh->SetStaticMesh(CubeMesh.Object);
		}
		Collider = Mesh;
	}
	else
	{
		// A pane: a box with no mesh at all, which is rule 5 in its plainest form. It blocks exactly
		// as a wall does and draws nothing -- and a hall whose windows draw nothing is also the
		// cheapest answer to checkpoint 7's blind cube: the ship is visible through them.
		UBoxComponent* Pane = CreateDefaultSubobject<UBoxComponent>(Name);
		Pane->SetBoxExtent(SizeCm * 0.5);
		Pane->SetHiddenInGame(true);
		Collider = Pane;
	}

	Collider->SetupAttachment(Root);
	Collider->SetRelativeLocation(CentreCm);
	Collider->SetMobility(EComponentMobility::Movable);
	Collider->SetCollisionObjectType(ECC_WorldStatic);
	Collider->SetCollisionResponseToAllChannels(ECR_Block);

	// Query AND physics, each its own kinematic collider, for the reason the rig station's panels are
	// (SFTestStation.cpp 210-254, finding F1): a hull must meet the walls and stop at them. There is no
	// simulating body on this actor for them to be welded into.
	Collider->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	return Colliders.Add(Collider);
}

int32 ASFKitStation::AddVolume(
	FName Name, const FVector& MinCm, const FVector& MaxCm, const TCHAR* EnvelopeWord, const FVector& OutwardAxis)
{
	USFInteriorVolumeComponent* Volume = CreateDefaultSubobject<USFInteriorVolumeComponent>(Name);
	Volume->SetupAttachment(Root);

	FSFKitVolumeRecord Record;
	Record.AuthoredCentreCm = (MinCm + MaxCm) * 0.5;
	Record.AuthoredExtentCm = (MaxCm - MinCm) * 0.5;
	Record.EnvelopeWord = EnvelopeWord;
	Record.OutwardAxis = OutwardAxis;

	Volume->SetRelativeLocation(Record.AuthoredCentreCm);
	Volume->SetBoxExtent(Record.AuthoredExtentCm);

	Record.Volume = Volumes.Add(Volume);
	return VolumeRecords.Add(MoveTemp(Record));
}

UStaticMeshComponent* ASFKitStation::AddMarker(FName Name, UStaticMesh* Mesh, const FVector& LocationCm)
{
	UStaticMeshComponent* Marker = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Marker->SetupAttachment(Root);
	Marker->SetRelativeLocation(LocationCm);
	Marker->SetRelativeScale3D(FVector(KitStation::MarkerUniformScale));
	Marker->SetMobility(EComponentMobility::Movable);

	// Something to read direction by; it must not block a walk, and it must not join the release's
	// box: rule 5, a visual with collision would move the standoff in silence.
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Mesh)
	{
		Marker->SetStaticMesh(Mesh);
	}

	Markers.Add(Marker);
	return Marker;
}

void ASFKitStation::GetDockingPorts(TArray<USFDockingPortComponent*>& Out) const
{
	Out.Reset();
	for (const TObjectPtr<USFDockingPortComponent>& Port : Ports)
	{
		Out.Add(Port.Get());
	}
}

void ASFKitStation::OnDockingPortChanged(USFDockingPortComponent* Port)
{
	// Nothing is said about ports until the station has a line to say it in; that line is the
	// Station rig: line, and it replaces this body (station-kit plan, task 9).
}
