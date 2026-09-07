#include "Ship/SFTestShip.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** One engine cube is 100 cm on a side, so scale equals metres. */
	constexpr double CubeEdgeCm = 100.0;
	/** Panel thickness as a mesh scale factor, not centimetres: 0.2 x 100 cm = 20 cm. */
	constexpr double PanelThickness = 0.2;

	/**
	 * Height above the floor plate at which an occupant is placed, in centimetres.
	 * Must clear a standard character capsule half-height (88 cm) so the pawn starts
	 * standing on the plate rather than embedded in it.
	 */
	constexpr double SpawnClearanceCm = 100.0;

	/**
	 * Attenuation radius for the interior light, in centimetres. The room is 8 m
	 * (800 cm) across; 2000 cm gives the light enough reach to fall off gently across
	 * the whole interior instead of clipping hard at the far wall.
	 */
	constexpr double InteriorLightRadiusCm = 2000.0;

	/**
	 * Interior light intensity in candelas. The room is a sealed grey box with no
	 * other light of any kind, so this single source has to read every wall clearly
	 * on its own; 50000 cd at the room's 20 m attenuation radius is bright enough for
	 * that without blowing out the white hull panels.
	 */
	constexpr double InteriorLightIntensityCandelas = 50000.0;

	/**
	 * Each basic-shape marker is the engine's 100 cm default asset scaled down to
	 * roughly half a metre, so it reads as a small orientation cue rather than
	 * another wall.
	 */
	constexpr double MarkerUniformScale = 0.5;

	/**
	 * How far a marker sits from the room's centre, in centimetres. Clear of the
	 * walls (100 cm short of the 400 cm half-extent) and clear of the spawn point,
	 * which sits on the pivot's X/Y column at floor height.
	 */
	constexpr double MarkerOffsetCm = 300.0;
}

ASFTestShip::ASFTestShip()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Frame = CreateDefaultSubobject<USFShipFrameComponent>(TEXT("Frame"));

	const double Span = InteriorHalfExtentCm / CubeEdgeCm * 2.0;
	const double Half = InteriorHalfExtentCm;

	AddBoxPanel(TEXT("Floor"), FVector(0.0, 0.0, -Half), FVector(Span, Span, PanelThickness));
	AddBoxPanel(TEXT("Ceiling"), FVector(0.0, 0.0, Half), FVector(Span, Span, PanelThickness));
	AddBoxPanel(TEXT("WallXPos"), FVector(Half, 0.0, 0.0), FVector(PanelThickness, Span, Span));
	AddBoxPanel(TEXT("WallXNeg"), FVector(-Half, 0.0, 0.0), FVector(PanelThickness, Span, Span));
	AddBoxPanel(TEXT("WallYPos"), FVector(0.0, Half, 0.0), FVector(Span, PanelThickness, Span));
	AddBoxPanel(TEXT("WallYNeg"), FVector(0.0, -Half, 0.0), FVector(Span, PanelThickness, Span));

	// The room had no light of any kind: a black screen makes the one check that
	// matters most after drift -- whether the interior holds still in front of the
	// eyes while the ship rotates -- impossible to perform. Attaching to Root means
	// the light travels and rotates with the ship like everything else on board.
	InteriorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("InteriorLight"));
	InteriorLight->SetupAttachment(Root);
	InteriorLight->SetRelativeLocation(FVector::ZeroVector);
	InteriorLight->SetMobility(EComponentMobility::Movable);
	InteriorLight->IntensityUnits = ELightUnits::Candelas;
	InteriorLight->SetIntensity(static_cast<float>(InteriorLightIntensityCandelas));
	InteriorLight->AttenuationRadius = static_cast<float>(InteriorLightRadiusCm);
	// Shadows from a single centred point light in a sealed 8 m box mostly just
	// self-shadow the corners nearest it and add nothing to the one thing this rig
	// needs the light for -- reading the room's orientation at a glance -- so they
	// are left off to keep the tick budget clean for the measurement itself.
	InteriorLight->SetCastShadows(false);

	// Six identical grey walls give the eye nothing to judge rotation against.
	// Distinct silhouettes are used instead of colours: a colour needs a material
	// parameter name, and a wrong guess there fails silently, which is exactly the
	// class of defect this branch exists to remove.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	AddOrientationMarker(TEXT("MarkerSphere"), SphereMesh.Object,
		FVector(MarkerOffsetCm, 0.0, 0.0), MarkerUniformScale);
	AddOrientationMarker(TEXT("MarkerCone"), ConeMesh.Object,
		FVector(0.0, MarkerOffsetCm, 0.0), MarkerUniformScale);
	AddOrientationMarker(TEXT("MarkerCylinder"), CylinderMesh.Object,
		FVector(-MarkerOffsetCm, 0.0, 0.0), MarkerUniformScale);
}

UStaticMeshComponent* ASFTestShip::AddBoxPanel(FName Name, const FVector& RelativeLocation, const FVector& RelativeScale)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	UStaticMeshComponent* Panel = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Panel->SetupAttachment(Root);
	Panel->SetRelativeLocation(RelativeLocation);
	Panel->SetRelativeScale3D(RelativeScale);
	Panel->SetMobility(EComponentMobility::Movable);
	Panel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Panel->SetCollisionObjectType(ECC_WorldDynamic);
	Panel->SetCollisionResponseToAllChannels(ECR_Block);

	if (CubeMesh.Succeeded())
	{
		Panel->SetStaticMesh(CubeMesh.Object);
	}

	Panels.Add(Panel);
	return Panel;
}

UStaticMeshComponent* ASFTestShip::AddOrientationMarker(
	FName Name, UStaticMesh* Mesh, const FVector& RelativeLocation, double UniformScale)
{
	UStaticMeshComponent* Marker = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Marker->SetupAttachment(Root);
	Marker->SetRelativeLocation(RelativeLocation);
	Marker->SetRelativeScale3D(FVector(UniformScale));
	Marker->SetMobility(EComponentMobility::Movable);

	// Purely a visual orientation cue -- it must not be able to block the walk test
	// or perturb the drift figure.
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (Mesh)
	{
		Marker->SetStaticMesh(Mesh);
	}

	OrientationMarkers.Add(Marker);
	return Marker;
}

FVector ASFTestShip::GetInteriorSpawnWorldLocation() const
{
	// Just above the floor plate, on the pivot.
	const FVector Local(0.0, 0.0, -InteriorHalfExtentCm + SpawnClearanceCm);
	return GetActorTransform().TransformPosition(Local);
}

void ASFTestShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (DeltaSeconds <= UE_SMALL_NUMBER)
	{
		return;
	}

	ElapsedSeconds += DeltaSeconds;

	const double Phase = 2.0 * UE_DOUBLE_PI * ElapsedSeconds / ProfilePeriodSeconds;

	// A full-amplitude sinusoid along X: speed peaks at the design ceiling and
	// acceleration reverses hardest at the zero crossings, which is where a
	// passenger is most likely to be thrown or left behind.
	const FVector VelocityCms(PeakSpeedCms * FMath::Sin(Phase), 0.0, 0.0);
	AddActorWorldOffset(VelocityCms * DeltaSeconds, /*bSweep=*/false);

	LinearAccelMetersPerSecSq = (VelocityCms - PreviousVelocityCms) / DeltaSeconds / 100.0;
	PreviousVelocityCms = VelocityCms;

	// All three angular axes loaded at once so no accidental symmetry hides a
	// bug. Pitch and yaw sweep in quadrature; roll is held flat at its ceiling,
	// which is the harsher case because it never lets up.
	const FRotator RatePerSecond(
		PeakPitchYawRateDegPerSec * FMath::Sin(Phase * 0.5),
		PeakPitchYawRateDegPerSec * FMath::Cos(Phase * 0.5),
		PeakRollRateDegPerSec);

	AddActorLocalRotation((RatePerSecond * DeltaSeconds).Quaternion());
}
