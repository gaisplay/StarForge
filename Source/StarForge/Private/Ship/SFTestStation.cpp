#include "Ship/SFTestStation.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Ship/SFDockingPortComponent.h"
#include "Ship/SFInteriorVolumeComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "StarForgeLog.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
/**
 * Nested and named on purpose. The game module is unity-built, so every file's anonymous
 * namespace is one and the same anonymous namespace: a second CubeEdgeCm at that scope
 * would redefine the ship's, and the error would surface only after this file is
 * committed and rejoins the unity blob. The qualifier is what keeps the two hulls' figures
 * apart.
 */
namespace Station
{
	/** One engine cube is 100 cm on a side, so a mesh scale equals metres. */
	constexpr double CubeEdgeCm = 100.0;

	/** Panel thickness as a scale factor, not centimetres: 0.2 x 100 cm = 20 cm. */
	constexpr double PanelThickness = 0.2;

	/** The box is 12 x 8 x 8 m: half again as long as the ship's cabin along X. */
	constexpr double HalfXCm = 600.0;
	constexpr double HalfYCm = 400.0;
	constexpr double HalfZCm = 400.0;

	/** The hatch in the -Y wall, mirroring the ship's exactly. See SFTestShip.cpp. */
	constexpr double HatchHalfWidthCm = 100.0;
	constexpr double HatchHeightCm = 250.0;

	/**
	 * How far the airlock tunnel reaches out beyond the -Y wall, in centimetres.
	 *
	 * Two metres: long enough that a person is unmistakably inside it for several steps
	 * -- the whole point of the airlock is that the handover happens somewhere with a
	 * length, not on a line -- and short enough that a ship three metres out of position
	 * has not yet met it.
	 */
	constexpr double TunnelLengthCm = 200.0;

	/**
	 * The markers are the same three shapes as the ship's at a different size, and both
	 * halves of that carry information. The shape says which way you are facing -- sphere
	 * +X, cone +Y, cylinder -X in the hull's own axes; the size says which hull you are
	 * in. A person who has just walked out of the tunnel needs both at once, and the two
	 * rooms are otherwise the same grey box.
	 *
	 * Because the two hulls mate along Y, the ship's +Y and the station's -Y face each
	 * other: walking out of the ship's hatch is walking toward the station's cone.
	 */
	constexpr double MarkerUniformScale = 0.8;
	constexpr double MarkerOffsetXCm = 450.0;
	constexpr double MarkerOffsetYCm = 300.0;

	/**
	 * Attenuation radius in centimetres, raised from the ship's 2000 for a room half
	 * again as long. Intensity is the ship's 2000 cd unchanged: the project runs with
	 * auto-exposure off, so candelas map straight to screen brightness and a second
	 * number here would make the two rooms differently lit for no reason.
	 */
	constexpr double InteriorLightRadiusCm = 2500.0;
	constexpr double InteriorLightIntensityCandelas = 2000.0;
}
}

ASFTestStation::ASFTestStation()
{
	// Nothing to do per frame. The frame component ticks itself to sample the transform,
	// and that sample is constant here -- which is the point of a station.
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);

	Frame = CreateDefaultSubobject<USFShipFrameComponent>(TEXT("Frame"));

	const double SpanX = Station::HalfXCm / Station::CubeEdgeCm * 2.0;
	const double SpanY = Station::HalfYCm / Station::CubeEdgeCm * 2.0;
	const double SpanZ = Station::HalfZCm / Station::CubeEdgeCm * 2.0;

	AddBoxPanel(TEXT("Floor"), FVector(0.0, 0.0, -Station::HalfZCm), FVector(SpanX, SpanY, Station::PanelThickness));
	AddBoxPanel(TEXT("Ceiling"), FVector(0.0, 0.0, Station::HalfZCm), FVector(SpanX, SpanY, Station::PanelThickness));
	AddBoxPanel(TEXT("WallXPos"), FVector(Station::HalfXCm, 0.0, 0.0), FVector(Station::PanelThickness, SpanY, SpanZ));
	AddBoxPanel(TEXT("WallXNeg"), FVector(-Station::HalfXCm, 0.0, 0.0), FVector(Station::PanelThickness, SpanY, SpanZ));
	AddBoxPanel(TEXT("WallYPos"), FVector(0.0, Station::HalfYCm, 0.0), FVector(SpanX, Station::PanelThickness, SpanZ));

	// The -Y wall carries the hatch, mirroring the ship's: three panels around an opening
	// 200 across and 250 tall from the deck, with the floor panel closing everything
	// below it. Both decks sit at -390 in their own hulls, which is what makes the two
	// floors continuous once the ports are mated.
	const double DeckTopZ = -Station::HalfZCm + Station::PanelThickness * Station::CubeEdgeCm * 0.5;   // -390
	const double HatchTopZ = DeckTopZ + Station::HatchHeightCm;                      // -140
	const double JambWidthCm = Station::HalfXCm - Station::HatchHalfWidthCm;                  //  500
	const double JambCentreXCm = (Station::HalfXCm + Station::HatchHalfWidthCm) * 0.5;        //  350
	const double LintelHeightCm = Station::HalfZCm - HatchTopZ;                      //  540
	const double LintelCentreZCm = (HatchTopZ + Station::HalfZCm) * 0.5;             //  130

	AddBoxPanel(TEXT("WallYNegJambXNeg"), FVector(-JambCentreXCm, -Station::HalfYCm, 0.0),
		FVector(JambWidthCm / Station::CubeEdgeCm, Station::PanelThickness, SpanZ));
	AddBoxPanel(TEXT("WallYNegJambXPos"), FVector(JambCentreXCm, -Station::HalfYCm, 0.0),
		FVector(JambWidthCm / Station::CubeEdgeCm, Station::PanelThickness, SpanZ));
	AddBoxPanel(TEXT("WallYNegLintel"), FVector(0.0, -Station::HalfYCm, LintelCentreZCm),
		FVector(Station::HatchHalfWidthCm * 2.0 / Station::CubeEdgeCm, Station::PanelThickness, LintelHeightCm / Station::CubeEdgeCm));

	// The airlock tunnel: a 200 x 250 corridor reaching two metres out along -Y, with a
	// floor flush with the station's and no doors at either end. It belongs to the port
	// in the sense spec 8 means -- it is the port's approach corridor and moves with the
	// station -- but its panels are attached to the root, so their transforms are written
	// in the same station centimetres as every other panel. A socket carrying walls is a
	// socket that cannot be moved without rebuilding the corridor.
	const double TunnelCentreYCm = -Station::HalfYCm - Station::TunnelLengthCm * 0.5;         // -500
	const double TunnelSpanY = Station::TunnelLengthCm / Station::CubeEdgeCm;                 //  2.0
	const double TunnelSpanX = Station::HatchHalfWidthCm * 2.0 / Station::CubeEdgeCm;         //  2.0
	const double TunnelSpanZ = Station::HatchHeightCm / Station::CubeEdgeCm;                  //  2.5
	const double TunnelPortZ = DeckTopZ + Station::HatchHeightCm * 0.5;              // -265

	AddBoxPanel(TEXT("TunnelFloor"), FVector(0.0, TunnelCentreYCm, -Station::HalfZCm),
		FVector(TunnelSpanX, TunnelSpanY, Station::PanelThickness));
	AddBoxPanel(TEXT("TunnelCeiling"), FVector(0.0, TunnelCentreYCm, HatchTopZ + Station::PanelThickness * Station::CubeEdgeCm * 0.5),
		FVector(TunnelSpanX, TunnelSpanY, Station::PanelThickness));
	AddBoxPanel(TEXT("TunnelWallXNeg"),
		FVector(-Station::HatchHalfWidthCm - Station::PanelThickness * Station::CubeEdgeCm * 0.5, TunnelCentreYCm, TunnelPortZ),
		FVector(Station::PanelThickness, TunnelSpanY, TunnelSpanZ));
	AddBoxPanel(TEXT("TunnelWallXPos"),
		FVector(Station::HatchHalfWidthCm + Station::PanelThickness * Station::CubeEdgeCm * 0.5, TunnelCentreYCm, TunnelPortZ),
		FVector(Station::PanelThickness, TunnelSpanY, TunnelSpanZ));

	// At the outboard mouth of the tunnel, facing out along -Y. A yaw of -90 takes local
	// +X onto -Y and leaves +Z alone, so the station's deck up and the ship's agree when
	// the two ports mate.
	DockingPort = CreateDefaultSubobject<USFDockingPortComponent>(TEXT("DockingPort"));
	DockingPort->SetupAttachment(Root);
	DockingPort->SetRelativeLocation(FVector(0.0, -Station::HalfYCm - Station::TunnelLengthCm, TunnelPortZ));
	DockingPort->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));

	// The box plus the tunnel: Y from -600 (the tunnel's outboard mouth) to +400, so the
	// centre is at -100 and the half-extent 500. X and Z reach the inner faces of the
	// walls. The volume is wider than the tunnel along X and Z, which costs nothing --
	// the extra is inside solid panels, where a pawn cannot be.
	//
	// It does not touch the ship's. Docked, the ship's cabin ends at station-local -610
	// and this begins at -600: ten centimetres of wall, deliberately unclaimed.
	const double InteriorHalfYCm = (Station::HalfYCm + Station::TunnelLengthCm) * 0.5;        // 500
	const double InteriorCentreYCm = Station::HalfYCm - InteriorHalfYCm;             // -100

	Interior = CreateDefaultSubobject<USFInteriorVolumeComponent>(TEXT("Interior"));
	Interior->SetupAttachment(Root);
	Interior->SetRelativeLocation(FVector(0.0, InteriorCentreYCm, 0.0));
	Interior->SetBoxExtent(FVector(
		Station::HalfXCm - Station::PanelThickness * Station::CubeEdgeCm * 0.5,
		InteriorHalfYCm,
		Station::HalfZCm - Station::PanelThickness * Station::CubeEdgeCm * 0.5));

	InteriorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("InteriorLight"));
	InteriorLight->SetupAttachment(Root);
	InteriorLight->SetRelativeLocation(FVector::ZeroVector);
	InteriorLight->SetMobility(EComponentMobility::Movable);
	InteriorLight->IntensityUnits = ELightUnits::Candelas;
	InteriorLight->SetIntensity(static_cast<float>(Station::InteriorLightIntensityCandelas));
	InteriorLight->AttenuationRadius = static_cast<float>(Station::InteriorLightRadiusCm);
	InteriorLight->SetCastShadows(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	AddOrientationMarker(TEXT("MarkerSphere"), SphereMesh.Object,
		FVector(Station::MarkerOffsetXCm, 0.0, 0.0), Station::MarkerUniformScale);
	AddOrientationMarker(TEXT("MarkerCone"), ConeMesh.Object,
		FVector(0.0, Station::MarkerOffsetYCm, 0.0), Station::MarkerUniformScale);
	AddOrientationMarker(TEXT("MarkerCylinder"), CylinderMesh.Object,
		FVector(-Station::MarkerOffsetXCm, 0.0, 0.0), Station::MarkerUniformScale);
}

UStaticMeshComponent* ASFTestStation::AddBoxPanel(
	FName Name, const FVector& RelativeLocation, const FVector& RelativeScale)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	UStaticMeshComponent* Panel = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Panel->SetupAttachment(Root);
	Panel->SetRelativeLocation(RelativeLocation);
	Panel->SetRelativeScale3D(RelativeScale);
	Panel->SetMobility(EComponentMobility::Movable);
	Panel->SetCollisionObjectType(ECC_WorldStatic);
	Panel->SetCollisionResponseToAllChannels(ECR_Block);

	// Query only, as on the ship. These panels hold a person up and nothing else; there
	// is no physics body anywhere on this actor for them to be welded into.
	Panel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	if (CubeMesh.Succeeded())
	{
		Panel->SetStaticMesh(CubeMesh.Object);
	}

	Panels.Add(Panel);
	return Panel;
}

UStaticMeshComponent* ASFTestStation::AddOrientationMarker(
	FName Name, UStaticMesh* Mesh, const FVector& RelativeLocation, double UniformScale)
{
	UStaticMeshComponent* Marker = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Marker->SetupAttachment(Root);
	Marker->SetRelativeLocation(RelativeLocation);
	Marker->SetRelativeScale3D(FVector(UniformScale));
	Marker->SetMobility(EComponentMobility::Movable);

	// Purely something to read direction by; it must not be able to block a walk.
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (Mesh)
	{
		Marker->SetStaticMesh(Mesh);
	}

	OrientationMarkers.Add(Marker);
	return Marker;
}

void ASFTestStation::BeginPlay()
{
	Super::BeginPlay();

	const FVector Location = GetActorLocation();
	UE_LOG(LogStarForge, Display,
		TEXT("Station at world (%.0f %.0f %.0f) cm: %.0f x %.0f x %.0f m box, hatch in the -Y wall, ")
		TEXT("%.0f cm tunnel, frame %s, no physics body."),
		Location.X, Location.Y, Location.Z,
		Station::HalfXCm * 2.0 / Station::CubeEdgeCm, Station::HalfYCm * 2.0 / Station::CubeEdgeCm, Station::HalfZCm * 2.0 / Station::CubeEdgeCm,
		Station::TunnelLengthCm,
		Frame ? TEXT("present") : TEXT("MISSING"));
}
