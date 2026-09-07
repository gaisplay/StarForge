#include "Ship/SFTestShip.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFThrusterArrayComponent.h"
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
	 * on its own. The project runs with auto-exposure off
	 * (r.DefaultFeature.AutoExposure=False in Config/DefaultEngine.ini), so there is
	 * nothing to compensate and intensity maps straight to screen brightness: 50000 cd
	 * blew the scene to pure white. 2000 cd reads every wall without doing that -- if
	 * it looks dim, that is auto-exposure being off, not a mistake in this number.
	 */
	constexpr double InteriorLightIntensityCandelas = 2000.0;

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

	/**
	 * Stand-in mass until the ship data asset exists, in kilograms. Large enough
	 * that a character walking on the deck cannot perturb the measurement.
	 */
	constexpr double PlaceholderMassKg = 50000.0;
}

ASFTestShip::ASFTestShip()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Hull = CreateDefaultSubobject<UBoxComponent>(TEXT("Hull"));
	SetRootComponent(Hull);
	Hull->SetBoxExtent(FVector(InteriorHalfExtentCm));
	Hull->SetMobility(EComponentMobility::Movable);

	// Physics only: this box exists to give the body mass and inertia, not to be
	// walked into. With query collision on, the occupant would be sealed inside a
	// solid cube instead of the hollow room the panels make.
	Hull->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	Hull->SetCollisionObjectType(ECC_WorldDynamic);
	Hull->SetSimulatePhysics(true);

	// A ship in space does not fall, and the rig has no other use for gravity.
	Hull->SetEnableGravity(false);

	// Stated rather than derived from the box volume, so the inertia the flight
	// maths later reads back from Chaos is reproducible instead of depending on a
	// default density. Task 4 replaces this with the ship data asset.
	Hull->SetMassOverrideInKg(NAME_None, static_cast<float>(PlaceholderMassKg), true);

	Frame = CreateDefaultSubobject<USFShipFrameComponent>(TEXT("Frame"));
	Thrusters = CreateDefaultSubobject<USFThrusterArrayComponent>(TEXT("Thrusters"));

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
	// eyes while the ship rotates -- impossible to perform. Attaching to Hull means
	// the light travels and rotates with the ship like everything else on board.
	InteriorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("InteriorLight"));
	InteriorLight->SetupAttachment(Hull);
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
	Panel->SetupAttachment(Hull);
	Panel->SetRelativeLocation(RelativeLocation);
	Panel->SetRelativeScale3D(RelativeScale);
	Panel->SetMobility(EComponentMobility::Movable);
	Panel->SetCollisionObjectType(ECC_WorldDynamic);
	Panel->SetCollisionResponseToAllChannels(ECR_Block);

	// Query only. These panels hold the occupant up; the hull box is what the
	// physics solver sees. Leaving physics collision on them risks their being
	// welded into the hull body, which would make that body shape depend on
	// component initialisation order.
	Panel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

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
	Marker->SetupAttachment(Hull);
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

	// Same profile as the kinematic rig, driven through the physics solver instead
	// of by moving the transform. Velocity is commanded every tick rather than
	// integrated from forces: the point of this task is to change one thing only --
	// where the transform comes from -- so the motion must stay the one the
	// checkpoint 1 figures were taken against.
	const FVector VelocityCms(PeakSpeedCms * FMath::Sin(Phase), 0.0, 0.0);
	Hull->SetPhysicsLinearVelocity(VelocityCms);

	LinearAccelMetersPerSecSq = (VelocityCms - PreviousVelocityCms) / DeltaSeconds / 100.0;
	PreviousVelocityCms = VelocityCms;

	// All three angular axes loaded at once so no accidental symmetry hides a bug.
	// Pitch and yaw sweep in quadrature; roll is held flat at its ceiling, which is
	// the harsher case because it never lets up.
	//
	// An angular velocity vector is (roll about X, pitch about Y, yaw about Z) --
	// not the order FRotator prints in. It is commanded in the ship own axes and
	// rotated into world space, because SetPhysicsAngularVelocityInRadians is world
	// space while the old AddActorLocalRotation was local.
	const FVector LocalRateRadPerSec(
		FMath::DegreesToRadians(PeakRollRateDegPerSec),
		FMath::DegreesToRadians(PeakPitchYawRateDegPerSec * FMath::Sin(Phase * 0.5)),
		FMath::DegreesToRadians(PeakPitchYawRateDegPerSec * FMath::Cos(Phase * 0.5)));

	const FVector WorldRateRadPerSec =
		GetActorTransform().TransformVectorNoScale(LocalRateRadPerSec);

	Hull->SetPhysicsAngularVelocityInRadians(WorldRateRadPerSec);
}
