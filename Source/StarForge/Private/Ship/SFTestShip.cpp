#include "Ship/SFTestShip.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "FlightController.h"
#include "HAL/IConsoleManager.h"
#include "PBDRigidsSolver.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Ship/SFAutomationOperatorComponent.h"
#include "Ship/SFConsoleOperatorComponent.h"
#include "Ship/SFDamageableComponent.h"
#include "Ship/SFDockingComponent.h"
#include "Ship/SFDockingPortComponent.h"
#include "Ship/SFGunnerStationComponent.h"
#include "Ship/SFInteriorVolumeComponent.h"
#include "Ship/SFPilotStationComponent.h"
#include "Ship/SFPursuitOperatorComponent.h"
#include "Ship/SFShipDataAsset.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFTargetDrone.h"
#include "Ship/SFThrusterArrayComponent.h"
#include "Ship/SFTurretComponent.h"
#include "Ship/SFWeaponComponent.h"
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
	 * The hatch in the +Y wall, in centimetres. Two hundred across and two hundred and
	 * fifty tall from the deck: wide enough for a capsule of radius 34 to walk through
	 * without brushing a jamb, and tall enough that a standing capsule (88 cm
	 * half-height, so 176 overall) has head-room instead of reading as a crawl space.
	 */
	constexpr double HatchHalfWidthCm = 100.0;
	constexpr double HatchHeightCm = 250.0;

	/**
	 * Height above the floor plate at which an occupant is placed, in centimetres.
	 * Must clear a standard character capsule half-height (88 cm) so the pawn starts
	 * standing on the plate rather than embedded in it.
	 */
	constexpr double SpawnClearanceCm = 100.0;

	/** How far aft of the pilot's seat the gunner sits, in centimetres. See the constructor. */
	constexpr double GunnerSeatAftCm = 150.0;

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

	/** The barrel is a one-metre cone: the engine cone is 100 cm tall, so Z scale is length in metres. */
	constexpr double BarrelLengthCm = 100.0;

	/** Cone base as a mesh scale: 0.3 x 100 cm = 30 cm across, thin enough to read as a barrel. */
	constexpr double BarrelThicknessScale = 0.3;

	/** Between the barrel's highest reach and the ceiling's inner face, in centimetres. */
	constexpr double BarrelCeilingClearanceCm = 10.0;

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

	// Blocks another hull, and nothing else changes. Spec 5 makes the ship a real physical body
	// "so that collisions and hits work normally", and the shape component's default profile,
	// OverlapAllDynamic, left two hulls passing through each other: the first combat run flew a
	// coasting pirate straight through the player's cabin, where its interior volume claimed the
	// man and the carry lost him. Blocking keeps a hull out of the cabin's middle, not off its walls:
	// contact still presses one hull some centimetres into the other (see the CCD note below), and
	// the man is protected by the handover guard, not by this line.
	//
	// ECC_WorldDynamic, which is what a hull is, and ECC_WorldStatic, which is what a station's
	// panels are. The second was added on 2026-09-22 for finding F1 of checkpoint 8, and until
	// then this line blocked hulls alone on the argument that the station's panels were query
	// only and it had no physics body, so a lone ship met nothing it did not meet before. It
	// met the inside of the station. The owner flew into one by hand at under five metres a
	// second, the station's interior volume covered the pilot in his seat, the handover guard
	// let him through once the two carriers were within a metre a second of each other, and
	// he was left in space when his ship flew on without him. SFTestStation.cpp carries the
	// account; with its panels now query-and-physics and this response in place, a hull stops
	// at a station's wall and no station volume can reach into a cabin.
	//
	// Still nothing else: the drone has no collision, a projectile is a query, the interior
	// volumes are query-only and answer pawns alone, and the character's capsule is ECC_Pawn,
	// which this box still only overlaps.
	Hull->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Hull->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

	// Continuous detection, with its thresholds set in Config/DefaultEngine.ini under
	// [ConsoleVariables]: a step is swept only when the pair moves more than 8 cm in it, and a swept
	// step is rolled back to at most 4 cm. That is not a bound on penetration -- ordinary contact
	// can still build it over several unswept steps -- and the ini says what was measured: first
	// touch at 854.9 cm between centres for the combat rig's pitched approach, and about 10 cm of
	// overlap at 60 fps and 37 cm at 400, against about 122 cm at 60 fps with the engine's own
	// thresholds. The box's extent is not the place to buy more: it is what Chaos derives the inertia
	// from, and 533333 kg m^2 is a recorded figure.
	//
	// The margin that matters is 20 cm: a panel's outer face is at +/-410 and the cabin's volume at
	// +/-390, so a hull pressed that far into this one brings its interior volume onto a man at the
	// wall. What protects him is not this flag but the handover guard in
	// USFInteriorVolumeComponent::TryHandover, which refuses to bind him to a carrier moving faster
	// than 1.0 m/s relative to his own where he stands.
	Hull->BodyInstance.bUseCCD = true;

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

	// This hull must never sleep, and that is a correctness requirement rather than a
	// preference. A sleeping body keeps the force and torque handed to it and
	// integrates neither, so a ship under a control loop stops answering its own
	// thrusters; and the occupant's carry reads this transform every frame, so a hull
	// that stops updating stops carrying him.
	//
	// It bit hard. Chaos sleeps a body whose per-step velocity change stays under a
	// threshold, and that change shrinks as the frame rate rises: at 60 fps a full roll
	// command gains 0.05 rad/s per step and stays awake, at 400 fps it gains 0.0075 and
	// is re-slept inside the same step, losing what it gained. Measured on 2026-09-10 by
	// hand at ~370 fps and reproduced headlessly: roll settled at 1.7 deg/s instead of
	// 180.0, pitch at 1.7 instead of 120.0, while the readout reported the whole
	// commanded 1.6 MNm as achieved. The boundary sits between 200 and 300 fps.
	//
	// Two accidents hid it for an entire stage. Linear thrust never showed it, because
	// 16 m/s^2 clears the linear threshold in one step; yaw never showed it, because
	// this array makes more torque about Z than about X and wins the same race. And
	// every headless check runs at a fixed 60 fps, where the torque wins with room to
	// spare -- so "a full roll settles at exactly 180.0 deg/s" was true of the test rig
	// and false of the game on a fast machine.
	//
	// Waking the body when a wrench is applied was tried first and measured to do
	// nothing, for the reason above: being awake is not what is in question.
	//
	// A multiplier of zero means the hull sleeps only when it is exactly motionless, so
	// the first step of any commanded thrust keeps it awake from then on.
	Hull->BodyInstance.SleepFamily = ESleepFamily::Custom;
	Hull->BodyInstance.CustomSleepThresholdMultiplier = 0.0f;

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

	GunnerStation = CreateDefaultSubobject<USFGunnerStationComponent>(TEXT("GunnerStation"));

	// A metre and a half aft of the pilot, at exactly the pilot's height and for the
	// same reason (see PilotStation above: a seat above standing height drops him into
	// MOVE_Falling and the carry stops). Aft rather than beside so the roll axis, which
	// runs along X, stays overhead: at (-1.5, 0, -3) he is the same three metres from
	// it as in the pilot's seat. Within the 2 m interact radius of the spawn point, so
	// "nearest free seat" decides between the two and neither is out of reach from
	// where a person starts. The capsule (radius 34) spans X -184..-116; the aft wall's
	// inner face is at -390, and the cylinder marker at (-300, 0, 0) is three metres up.
	GunnerStation->SeatLocationMetres = FVector(
		StarForge::Units::CmToMetres(-GunnerSeatAftCm),
		0.0,
		StarForge::Units::CmToMetres(-InteriorHalfExtentCm + SpawnClearanceCm));

	AutomationOperator = CreateDefaultSubobject<USFAutomationOperatorComponent>(TEXT("AutomationOperator"));

	// On every hull, not only on a pirate, because there is no pirate class: Crewing is set after
	// construction, so the one hull type carries both operators and Crewing decides which of them
	// sits down (see BeginPlay). On the player's ship this one never takes the seat.
	PursuitOperator = CreateDefaultSubobject<USFPursuitOperatorComponent>(TEXT("PursuitOperator"));

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

	// The +Y wall is three panels with a hatch between them. Every figure is derived
	// from Half and the panel thickness, so moving a wall moves the hatch and the port
	// with it -- the same rule the turret mount and the seats already follow.
	//
	// The hatch starts at the deck's top face, Z -390, so no sill panel is needed under
	// it -- but not because the floor reaches all the way out. The floor slab stops at
	// Y 400 while the wall's slabs run from 390 to 410, which leaves a notch under the
	// opening 200 wide, 10 deep and 20 tall with nothing beneath it. Harmless twice
	// over: a capsule of radius 34 cannot enter a 10 cm gap, and docked, the tunnel's
	// own floor lies across it.
	const double DeckTopZ = -Half + PanelThickness * CubeEdgeCm * 0.5;      // -390
	const double HatchTopZ = DeckTopZ + HatchHeightCm;                      // -140
	const double JambWidthCm = Half - HatchHalfWidthCm;                     //  300
	const double JambCentreXCm = (Half + HatchHalfWidthCm) * 0.5;           //  250
	const double LintelHeightCm = Half - HatchTopZ;                         //  540
	const double LintelCentreZCm = (HatchTopZ + Half) * 0.5;                //  130

	AddBoxPanel(TEXT("WallYPosJambXNeg"), FVector(-JambCentreXCm, Half, 0.0),
		FVector(JambWidthCm / CubeEdgeCm, PanelThickness, Span));
	AddBoxPanel(TEXT("WallYPosJambXPos"), FVector(JambCentreXCm, Half, 0.0),
		FVector(JambWidthCm / CubeEdgeCm, PanelThickness, Span));
	AddBoxPanel(TEXT("WallYPosLintel"), FVector(0.0, Half, LintelCentreZCm),
		FVector(HatchHalfWidthCm * 2.0 / CubeEdgeCm, PanelThickness, LintelHeightCm / CubeEdgeCm));

	// Centred in the hatch, on the wall's mid-plane: +X out along the ship's +Y, +Z the
	// deck's up. A yaw of 90 takes local +X onto +Y and leaves +Z alone.
	//
	// The cabin is no longer a closed box, and that is a real change to this rig, not
	// only to the loop. A standing occupant is bounded by the walls and by nothing else
	// -- falling friction limits only the component along the deck -- so on L_FrameSpike,
	// where there is no tunnel behind this opening, a knocked-down man now has a way out
	// of the room. Every figure already recorded was taken standing and is unaffected;
	// the fact is written down rather than compensated, because closing the hatch on one
	// map and opening it on another would mean having two ships.
	DockingPort = CreateDefaultSubobject<USFDockingPortComponent>(TEXT("DockingPort"));
	DockingPort->SetupAttachment(Hull);
	DockingPort->SetRelativeLocation(FVector(0.0, Half, DeckTopZ + HatchHeightCm * 0.5));
	DockingPort->SetRelativeRotation(FRotator(0.0, 90.0, 0.0));

	Docking = CreateDefaultSubobject<USFDockingComponent>(TEXT("Docking"));

	AddBoxPanel(TEXT("WallYNeg"), FVector(0.0, -Half, 0.0), FVector(Span, PanelThickness, Span));

	// The cabin, out to the inner faces of the panels: half-extent 390 on every axis,
	// which along +Y is exactly the hatch plane. Everything inside the hull is the ship;
	// the tunnel and beyond are the station (spec 8), and the two volumes meet at this
	// plane with the wall's twenty centimetres between them -- ten to each side of the
	// pair's midline. Nobody is claimed in that gap, and the last binding holds through it.
	Interior = CreateDefaultSubobject<USFInteriorVolumeComponent>(TEXT("Interior"));
	Interior->SetupAttachment(Hull);
	Interior->SetRelativeLocation(FVector::ZeroVector);
	Interior->SetBoxExtent(FVector(Half - PanelThickness * CubeEdgeCm * 0.5));

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

	// The mount hangs from the ceiling inside the cabin, which is not where a turret
	// goes. It is where a person can see it: the cabin has no windows, so a barrel on
	// the roof could be checked only from the log, and the one thing a log cannot check
	// is whether the picture agrees with the numbers. The aim passes through the walls;
	// the drone outside is what the numbers describe.
	//
	// Height: the ceiling panel's inner face is at InteriorHalfExtentCm minus half its
	// thickness (400 - 10 = 390). A barrel of BarrelLengthCm pitched to its +60 stop
	// rises sin 60 = 0.866 of its length above the mount, so a mount at
	// 390 - 100 - 10 = 280 tops out at 366.6, clear by 23 cm. Derived from the same
	// constants as the panels, so moving a wall moves the turret with it.
	Turret = CreateDefaultSubobject<USFTurretComponent>(TEXT("Turret"));
	Turret->SetupAttachment(Hull);
	Turret->SetRelativeLocation(FVector(
		0.0, 0.0,
		InteriorHalfExtentCm - PanelThickness * CubeEdgeCm * 0.5 - BarrelLengthCm - BarrelCeilingClearanceCm));

	TurretBarrel = CreateDefaultSubobject<USceneComponent>(TEXT("TurretBarrel"));
	TurretBarrel->SetupAttachment(Turret);
	Turret->SetBarrel(TurretBarrel);

	// A cone, not a cylinder: it has a pointed end, so a sign error in yaw or pitch
	// shows as a barrel pointing away from where the readout says. The engine cone's
	// apex is along its own +Z; a pitch of -90 turns +Z onto +X (a +90 pitch takes +X
	// onto +Z, and this is its inverse), so the apex is the muzzle. Offset forward by
	// half its length so the base sits on the pivot.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BarrelMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	TurretBarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretBarrelMesh"));
	TurretBarrelMesh->SetupAttachment(TurretBarrel);
	TurretBarrelMesh->SetRelativeRotation(FRotator(-90.0, 0.0, 0.0));
	TurretBarrelMesh->SetRelativeLocation(FVector(BarrelLengthCm * 0.5, 0.0, 0.0));
	TurretBarrelMesh->SetRelativeScale3D(FVector(
		BarrelThicknessScale, BarrelThicknessScale, BarrelLengthCm / CubeEdgeCm));
	TurretBarrelMesh->SetMobility(EComponentMobility::Movable);

	// Visual only. It must not be able to block the walk test or catch a sweep.
	TurretBarrelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (BarrelMesh.Succeeded())
	{
		TurretBarrelMesh->SetStaticMesh(BarrelMesh.Object);
	}

	// At the apex of the cone. The mesh is offset half its length along the pivot's +X and is
	// BarrelLengthCm long, so its tip is exactly BarrelLengthCm from the axis the turret turns
	// about -- and hanging the muzzle off the same pivot means the shot leaves where the
	// picture says the barrel points.
	Weapon = CreateDefaultSubobject<USFWeaponComponent>(TEXT("Weapon"));
	Weapon->SetupAttachment(TurretBarrel);
	Weapon->SetRelativeLocation(FVector(BarrelLengthCm, 0.0, 0.0));
	if (ShipDataAsset.Succeeded())
	{
		Weapon->ShipData = ShipDataAsset.Object;
	}

	// The half-extents the panels are placed from, not a second figure. Hits land on the panels'
	// outer faces at 410, not 400; the face is decided by the ratios of the three coordinates to
	// these, so on a hull with the same half-extent on every axis the ten centimetres change
	// nothing.
	Damageable = CreateDefaultSubobject<USFDamageableComponent>(TEXT("Damageable"));
	Damageable->HalfExtentsCm = FVector(InteriorHalfExtentCm);
	if (ShipDataAsset.Succeeded())
	{
		Damageable->ShipData = ShipDataAsset.Object;
	}
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
	// Before Super, and that ordering is the point: Super is what begins play on the
	// components, and each of them decides in its own BeginPlay whether to take a seat or
	// to listen to a console variable. Told afterwards, they would have already acted.
	const bool bAnswersConsole = (Crewing == ESFShipCrewing::PlayerShip);
	if (PilotStation) { PilotStation->SetConsoleControlled(bAnswersConsole); }
	if (Thrusters) { Thrusters->SetConsoleControlled(bAnswersConsole); }
	if (ConsoleOperator) { ConsoleOperator->SetConsoleControlled(bAnswersConsole); }
	if (AutomationOperator) { AutomationOperator->SetConsoleControlled(bAnswersConsole); }
	if (PursuitOperator) { PursuitOperator->SetConsoleControlled(bAnswersConsole); }
	if (Docking) { Docking->SetConsoleControlled(bAnswersConsole); }
	if (Weapon)
	{
		Weapon->SetConsoleControlled(bAnswersConsole);
		Weapon->SetWeaponsFree(bWeaponsFree);
	}

	Super::BeginPlay();

	// Said once at startup rather than checked by hand, because a hand check is a
	// check somebody forgets. An asynchronous solver with interpolation is the one
	// mechanism that can hand GetActorTransform a smoothed value instead of the one
	// physics computed, and every drift figure this rig has ever produced rests on
	// that not happening.
	//
	// Asked of this world's solver, the way the engine asks it: IsUsingAsyncResults
	// (PhysicsSolverBase.h 712-715) is !p.ForceDisableAsyncPhysics && AsyncDt >= 0, and the
	// scene set AsyncDt from bTickPhysicsAsync when it was built, -1 with it off
	// (PhysScene_Chaos.cpp 549). Its two inputs are read rather than the function called,
	// because it reads a Chaos export and this module does not link Chaos. A variable that is
	// not found is said in capitals on a Warning, never in brackets on a Display line: the
	// "p.Chaos.Solver.AsyncPhysicsBlockMode" looked up here until 2026-10-01 is registered
	// nowhere in 5.8, all 306 of these lines in evidence read "(cvar absent)", and a build with
	// Tick Physics Async on would have printed the same. Substepping is read from the settings,
	// where the engine reads it every frame (PhysLevel.cpp 212).
	//
	// Either one on is a state no drift figure in this project was taken under, so it is said
	// in capitals and as a Warning, as every rig line says a state that did not hold.
	const FPhysScene* PhysicsScene = GetWorld() ? GetWorld()->GetPhysicsScene() : nullptr;
	const Chaos::FPhysicsSolverBase* Solver = PhysicsScene ? PhysicsScene->GetSolver() : nullptr;
	const IConsoleVariable* ForceDisableAsync =
		IConsoleManager::Get().FindConsoleVariable(TEXT("p.ForceDisableAsyncPhysics"));
	const bool bSubstepping = UPhysicsSettings::Get()->bSubstepping;
	const bool bRead = Solver && ForceDisableAsync;
	const bool bAsync = bRead && ForceDisableAsync->GetInt() == 0 && Solver->GetAsyncDeltaTime() >= 0.0;

	const FString SolverText = !Solver ? FString(TEXT("solver UNREAD -- this world has no physics scene"))
		: !ForceDisableAsync ? FString(TEXT("solver UNREAD -- p.ForceDisableAsyncPhysics is not registered"))
		: bAsync ? FString::Printf(TEXT("solver ASYNCHRONOUS at a fixed %.4f s step"), Solver->GetAsyncDeltaTime())
		: FString(TEXT("solver synchronous"));

	if (!bRead || bAsync || bSubstepping)
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Physics: %s, substepping %s. Drift figures are only comparable across runs where these match."),
			*SolverText, bSubstepping ? TEXT("ON") : TEXT("off"));
	}
	else
	{
		UE_LOG(LogStarForge, Display,
			TEXT("Physics: %s, substepping off. Drift figures are only comparable across runs where these match."),
			*SolverText);
	}

	// The rig assembles itself, so the thing the turret tracks is spawned here rather
	// than placed: L_FrameSpike is empty by design. Anchored to this ship's position so
	// it stays in range whichever way the ship is driven, and in world axes so it does
	// not turn with the ship, which is what makes a rolling hull a test of the aim.
	//
	// Deferred, because the drone's BeginPlay logs its anchor and takes its first
	// position from it, and a spawn that finishes before SetAnchor would log "the world
	// origin" for a drone that is about to be anchored to this ship.
	if (bSpawnTargetDrone)
	{
		FTransform DroneSpawn = GetActorTransform();
		DroneSpawn.SetScale3D(FVector::OneVector);
		TargetDrone = GetWorld()->SpawnActorDeferred<ASFTargetDrone>(
			ASFTargetDrone::StaticClass(), DroneSpawn, this, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (TargetDrone)
		{
			TargetDrone->SetAnchor(this);
			TargetDrone->FinishSpawning(DroneSpawn);

			// Components have begun play inside Super::BeginPlay, so the automation is
			// already in its seat by now; the first station tick is still ahead, and a null
			// drone until then is reported as no target rather than aimed at.
			if (AutomationOperator)
			{
				AutomationOperator->SetTargetActor(TargetDrone);
			}
		}
		else
		{
			UE_LOG(LogStarForge, Error,
				TEXT("Target drone for [%s] failed to spawn; the turret will have nothing to track."), *GetName());
		}
	}
	else
	{
		// Said out loud: a gunner post with no target looks exactly like a gunner post
		// that is not working, and this is the line that tells the two apart.
		UE_LOG(LogStarForge, Display,
			TEXT("Ship [%s] spawns no target drone; its gunner's automation will track whatever it is given."),
			*GetName());
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

	// Only the speed is kept here, for the readout's profile-phase line. The
	// acceleration moved to the frame component: the felt-acceleration figure and the
	// knockdown are read from one instrument, and that instrument is not the test
	// ship's class -- the movement component must not have to know this class exists
	// to find out how hard the deck is accelerating.
	PreviousVelocityCms = Hull->GetPhysicsLinearVelocity();
}

void ASFTestShip::TickScriptedProfile()
{
	// The profile commands a physics body, and a hull that is not simulating is not one:
	// the velocities below go nowhere, and yet the ship reads them straight back out of
	// Chaos into its readout, printing speed it never had beside the word "docked". The
	// lock on the thruster array shuts the other route into that same lie by itself and
	// cannot shut this one, because this writes velocity rather than force: the lock has
	// to be READ here, which is what the second half of the condition does.
	//
	// A locked array is the second refusal, and it is the same one the pilot's post makes
	// one level up: locked means nothing may command this hull, whatever locked it, and
	// the profile is a commander like any other with no standing the seat does not have.
	//
	// Both are asked as questions about the hull rather than about docking, on purpose:
	// those are the profile's true preconditions, and putting them this way means the rig
	// never learns that docking exists, exactly as the pilot's post never does. The two
	// coincide while docking is what locks the array, because docking also stops the
	// simulation, and they part the moment something locks a hull that is still simulating:
	// a powered-down ship does, and IsLocked asks its damage model as well as the docking lock.
	// Neither predicate is therefore a restatement of the other, and dropping either one is
	// a decision, not a tidy-up.
	//
	// Silent, because it would run every frame.
	if (!Hull->IsSimulatingPhysics() || (Thrusters && Thrusters->IsLocked()))
	{
		return;
	}

	// Third refusal, beside the two that ask about the hull. An automated hull does not
	// answer sf.ShipProfile at all, so the variable cannot fly a pirate by accident while
	// a person is measuring his own ship with it.
	if (Crewing != ESFShipCrewing::PlayerShip) { return; }

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

	// Docking first, because it gates everything below it: a docked ship is a ship whose
	// engines are locked, and the flight numbers that follow are then correctly zero
	// rather than suspiciously zero. The variable behind this line is sf.FlightLog, which
	// is declared in this file -- the docking component's own sf.Dock is read there and
	// nowhere else, and this line reads the component's accessors instead.
	if (Docking)
	{
		FString DockText;

		switch (Docking->GetState())
		{
		case ESFDockingState::Docked:
			DockText = TEXT("docked");
			break;
		case ESFDockingState::Capturing:
			DockText = FString::Printf(TEXT("capturing %.1f of %.1f s"),
				Docking->GetCaptureElapsedSeconds(), Docking->GetCaptureSeconds());
			break;
		case ESFDockingState::Releasing:
			DockText = FString::Printf(TEXT("releasing %.1f of %.1f s"),
				Docking->GetCaptureElapsedSeconds(), Docking->GetCaptureSeconds());
			break;
		default:
			if (!Docking->HasStation())
			{
				DockText = TEXT("free -- no station");
			}
			else
			{
				const StarForge::Docking::FDockingApproach Approach = Docking->GetLastApproach();
				const FString Verdict = Docking->CanDockNow()
					? FString(TEXT("CAN DOCK"))
					: FString::Printf(TEXT("out of window: %s"),
						USFDockingComponent::RefusalName(Docking->GetLastRefusal()));

				const FVector At = Docking->GetPortOffsetShipLocalMetres();
				DockText = FString::Printf(
					TEXT("free | port %.1f m at (%+.1f %+.1f %+.1f) ship, %.1f m/s, fwd %.1f deg, up %.1f deg -- %s"),
					Approach.Distance, At.X, At.Y, At.Z, Approach.SpeedMetresPerSec,
					FMath::RadiansToDegrees(Approach.ForwardAngleRad),
					FMath::RadiansToDegrees(Approach.UpAngleRad),
					*Verdict);
			}
			break;
		}

		UE_LOG(LogStarForge, Log, TEXT("Dock: [%s] %s"), *GetName(), *DockText);
	}

	// The turret reports before the thruster check: a ship whose array never built can
	// still lay a turret, and a line that vanishes with the array would look like a
	// turret that stopped.
	if (GunnerStation && GunnerStation->GetTurret())
	{
		const USFTurretComponent* GunnerTurret = GunnerStation->GetTurret();
		const StarForge::FTurretIntent TurretIntent = GunnerStation->GetLastIntent();

		FString GunnedBy = GunnerStation->GetOperatorDisplayName();
		if (GunnerStation->IsOccupied() && !GunnerStation->IsOperatorGunning())
		{
			GunnedBy += TEXT(" (standing down)");
		}

		const FString Target = TurretIntent.bHasTarget
			? FString::Printf(TEXT("(%.0f %.0f %.0f) cm"),
				TurretIntent.TargetWorldLocation.X,
				TurretIntent.TargetWorldLocation.Y,
				TurretIntent.TargetWorldLocation.Z)
			: FString(TEXT("none"));

		const StarForge::Turret::FTurretAim Bearing = GunnerTurret->GetLastBearing();
		const FString BearingText = GunnerTurret->HasBearing()
			? FString::Printf(TEXT("yaw %.1f pitch %.1f deg"),
				FMath::RadiansToDegrees(Bearing.YawRad), FMath::RadiansToDegrees(Bearing.PitchRad))
			: FString(TEXT("none"));

		const StarForge::Turret::FTurretAim Aim = GunnerTurret->GetAim();

		UE_LOG(LogStarForge, Log,
			TEXT("Turret: [%s] gunned by %s | target %s firing %d | bearing %s | aim yaw %.1f pitch %.1f deg | %s"),
			*GetName(), *GunnedBy, *Target, TurretIntent.bFiring ? 1 : 0, *BearingText,
			FMath::RadiansToDegrees(Aim.YawRad), FMath::RadiansToDegrees(Aim.PitchRad),
			USFTurretComponent::StateName(GunnerTurret->GetState()));
	}
	else
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Turret: [%s] no gunner station or no turret -- nothing is aiming."), *GetName());
	}

	// A line of its own rather than more fields on Flight:, which already takes reading in pieces.
	// Before the thruster check for the turret's reason: a hull whose array never built can still
	// be shot, and a Combat line that vanished with the array would read as a hull nobody can hurt.
	if (Damageable && !Damageable->IsReady())
	{
		// Its accessors read zero, which is also what a ship shot to pieces reads. Said instead,
		// as the thruster count below says NOT built rather than 0/24.
		UE_LOG(LogStarForge, Warning,
			TEXT("Combat: [%s] damageable component NOT ready -- no shield or hull figures to report."), *GetName());
	}
	else if (Damageable)
	{
		FString ShieldText;
		for (int32 i = 0; i < StarForge::Damage::FaceCount; ++i)
		{
			const StarForge::Damage::EShipFace Face = static_cast<StarForge::Damage::EShipFace>(i);
			ShieldText += FString::Printf(TEXT(" %s %.0f"),
				StarForge::Damage::FaceName(Face), Damageable->GetFaceCharge(Face));
		}

		// Surviving over built-with, and still counted once the survivors no longer build: a ship
		// shot down to two corners reads "12/24 NOT built", which says what happened to it, where
		// "NOT built" alone would read the same as a ship that never had an asset.
		FString ThrusterText = TEXT("thrusters NOT built");
		if (Thrusters && Thrusters->ShipData)
		{
			ThrusterText = FString::Printf(TEXT("thrusters %d/%d%s"),
				Thrusters->GetLiveThrusterCount(), Thrusters->GetThrusterCount(),
				Thrusters->IsReady() ? TEXT("") : TEXT(" NOT built"));
		}

		UE_LOG(LogStarForge, Log, TEXT("Combat: [%s] shields%s, hull %.1f/%.1f, %s, %s."),
			*GetName(), *ShieldText, Damageable->GetHullStrength(), Damageable->GetHullCapacity(),
			*ThrusterText, Damageable->IsPoweredDown() ? TEXT("powered down") : TEXT("powered up"));
	}
	else
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Combat: [%s] no damageable component -- nothing on this hull can be hurt."), *GetName());
	}

	// Only where the pursuit operator can hold a seat, so neither existing rig gains a line. Before
	// the thruster check for the turret's reason, and read from the two transforms rather than
	// from the last intent: a powered-down or declining pirate is asked for nothing, and its
	// distance is exactly what those two cases are judged on. The game time is in the line because
	// the frame counter in the log prefix wraps at a thousand, and 400 fps passes that in 2.5 s.
	if (Crewing == ESFShipCrewing::Automated && PursuitOperator)
	{
		double DistanceMetres = 0.0;
		StarForge::Turret::FTurretAim Bearing;
		if (PursuitOperator->MeasureTarget(DistanceMetres, Bearing))
		{
			UE_LOG(LogStarForge, Log,
				TEXT("Pursuit: [%s] t %.2f s | target %s at %.1f m, bearing yaw %.1f pitch %.1f deg | hold %.0f m%s"),
				*GetName(), GetWorld()->GetTimeSeconds(), *GetNameSafe(PursuitOperator->GetTargetActor()),
				DistanceMetres, FMath::RadiansToDegrees(Bearing.YawRad), FMath::RadiansToDegrees(Bearing.PitchRad),
				PursuitOperator->HoldDistanceMetres,
				PursuitOperator->IsDeclining() ? TEXT(" | declining (sf.PirateAI 0)") : TEXT(""));
		}
		else
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("Pursuit: [%s] t %.2f s | no target or no frame -- nothing to chase, and the post is declined."),
				*GetName(), GetWorld()->GetTimeSeconds());
		}
	}

	// The log is now written in both modes, so it can be reached before the array has
	// built -- which the flight branch used to make impossible. Said out loud rather
	// than skipped: a flight log that simply stops printing reads like a quiet ship,
	// and the whole point of writing these numbers down is that a silent instrument
	// must never be mistakable for a calm one.
	if (!Thrusters || !Thrusters->IsReady())
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Flight: [%s] thruster array NOT built -- no flight numbers to report."), *GetName());
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
			TEXT("Flight: [%s] no pilot station -- the intent below is nobody's."), *GetName());
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
		TEXT("Flight: [%s] flown by %s | intent lin (%.2f %.2f %.2f) ang (%.2f %.2f %.2f) assist %d | ")
		TEXT("speed %.1f m/s, accel %.1f m/s2, ang roll/pitch/yaw (%.1f %.1f %.1f) deg/s | ")
		TEXT("force want (%.0f %.0f %.0f) got (%.0f %.0f %.0f) N | ")
		TEXT("torque want (%.0f %.0f %.0f) got (%.0f %.0f %.0f) Nm | ")
		TEXT("throttles %d of %d commanded, %d spooled"),
		*GetName(),
		*FlyingBy,
		Intent.LinearAxis.X, Intent.LinearAxis.Y, Intent.LinearAxis.Z,
		Intent.AngularAxis.X, Intent.AngularAxis.Y, Intent.AngularAxis.Z,
		Intent.bFlightAssistEnabled ? 1 : 0,
		State.LinearVelocity.Size(),
		Frame->GetLinearAccelMetersPerSecSq().Size(),
		FMath::RadiansToDegrees(State.AngularVelocity.X),
		FMath::RadiansToDegrees(State.AngularVelocity.Y),
		FMath::RadiansToDegrees(State.AngularVelocity.Z),
		Demand.Force.X, Demand.Force.Y, Demand.Force.Z,
		Achieved.Force.X, Achieved.Force.Y, Achieved.Force.Z,
		Demand.Torque.X, Demand.Torque.Y, Demand.Torque.Z,
		Achieved.Torque.X, Achieved.Torque.Y, Achieved.Torque.Z,
		Commanded, Thrusters->GetTargetThrottles().Num(), Spooled);
}
