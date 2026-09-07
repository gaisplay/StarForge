#include "Ship/SFTestShip.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FlightController.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFShipDataAsset.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFThrusterArrayComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/**
	 * Which way the ship is driven. On, it repeats the scripted checkpoint profile
	 * with commanded velocities -- the motion the drift figures were taken against.
	 * Off, it flies on thrust from the intent below.
	 *
	 * Defaults on, so the measurement rig stays the measurement rig until somebody
	 * deliberately asks it to be something else.
	 */
	TAutoConsoleVariable<int32> CVarShipProfile(
		TEXT("sf.ShipProfile"), 1,
		TEXT("1: fly the scripted checkpoint profile. 0: fly on thrust from sf.Intent.*"),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarIntentX(TEXT("sf.Intent.X"), 0.0f,
		TEXT("Linear intent along the ship forward axis, -1 to 1."), ECVF_Default);
	TAutoConsoleVariable<float> CVarIntentY(TEXT("sf.Intent.Y"), 0.0f,
		TEXT("Linear intent along the ship right axis, -1 to 1."), ECVF_Default);
	TAutoConsoleVariable<float> CVarIntentZ(TEXT("sf.Intent.Z"), 0.0f,
		TEXT("Linear intent along the ship up axis, -1 to 1."), ECVF_Default);

	// Named for what they do, stored in angular velocity order: X roll, Y pitch,
	// Z yaw. The names and the order agree here so nobody has to remember which.
	TAutoConsoleVariable<float> CVarIntentRoll(TEXT("sf.Intent.Roll"), 0.0f,
		TEXT("Roll intent about the ship X axis, -1 to 1."), ECVF_Default);
	TAutoConsoleVariable<float> CVarIntentPitch(TEXT("sf.Intent.Pitch"), 0.0f,
		TEXT("Pitch intent about the ship Y axis, -1 to 1."), ECVF_Default);
	TAutoConsoleVariable<float> CVarIntentYaw(TEXT("sf.Intent.Yaw"), 0.0f,
		TEXT("Yaw intent about the ship Z axis, -1 to 1."), ECVF_Default);

	TAutoConsoleVariable<int32> CVarIntentAssist(TEXT("sf.Intent.Assist"), 1,
		TEXT("1: the stick asks for a speed. 0: it asks for thrust."), ECVF_Default);

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

	if (DeltaSeconds <= UE_SMALL_NUMBER || !Hull)
	{
		return;
	}

	ElapsedSeconds += DeltaSeconds;

	if (CVarShipProfile.GetValueOnGameThread() != 0)
	{
		TickScriptedProfile();
	}
	else
	{
		TickFlight(DeltaSeconds);
	}

	// Measured from what the body actually did, not from what was commanded. In the
	// scripted mode the two agree; under thrust they do not, and the grav-plate
	// readout has to show the ship real acceleration or it is showing nothing.
	const FVector VelocityCms = Hull->GetPhysicsLinearVelocity();
	LinearAccelMetersPerSecSq = (VelocityCms - PreviousVelocityCms) / DeltaSeconds / 100.0;
	PreviousVelocityCms = VelocityCms;
}

void ASFTestShip::TickScriptedProfile()
{
	const double Phase = 2.0 * UE_DOUBLE_PI * ElapsedSeconds / ProfilePeriodSeconds;

	const FVector VelocityCms(PeakSpeedCms * FMath::Sin(Phase), 0.0, 0.0);
	Hull->SetPhysicsLinearVelocity(VelocityCms);

	const FVector LocalRateRadPerSec(
		FMath::DegreesToRadians(PeakRollRateDegPerSec),
		FMath::DegreesToRadians(PeakPitchYawRateDegPerSec * FMath::Sin(Phase * 0.5)),
		FMath::DegreesToRadians(PeakPitchYawRateDegPerSec * FMath::Cos(Phase * 0.5)));

	Hull->SetPhysicsAngularVelocityInRadians(
		GetActorTransform().TransformVectorNoScale(LocalRateRadPerSec));
}

void ASFTestShip::TickFlight(double DeltaSeconds)
{
	if (!Thrusters || !Thrusters->IsReady() || !Thrusters->ShipData)
	{
		return;
	}

	LastIntent.LinearAxis = FVector(
		CVarIntentX.GetValueOnGameThread(),
		CVarIntentY.GetValueOnGameThread(),
		CVarIntentZ.GetValueOnGameThread());

	LastIntent.AngularAxis = FVector(
		CVarIntentRoll.GetValueOnGameThread(),
		CVarIntentPitch.GetValueOnGameThread(),
		CVarIntentYaw.GetValueOnGameThread());

	LastIntent.bFlightAssistEnabled = CVarIntentAssist.GetValueOnGameThread() != 0;

	const StarForge::FWrench Demand = StarForge::ComputeDemandedWrench(
		LastIntent,
		Thrusters->ReadState(),
		Thrusters->GetMassProperties(),
		Thrusters->ShipData->BuildLimits(),
		Thrusters->GetArrayCapability());

	Thrusters->ApplyDemand(Demand, DeltaSeconds);
}
