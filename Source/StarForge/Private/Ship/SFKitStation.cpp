#include "Ship/SFKitStation.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DockingMath.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "NumberText.h"
#include "Ship/SFDockingComponent.h"
#include "Ship/SFDockingPortComponent.h"
#include "Ship/SFInteriorVolumeComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "ShipFrameMath.h"
#include "StarForgeLog.h"
#include "TimerManager.h"
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

	FVector ToMetres(const FVector& Cm)
	{
		return FVector(StarForge::Units::CmToMetres(Cm.X), StarForge::Units::CmToMetres(Cm.Y), StarForge::Units::CmToMetres(Cm.Z));
	}

	FBox ToMetres(const FBox& Cm)
	{
		return Cm.IsValid ? FBox(ToMetres(Cm.Min), ToMetres(Cm.Max)) : FBox(ForceInit);
	}

	/**
	 * A primitive's box in the station's own frame, in metres: through the same relative transform the
	 * release boxes the station's colliders with (USFDockingComponent::StationPhysicalBoundsLocalCm),
	 * so the line and the release read the same geometry.
	 */
	FBox BoxInFrameMetres(const UPrimitiveComponent& Primitive, const FTransform& StationFrameCm)
	{
		return ToMetres(Primitive.CalcBounds(Primitive.GetComponentTransform().GetRelativeTransform(StationFrameCm)).GetBox());
	}

	/** "-Y" for an axis along one of the station's own, the vector otherwise. */
	FString AxisWord(const FVector& Axis)
	{
		static const TCHAR* const Names[3] = { TEXT("X"), TEXT("Y"), TEXT("Z") };
		for (int32 i = 0; i < 3; ++i)
		{
			if (FMath::IsNearlyEqual(FMath::Abs(Axis[i]), 1.0, 1e-6))
			{
				return FString::Printf(TEXT("%s%s"), Axis[i] > 0.0 ? TEXT("+") : TEXT("-"), Names[i]);
			}
		}
		return FString::Printf(TEXT("(%.2f %.2f %.2f)"), Axis.X, Axis.Y, Axis.Z);
	}

	/**
	 * A coordinate as it is printed to two decimals, with a sign-only zero made plain. A turned
	 * station's round trip through GetRelativeTransform can leave a rounding residue of either sign
	 * where the builder put zero, and "-0.00" in a line that is compared character for character is a
	 * difference made of nothing.
	 */
	double Shown(double Metres)
	{
		return FMath::Abs(Metres) < 0.005 ? 0.0 : Metres;
	}

	const TCHAR* Plural(int32 Count)
	{
		return Count == 1 ? TEXT("") : TEXT("s");
	}

	/**
	 * Control N1 and N1b of the station kit (spec 8.3): a 4 m collider at the far end of block 0's berth
	 * face, its -Y face this many metres past the port plane. A string, as sf.PlaceOffsetM is,
	 * read by StarForge::Parse::TryParseFiniteNumber (NumberText.h), which wants a digit in the mantissa
	 * and in any exponent: Atod answers zero for a string with no number in it, and
	 * FDefaultValueHelper::ParseDouble let ".", "-" and "e10" through as zero -- a control set with a
	 * typo would have placed nothing and read calm. Empty or 0: no collider. Read at the station's
	 * BeginPlay and again at every change of the value, through the variable's change delegate -- a kit
	 * station does not tick.
	 *
	 * Not put back when a world ends, unlike sf.Dock, sf.BindFrame and sf.AimAt: those act on a change,
	 * and a value left behind is a keypress in the next world; this is a level, as sf.PlaceOffsetM is,
	 * and a value left from the last Play is a control still set, which the Station rig: line says in red.
	 *
	 * Declared here because this is the file that reads it. The Station rig: line's Error is legitimate
	 * only while a control acts (spec 8.3): this is neither empty nor 0 (N1, N1b), or a volume grown by
	 * sf.StationVolumeReach has not been returned to 0 (N2, N2b).
	 */
	TAutoConsoleVariable<FString> CVarPortProtrusionM(TEXT("sf.PortProtrusionM"), TEXT(""),
		TEXT("Metres the -Y face of a 4 m control cube, X 26..30 and Z 0..4 of block 0, stands past the port plane (station kit, N1/N1b).\n")
		TEXT("Empty or 0: none. Read at the kit station's BeginPlay and on every change."),
		ECVF_Default);

	/** Control N1's collider, spec 8.3: 4 m on a side, X 26..30 and Z 0..4 of block 0, far from the hull's path. */
	constexpr double ControlSizeCm = 400.0;
	constexpr double ControlXMinCm = 2600.0;
	constexpr double ControlZMinCm = 0.0;

	/**
	 * The smallest control accepted: the line's printed resolution (spec 8.3). Below
	 * StarForge::Docking::RedAtOrAboveMetres, half of it, the line would print 0.000 and stay calm; between
	 * the two it would print 0.001 and go red. A control is refused under a whole millimetre so that every
	 * accepted one is at least one printed unit: the line prints it rounded to the millimetre, never as
	 * 0.000, and goes red (amendment P21).
	 */
	constexpr double MinimumControlMetres = 0.001;

	/** Metres and seconds alike: a finite number written out, or false and OutMetres untouched. */
	bool ParseFiniteMetres(const FString& Raw, double& OutMetres)
	{
		return StarForge::Parse::TryParseFiniteNumber(Raw, OutMetres);
	}

	void StationVolumeReachNow(UWorld* World, FName VolumeName, double Metres, const FString& Input)
	{
		int32 Applied = 0;
		// TActorIterator checks for a world and would stop the program over a null one; with none, no
		// station answers, and the Warning below says so.
		if (World)
		{
			for (TActorIterator<ASFKitStation> It(World); It; ++It)
			{
				if (It->ApplyVolumeReach(VolumeName, Metres))
				{
					++Applied;
				}
			}
		}
		if (Applied == 0)
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("sf.StationVolumeReach '%s': no kit station in this world has an interior volume named %s; nothing was changed."),
				*Input, *VolumeName.ToString());
		}
	}

	void StationVolumeReachFromConsole(const TArray<FString>& Args, UWorld* World)
	{
		const FString Input = FString::Join(Args, TEXT(" "));

		double Metres = 0.0;
		double AfterSeconds = 0.0;
		const bool bShape = (Args.Num() == 2 || Args.Num() == 3)
			&& ParseFiniteMetres(Args[1], Metres)
			&& (Args.Num() == 2 || ParseFiniteMetres(Args[2], AfterSeconds));

		// A negative figure accepted in silence would shrink the volume from its outward face, read
		// 0.000 and look like a control that found nothing; a positive one under a millimetre is under
		// the line's printed resolution, and under half of one it would print 0.000.
		if (!bShape || !(Metres == 0.0 || Metres >= MinimumControlMetres) || !(AfterSeconds >= 0.0))
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("sf.StationVolumeReach '%s': needs a volume name, 0 or a number of metres of at least 0.001, and optionally a ")
				TEXT("non-negative delay in seconds; nothing was changed."),
				*Input);
			return;
		}

		const FName VolumeName(*Args[0]);
		// The timer takes a float and sets nothing at or under zero (TimerManager.cpp 673). A delay too
		// small to survive that conversion is no delay: scheduled, it would set no timer while the line
		// below announced one.
		const float DelaySeconds = static_cast<float>(AfterSeconds);
		if (DelaySeconds <= 0.f || !World)
		{
			StationVolumeReachNow(World, VolumeName, Metres, Input);
			return;
		}

		// Seconds of game time, not frames, and for the reason sf.KillThruster has one: -ExecCmds runs
		// its whole list on the first frame, before the start-up handover has been carried, and a
		// volume grown then would supersede it (spec 8.3).
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle,
			FTimerDelegate::CreateLambda([WeakWorld = TWeakObjectPtr<UWorld>(World), VolumeName, Metres, Input]()
			{
				if (UWorld* LiveWorld = WeakWorld.Get())
				{
					StationVolumeReachNow(LiveWorld, VolumeName, Metres, Input);
				}
			}),
			DelaySeconds, false);

		UE_LOG(LogStarForge, Display,
			TEXT("sf.StationVolumeReach '%s': scheduled %.2f s of game time from now."), *Input, DelaySeconds);
	}

	/**
	 * Controls N2 and N2b (spec 8.3). A COMMAND and not a variable, as sf.KillThruster is: a command runs
	 * once per entry, and it takes a delay.
	 */
	FAutoConsoleCommandWithWorldAndArgs CmdStationVolumeReach(
		TEXT("sf.StationVolumeReach"),
		TEXT("sf.StationVolumeReach <Volume> <Metres> [Seconds]: grow a kit station's interior volume (Boom0, Hall1, ...) that\n")
		TEXT("many metres past its authored size, outward along its block's port axis, now or after that many seconds of game time. 0 restores it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StationVolumeReachFromConsole));
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

	// Control N1's collider, built now and switched off: default subobjects exist only in the
	// constructor, and it is placed at BeginPlay or at a change of sf.PortProtrusionM.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ControlCube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	ControlProtrusion = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ControlProtrusion"));
	ControlProtrusion->SetupAttachment(Root);
	ControlProtrusion->SetMobility(EComponentMobility::Movable);
	ControlProtrusion->SetCollisionObjectType(ECC_WorldStatic);
	ControlProtrusion->SetCollisionResponseToAllChannels(ECR_Block);
	ControlProtrusion->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ControlProtrusion->SetHiddenInGame(true);
	if (ControlCube.Succeeded())
	{
		ControlProtrusion->SetStaticMesh(ControlCube.Object);
	}
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

void ASFKitStation::BeginPlay()
{
	Super::BeginPlay();

	// Told of every change, not polled: this actor does not tick. The same pattern the world-bounds
	// line uses for WorldBounds.DisableChecks (SFWorldBoundsSubsystem.cpp 44-48).
	if (IConsoleVariable* Variable = KitStation::CVarPortProtrusionM.AsVariable())
	{
		PortProtrusionChangedHandle = Variable->OnChangedDelegate().AddUObject(this, &ASFKitStation::OnPortProtrusionChanged);
	}

	ApplyPortProtrusion(KitStation::CVarPortProtrusionM.GetValueOnGameThread());
	ReportStationRig();
}

void ASFKitStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IConsoleVariable* Variable = KitStation::CVarPortProtrusionM.AsVariable())
	{
		Variable->OnChangedDelegate().Remove(PortProtrusionChangedHandle);
	}
	PortProtrusionChangedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void ASFKitStation::OnPortProtrusionChanged(IConsoleVariable* Variable)
{
	// The delegate fires on a Set that leaves the value where it was, too; one line per real change.
	const FString Raw = Variable ? Variable->GetString() : FString();
	if (Raw == LastProtrusionRaw)
	{
		return;
	}
	ApplyPortProtrusion(Raw);
	ReportStationRig();
}

void ASFKitStation::ApplyPortProtrusion(const FString& Raw)
{
	using namespace KitStation;

	LastProtrusionRaw = Raw;

	double Metres = 0.0;
	bool bPlace = false;
	if (!Raw.IsEmpty())
	{
		if (ParseFiniteMetres(Raw, Metres) && (Metres == 0.0 || Metres >= MinimumControlMetres))
		{
			bPlace = Metres > 0.0;
		}
		else
		{
			// A negative protrusion taken in silence would read p = 0 and a calm line under a set control.
			UE_LOG(LogStarForge, Warning,
				TEXT("Station rig: sf.PortProtrusionM='%s' is not 0 or a finite number of metres of at least 0.001; no control collider was placed."),
				*Raw);
		}
	}

	if (!ControlProtrusion)
	{
		return;
	}

	if (!bPlace || BlockOriginsCm.IsEmpty())
	{
		ControlProtrusion->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ControlProtrusion->SetHiddenInGame(true);
		return;
	}

	// A 4 m cube at X 26..30 and Z 0..4 in block 0's frame B, its -Y face p past the port plane.
	// Placed and sized before collision is switched on, so a collider switched on from off is seen by
	// no step anywhere else; a change from one value to another moves a collider that is already on.
	const FVector MinCm = BlockOriginsCm[0]
		+ FVector(ControlXMinCm, PortPlaneYCm - StarForge::Units::MetresToCm(Metres), ControlZMinCm);
	ControlProtrusion->SetRelativeLocation(MinCm + FVector(ControlSizeCm * 0.5));
	// Qualified although KitStation is open here: SFTestShip.cpp has a constant of the same name in
	// the module's one anonymous namespace, and under the directive the bare name would be ambiguous
	// as soon as that file came first in the unity blob.
	ControlProtrusion->SetRelativeScale3D(FVector(ControlSizeCm / KitStation::CubeEdgeCm));
	ControlProtrusion->SetHiddenInGame(false);
	ControlProtrusion->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

bool ASFKitStation::ApplyVolumeReach(FName VolumeName, double Metres)
{
	using namespace KitStation;

	for (const FSFKitVolumeRecord& Record : VolumeRecords)
	{
		USFInteriorVolumeComponent* Volume = Volumes[Record.Volume];
		if (!Volume || Volume->GetFName() != VolumeName)
		{
			continue;
		}

		// Grown from its authored box, not from its last size, on its outward side only: the centre
		// moves out by half, the extent grows by half along the axis. Moved first and resized with
		// overlaps updated, so a man the grown box now covers is told in this frame (BoxComponent.cpp
		// 28-47), without a tick.
		const double GrowCm = StarForge::Units::MetresToCm(Metres);
		Volume->SetRelativeLocation(Record.AuthoredCentreCm + Record.OutwardAxis * (GrowCm * 0.5));
		Volume->SetBoxExtent(Record.AuthoredExtentCm + Record.OutwardAxis.GetAbs() * (GrowCm * 0.5), /*bUpdateOverlaps=*/ true);

		if (Metres > 0.0)
		{
			UE_LOG(LogStarForge, Display,
				TEXT("Station rig: sf.StationVolumeReach grew %s of '%s' %.3f m outward along %s."),
				*Volume->GetName(), *GetName(), Metres, *AxisWord(Record.OutwardAxis));
		}
		else
		{
			UE_LOG(LogStarForge, Display,
				TEXT("Station rig: sf.StationVolumeReach returned %s of '%s' to its authored size."),
				*Volume->GetName(), *GetName());
		}
		ReportStationRig();
		return true;
	}
	return false;
}

void ASFKitStation::OnDockingPortChanged(USFDockingPortComponent* Port)
{
	// Never before BeginPlay: on the place rig the whole start-up docking happens before the world
	// begins play, and the BeginPlay line reads the state as it stands by then (spec 6.8).
	if (HasActorBegunPlay())
	{
		ReportStationRig();
	}
}

void ASFKitStation::ReportStationRig()
{
	using namespace KitStation;

	const FTransform StationFrameCm(GetActorQuat(), GetActorLocation());
	const FBox StationBoxM = ToMetres(USFDockingComponent::StationPhysicalBoundsLocalCm(*this));

	// Counted from the world rather than from the builder's list: a control collider switched on is
	// one of them, and a mesh given collision by mistake would be one too (rule 5).
	TArray<const UPrimitiveComponent*> Physical;
	bool bSimulating = false;
	{
		TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			if (!Primitive)
			{
				continue;
			}
			if (CollisionEnabledHasPhysics(Primitive->GetCollisionEnabled()))
			{
				Physical.Add(Primitive);
			}
			bSimulating |= Primitive->IsSimulatingPhysics();
		}
	}

	bool bRed = !Frame || bSimulating || !StationBoxM.IsValid;

	FString Line = FString::Printf(
		TEXT("Station rig: '%s' (%s), frame %s, %s -- %d block%s, %d port%s, %d interior volume%s, %d physical collider%s "),
		*GetName(), *GetClass()->GetName(),
		Frame ? TEXT("present") : TEXT("MISSING"),
		bSimulating ? TEXT("a SIMULATING BODY") : TEXT("no simulating body"),
		BlockOriginsCm.Num(), Plural(BlockOriginsCm.Num()),
		Ports.Num(), Plural(Ports.Num()),
		Volumes.Num(), Plural(Volumes.Num()),
		Physical.Num(), Plural(Physical.Num()));

	Line += StationBoxM.IsValid
		? FString::Printf(TEXT("boxed in its own frame at X %.2f..%.2f Y %.2f..%.2f Z %.2f..%.2f m; "),
			Shown(StationBoxM.Min.X), Shown(StationBoxM.Max.X), Shown(StationBoxM.Min.Y), Shown(StationBoxM.Max.Y),
			Shown(StationBoxM.Min.Z), Shown(StationBoxM.Max.Z))
		: FString(TEXT("and NOTHING PHYSICAL to box; "));

	// Each volume against the walls that are meant to enclose it: rule 2, forms 1 and 2.
	TArray<FBox> VolumeBoxesM;
	TArray<FString> VolumeTexts;
	for (const FSFKitVolumeRecord& Record : VolumeRecords)
	{
		const USFInteriorVolumeComponent* Volume = Volumes[Record.Volume];
		const FBox VolumeM = BoxInFrameMetres(*Volume, StationFrameCm);
		VolumeBoxesM.Add(VolumeM);

		const double OutM = StarForge::Docking::BoxReachOutside(VolumeM, EnvelopeMetres(Record, StationFrameCm));
		if (!FMath::IsFinite(OutM))
		{
			// BoxReachOutside answers NaN for a broken box on EITHER side (task 7), so the line says which:
			// sent to the walls when the volume is what broke, the reader would look at the wrong thing
			// (amendment P20).
			const bool bVolumeReadable = VolumeM.IsValid
				&& FMath::IsFinite(VolumeM.Min.X) && FMath::IsFinite(VolumeM.Min.Y) && FMath::IsFinite(VolumeM.Min.Z)
				&& FMath::IsFinite(VolumeM.Max.X) && FMath::IsFinite(VolumeM.Max.Y) && FMath::IsFinite(VolumeM.Max.Z);
			bRed = true;
			VolumeTexts.Add(bVolumeReadable
				? FString::Printf(TEXT("%s's %s envelope could NOT BE READ"), *Volume->GetName(), Record.EnvelopeWord)
				: FString::Printf(TEXT("%s itself could NOT BE MEASURED"), *Volume->GetName()));
		}
		else if (StarForge::Docking::IsRedAtPrintedResolution(OutM))
		{
			bRed = true;
			VolumeTexts.Add(FString::Printf(TEXT("%s REACHES %.3f m OUT of its %s"), *Volume->GetName(), OutM, Record.EnvelopeWord));
		}
		else
		{
			VolumeTexts.Add(FString::Printf(TEXT("%s within its %s (%.3f m out)"), *Volume->GetName(), Record.EnvelopeWord, OutM));
		}
	}
	Line += FString::Join(VolumeTexts, TEXT(", ")) + TEXT(".");

	for (const TObjectPtr<USFDockingPortComponent>& Port : Ports)
	{
		if (Port)
		{
			Line += TEXT(" ") + DescribePort(*Port, StationFrameCm, StationBoxM, Physical, VolumeBoxesM, bRed);
		}
	}

	if (bRed)
	{
		UE_LOG(LogStarForge, Error, TEXT("%s"), *Line);
	}
	else
	{
		UE_LOG(LogStarForge, Display, TEXT("%s"), *Line);
	}
}

FString ASFKitStation::DescribePort(const USFDockingPortComponent& Port, const FTransform& StationFrameCm, const FBox& StationBoxM,
	const TArray<const UPrimitiveComponent*>& Physical, const TArray<FBox>& VolumeBoxesM, bool& bInOutRed) const
{
	using namespace KitStation;

	const FTransform PortInFrame = Port.GetComponentTransform().GetRelativeTransform(StationFrameCm);
	const FVector OriginM = ToMetres(PortInFrame.GetLocation());
	const FVector Axis = PortInFrame.GetUnitAxis(EAxis::X);

	// Rule 1: p from the one box the release uses, and named by the collider that reaches furthest,
	// so the line says what to move. The box is one for the whole station: a protrusion anywhere on
	// the face costs every port its full p (spec 5).
	const double PastM = StarForge::Docking::BoundsPastPortPlane(StationBoxM, OriginM, Axis);
	FString PastText;
	if (StarForge::Docking::IsRedAtPrintedResolution(PastM))
	{
		bInOutRed = true;
		FString Worst = TEXT("nothing it could name");
		double WorstM = -1.0;
		for (const UPrimitiveComponent* Primitive : Physical)
		{
			const double M = StarForge::Docking::BoundsPastPortPlane(BoxInFrameMetres(*Primitive, StationFrameCm), OriginM, Axis);
			if (!(M <= WorstM))
			{
				WorstM = M;
				Worst = Primitive->GetName();
			}
		}
		PastText = FString::Printf(TEXT("%.3f m of something physical PAST its plane (%s)"), PastM, *Worst);
	}
	else
	{
		PastText = FString::Printf(TEXT("nothing physical past its plane (%.3f m)"), PastM);
	}

	// Rule 2, form 3: no interior volume past any port's plane.
	double VolumePastM = 0.0;
	FString VolumeName;
	for (int32 i = 0; i < VolumeBoxesM.Num(); ++i)
	{
		const double M = StarForge::Docking::BoundsPastPortPlane(VolumeBoxesM[i], OriginM, Axis);
		if (!(M <= VolumePastM))
		{
			VolumePastM = M;
			VolumeName = Volumes[VolumeRecords[i].Volume]->GetName();
		}
		// A NaN sticks. "!(M <= max)" takes a NaN in, and the next finite volume would then take its
		// place -- !(finite <= NaN) is true too -- and a volume nobody could measure would leave the
		// line calm (amendment P20).
		if (FMath::IsNaN(VolumePastM))
		{
			break;
		}
	}
	FString VolumeText;
	if (FMath::IsNaN(VolumePastM))
	{
		bInOutRed = true;
		VolumeText = FString::Printf(TEXT("how far an interior volume reaches past it could NOT BE MEASURED (%s)"), *VolumeName);
	}
	else if (StarForge::Docking::IsRedAtPrintedResolution(VolumePastM))
	{
		bInOutRed = true;
		VolumeText = FString::Printf(TEXT("an interior VOLUME REACHES %.3f m PAST it (%s)"), VolumePastM, *VolumeName);
	}
	else
	{
		VolumeText = FString::Printf(TEXT("no interior volume past it (%.3f m)"), VolumePastM);
	}

	return FString::Printf(TEXT("%s at (%.2f %.2f %.2f) m facing %s: %s, %s; %s."),
		*Port.GetName(), Shown(OriginM.X), Shown(OriginM.Y), Shown(OriginM.Z), *AxisWord(Axis),
		*PastText, *VolumeText, *DescribeHolder(Port, bInOutRed));
}

FString ASFKitStation::DescribeHolder(const USFDockingPortComponent& Port, bool& bInOutRed) const
{
	const USFDockingComponent* Holder = Port.GetHolder();
	if (!Holder)
	{
		return TEXT("free");
	}

	const FString Name = GetNameSafe(Holder->GetOwner());
	switch (Holder->GetState())
	{
	case ESFDockingState::Capturing:
		return FString::Printf(TEXT("capturing by %s"), *Name);
	case ESFDockingState::Releasing:
		return FString::Printf(TEXT("releasing by %s"), *Name);
	case ESFDockingState::Docked:
	{
		// Only here: ReleaseStandoff answers how far the hull still has to go along the port's axis,
		// from the pose it is in, for the sphere round it to clear the station's physical box, and
		// only from the docked pose is that the release F would make. Asked from any other pose it
		// answers another question beside a calm verdict (spec 6.8).
		double StandoffM = 0.0;
		double PeakMss = 0.0;
		if (!Holder->MeasureRelease(StandoffM, PeakMss))
		{
			bInOutRed = true;
			return FString::Printf(TEXT("held by %s, whose release could NOT BE MEASURED"), *Name);
		}

		// The very comparisons the release warns by (SFDockingComponent.cpp): the window on >, the
		// ceiling on >=.
		const bool bInside = StandoffM <= Holder->DistanceMetres;
		const bool bUnder = PeakMss < StarForge::ShipFrame::GravPlateLimitMetersPerSecSq;
		bInOutRed |= !bInside || !bUnder;

		const FString PeakText = (PeakMss >= TNumericLimits<double>::Max())
			? FString(TEXT("unbounded")) : FString::Printf(TEXT("%.1f m/s2"), PeakMss);

		return FString::Printf(
			TEXT("held by %s, whose release would be %.2f m in %.1f s, peak %s -- %s its %.1f m window, %s the %.2f m/s2 limit"),
			*Name, StandoffM, Holder->GetCaptureSeconds(), *PeakText,
			bInside ? TEXT("inside") : TEXT("OUTSIDE"), Holder->DistanceMetres,
			bUnder ? TEXT("under") : TEXT("OVER"), StarForge::ShipFrame::GravPlateLimitMetersPerSecSq);
	}
	default:
		bInOutRed = true;
		return FString::Printf(TEXT("held by %s while it reads FREE"), *Name);
	}
}

FBox ASFKitStation::EnvelopeMetres(const FSFKitVolumeRecord& Record, const FTransform& StationFrameCm) const
{
	using namespace KitStation;

	FVector Min(0.0);
	FVector Max(0.0);
	for (int32 Face = 0; Face < 6; ++Face)
	{
		const int32 Axis = Face / 2;
		const bool bMinFace = (Face % 2) == 0;

		if (Record.Faces[Face].IsEmpty())
		{
			return FBox(ForceInit);
		}

		// The tightest source: a min face as far in as its furthest-in wall, a max face likewise.
		double Value = bMinFace ? -TNumericLimits<double>::Max() : TNumericLimits<double>::Max();
		for (const FSFKitFaceSource& Source : Record.Faces[Face])
		{
			const UPrimitiveComponent* Collider = Colliders.IsValidIndex(Source.Collider) ? Colliders[Source.Collider].Get() : nullptr;
			if (!Collider)
			{
				return FBox(ForceInit);
			}
			const FBox Wall = BoxInFrameMetres(*Collider, StationFrameCm);
			const double Side = Source.bMaxSide ? Wall.Max[Axis] : Wall.Min[Axis];
			if (!FMath::IsFinite(Side))
			{
				// FMath::Max and Min hold a NaN only until the next finite source, so one wall nobody could
				// measure would vanish into its neighbours and leave the line calm (amendment P20's class).
				return FBox(ForceInit);
			}
			Value = bMinFace ? FMath::Max(Value, Side) : FMath::Min(Value, Side);
		}
		(bMinFace ? Min : Max)[Axis] = Value;
	}
	return FBox(Min, Max);
}
