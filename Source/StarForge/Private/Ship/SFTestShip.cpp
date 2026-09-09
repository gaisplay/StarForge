#include "Ship/SFTestShip.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "FlightController.h"
#include "HAL/IConsoleManager.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Ship/SFConsoleOperatorComponent.h"
#include "Ship/SFPilotStationComponent.h"
#include "Ship/SFShipDataAsset.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFTargetDrone.h"
#include "Ship/SFThrusterArrayComponent.h"
#include "StarForgeLog.h"
#include "Units.h"
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

	// The sf.Intent.* variables moved to SFConsoleOperatorComponent.cpp, which is the
	// only thing that reads them now that the flight loop lives in the post. Declaring
	// them here and resolving them there by name meant a failed lookup returned zero,
	// and a zeroed axis is a centred stick, which with assist on orders a stop.

	/**
	 * Writes one flight readout line a second to the log.
	 *
	 * The HUD shows the same numbers, but a number on a screen cannot be checked
	 * afterwards and cannot be compared against a run from last week. Off by default
	 * because a real measurement wants a quiet log.
	 */
	TAutoConsoleVariable<int32> CVarFlightLog(TEXT("sf.FlightLog"), 0,
		TEXT("1: log speed, angular rate, demanded against achieved wrench and throttle saturation once a second."),
		ECVF_Default);

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
	 * Mass the hull carries before a ship data asset is assigned, in kilograms.
	 * Large enough that a character walking on the deck cannot perturb the drift
	 * measurement, which is the one thing this rig still has to do without an asset.
	 *
	 * Once an asset is assigned, USFThrusterArrayComponent::Rebuild overrides this
	 * with the asset's MassKg at BeginPlay -- so editing this number only moves the
	 * assetless rig, and the flight model is balanced in the asset as spec 9 says.
	 */
	constexpr double DefaultHullMassKg = 50000.0;
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

	// Nor does it meet air. Chaos defaults a body to 0.01 linear damping, which is a
	// drag term: it cost the ship the last metre per second of its 200 m/s ceiling
	// and left the controller holding 99.5 kN forever to stand still against it.
	// Both are set explicitly rather than left at their defaults, so a change to the
	// project physics settings cannot quietly put the atmosphere back.
	Hull->SetLinearDamping(0.0f);
	Hull->SetAngularDamping(0.0f);

	// Stated rather than derived from the box volume, so the inertia the flight
	// maths later reads back from Chaos is reproducible instead of depending on a
	// default density. The thruster array replaces this with the asset's mass at
	// BeginPlay whenever one is assigned.
	Hull->SetMassOverrideInKg(NAME_None, static_cast<float>(DefaultHullMassKg), true);

	Frame = CreateDefaultSubobject<USFShipFrameComponent>(TEXT("Frame"));
	Thrusters = CreateDefaultSubobject<USFThrusterArrayComponent>(TEXT("Thrusters"));

	PilotStation = CreateDefaultSubobject<USFPilotStationComponent>(TEXT("PilotStation"));

	// Exactly the height a standing occupant already occupies, taken from the same two
	// constants as GetInteriorSpawnWorldLocation so the seat and the spawn point cannot
	// drift apart -- and deliberately not a hand-picked "looks about right" figure.
	//
	// Seating moves the character there in one teleport, so a seat above standing
	// height leaves no walkable floor under his capsule: FindFloor sweeps only
	// max(2 * MAX_FLOOR_DIST, MaxStepHeight + MAX_FLOOR_DIST), about 47 cm on the
	// default capsule, and finding nothing PhysWalking drops him into MOVE_Falling.
	// A falling character clears its movement base, and with no base the engine stops
	// calling UpdateBasedMovement -- the one override that carries an occupant with
	// the ship. The ship then leaves without him. The earlier -2.5 m sat 52 cm too
	// high and hid at rest, because standing still he simply falls the gap and lands;
	// only at speed does the ship depart in the frames he spends off the deck.
	//
	// The conversion to the property's metres goes through StarForge::Units like every
	// other crossing of that boundary; the two constants are the ship's own centimetres.
	PilotStation->SeatLocationMetres = FVector(
		0.0, 0.0, StarForge::Units::CmToMetres(-InteriorHalfExtentCm + SpawnClearanceCm));

	ConsoleOperator = CreateDefaultSubobject<USFConsoleOperatorComponent>(TEXT("ConsoleOperator"));

	// The ship data asset is wired here rather than assigned on a placed actor
	// because there is no placed actor to assign it on: L_FrameSpike is deliberately
	// empty and ASFSpikeGameMode::StartPlay spawns this rig from StaticClass(). With
	// nothing in the level, the property could never be authored, so the thruster
	// array failed Rebuild() with "no ship data asset" every run and the ship could
	// not fly at all. Resolving it in the constructor also means creating the asset
	// costs a restart rather than a recompile.
	//
	// Guarded by Succeeded() to match the mesh finders below: the asset does not
	// exist yet, and an unresolved finder simply leaves ShipData null, which the
	// array already reports in red on the readout instead of crashing.
	static ConstructorHelpers::FObjectFinder<USFShipDataAsset> ShipDataAsset(
		TEXT("/Game/Ships/DA_TestShip.DA_TestShip"));

	if (ShipDataAsset.Succeeded())
	{
		Thrusters->ShipData = ShipDataAsset.Object;
	}

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

void ASFTestShip::BeginPlay()
{
	Super::BeginPlay();

	// Said once at startup rather than checked by hand, because a hand check is a
	// check somebody forgets. An asynchronous solver with interpolation is the one
	// mechanism that can hand GetActorTransform a smoothed value instead of the one
	// physics computed, and every drift figure this rig has ever produced rests on
	// that not happening.
	const IConsoleVariable* AsyncMode =
		IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.Solver.AsyncPhysicsBlockMode"));

	UE_LOG(LogStarForge, Display,
		TEXT("Physics: async block mode %s, substepping %s. Drift figures are only "
			 "comparable across runs where these match."),
		AsyncMode ? *FString::FromInt(AsyncMode->GetInt()) : TEXT("(cvar absent)"),
		UPhysicsSettings::Get()->bSubstepping ? TEXT("on") : TEXT("off"));

	// The rig assembles itself, so the thing the turret tracks is spawned here rather
	// than placed: L_FrameSpike is empty by design. Anchored to this ship's position so
	// it stays in range whichever way the ship is driven, and in world axes so it does
	// not turn with the ship, which is what makes a rolling hull a test of the aim.
	//
	// Deferred, because the drone's BeginPlay logs its anchor and takes its first
	// position from it, and a spawn that finishes before SetAnchor would log "the world
	// origin" for a drone that is about to be anchored to this ship.
	FTransform DroneSpawn = GetActorTransform();
	DroneSpawn.SetScale3D(FVector::OneVector);
	TargetDrone = GetWorld()->SpawnActorDeferred<ASFTargetDrone>(
		ASFTargetDrone::StaticClass(), DroneSpawn, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (TargetDrone)
	{
		TargetDrone->SetAnchor(this);
		TargetDrone->FinishSpawning(DroneSpawn);
	}
	else
	{
		UE_LOG(LogStarForge, Error, TEXT("Target drone failed to spawn; the turret will have nothing to track."));
	}
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

	// In both modes, not only under thrust. In the scripted one the line shows a post
	// nobody is commanding while the ship still moves, which is worth seeing: it is
	// the difference between "the loop is quiet" and "the loop is not running".
	if (CVarFlightLog.GetValueOnGameThread() != 0)
	{
		LogFlightState(DeltaSeconds);
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

void ASFTestShip::LogFlightState(double DeltaSeconds)
{
	SecondsSinceFlightLog += DeltaSeconds;
	if (SecondsSinceFlightLog < 1.0)
	{
		return;
	}
	SecondsSinceFlightLog = 0.0;

	// The log is now written in both modes, so it can be reached before the array has
	// built -- which the flight branch used to make impossible. Said out loud rather
	// than skipped: a flight log that simply stops printing reads like a quiet ship,
	// and the whole point of writing these numbers down is that a silent instrument
	// must never be mistakable for a calm one.
	if (!Thrusters || !Thrusters->IsReady())
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Flight: thruster array NOT built -- no flight numbers to report."));
		return;
	}

	// Read from the post rather than kept here. The ship no longer runs the loop and
	// so no longer knows what was asked of it; the station that does is the only
	// honest source for this line.
	StarForge::FFlightIntent Intent;
	FString FlyingBy = TEXT("no pilot station");
	if (PilotStation)
	{
		Intent = PilotStation->GetLastIntent();
		FlyingBy = PilotStation->GetOperatorDisplayName();

		// Occupied and declining is not occupied and at rest, and the zeroed intent
		// below cannot tell them apart -- it is identical in both. Said in the name,
		// because this line is the one that gets compared against a run from last week.
		if (PilotStation->IsOccupied() && !PilotStation->IsOperatorFlying())
		{
			FlyingBy += TEXT(" (standing down)");
		}
	}
	else
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Flight: no pilot station -- the intent below is nobody's."));
	}

	const StarForge::FRigidBodyState State = Thrusters->ReadState();
	const StarForge::FWrench Demand = Thrusters->GetLastDemand();
	const StarForge::FWrench Achieved = Thrusters->GetLastAchieved();

	// Two counts, not one. The commanded array is where Solve's clamp lands, so it
	// shows saturation the moment it starts. The spooled array is where the thrusters
	// actually are, so the gap between the two IS the spool-up -- the thing
	// ResponseTime does and that nothing has ever observed doing it.
	int32 Commanded = 0;
	for (double Throttle : Thrusters->GetTargetThrottles())
	{
		if (Throttle > 0.999)
		{
			++Commanded;
		}
	}

	int32 Spooled = 0;
	for (double Throttle : Thrusters->GetLastThrottles())
	{
		if (Throttle > 0.999)
		{
			++Spooled;
		}
	}

	// Angular velocity is (X roll, Y pitch, Z yaw), and it is printed in that order.
	// Naming the order in the line matters: a ship that rolls when asked to pitch is
	// what confusing it produces, and this log is where that would show.
	UE_LOG(LogStarForge, Log,
		TEXT("Flight: flown by %s | intent lin (%.2f %.2f %.2f) ang (%.2f %.2f %.2f) assist %d | ")
		TEXT("speed %.1f m/s, ang roll/pitch/yaw (%.1f %.1f %.1f) deg/s | ")
		TEXT("force want (%.0f %.0f %.0f) got (%.0f %.0f %.0f) N | ")
		TEXT("torque want (%.0f %.0f %.0f) got (%.0f %.0f %.0f) Nm | ")
		TEXT("throttles %d of %d commanded, %d spooled"),
		*FlyingBy,
		Intent.LinearAxis.X, Intent.LinearAxis.Y, Intent.LinearAxis.Z,
		Intent.AngularAxis.X, Intent.AngularAxis.Y, Intent.AngularAxis.Z,
		Intent.bFlightAssistEnabled ? 1 : 0,
		State.LinearVelocity.Size(),
		FMath::RadiansToDegrees(State.AngularVelocity.X),
		FMath::RadiansToDegrees(State.AngularVelocity.Y),
		FMath::RadiansToDegrees(State.AngularVelocity.Z),
		Demand.Force.X, Demand.Force.Y, Demand.Force.Z,
		Achieved.Force.X, Achieved.Force.Y, Achieved.Force.Z,
		Demand.Torque.X, Demand.Torque.Y, Demand.Torque.Z,
		Achieved.Torque.X, Achieved.Torque.Y, Achieved.Torque.Z,
		Commanded, Thrusters->GetTargetThrottles().Num(), Spooled);
}
