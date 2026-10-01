#include "Ship/SFDockingComponent.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/EngineTypes.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFDockingPortComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFTestStation.h"
#include "Ship/SFThrusterArrayComponent.h"
#include "ShipFrameMath.h"
#include "StarForgeLog.h"
#include "Units.h"

namespace
{
	/**
	 * Docking from the console: the headless twin of the F key.
	 *
	 * Declared here because this is the file that reads it. A lookup by name from
	 * somewhere else returns null on a miss, and a missing variable reads as zero --
	 * which on this variable would mean "undock", issued by nobody.
	 *
	 * -1 is "no command". The component acts on a CHANGE of the value, not on its level:
	 * read as a level, a 1 would ask for a dock sixty times a second, turning one refusal
	 * into a torrent and one success into an attempt to dock with itself.
	 *
	 * 2 exists for headless runs and for nothing else. -ExecCmds applies its whole list at
	 * startup, so "sf.Dock 0, sf.Dock 1" leaves only the last value behind and the cycle
	 * docked -> free -> docked cannot be asked for in one run at all. 2 asks for the whole
	 * cycle on a single edge: undock now, dock again one capture later.
	 *
	 * The variable outlives the world it was typed in -- a PIE session, RestartLevel, open --
	 * and every component starts from -1, so a value left behind is a keypress on the next
	 * world's first tick: the loop rig's ship, spawned docked, released by nobody. EndPlay
	 * puts it back to -1 when a world ends.
	 */
	TAutoConsoleVariable<int32> CVarDock(
		TEXT("sf.Dock"), -1,
		TEXT("1: dock, if the approach is inside the window. 0: undock. 2: undock and dock again after CaptureSeconds (the headless cycle). -1: no command.\n")
		TEXT("Acted on when the value CHANGES, so this is a keypress and not a state."),
		ECVF_Default);

	/**
	 * The negative control for finding F3, declared here for sf.Dock's reason.
	 *
	 * 0 brings back the undock that let go at the docked pose, with the hull's port face
	 * flush on the tunnel's end faces; a sideways thrust from there must reproduce F3 line
	 * for line, or the release is not what fixed it. Read at the moment of the undock
	 * command, so a release already under way finishes as it began.
	 */
	TAutoConsoleVariable<int32> CVarDockRelease(
		TEXT("sf.DockRelease"), 1,
		TEXT("1: undocking eases the hull out along the port's axis until it is clear of the station, then lets go. ")
		TEXT("0: let go at the docked pose, as before (the negative control for checkpoint 8's F3)."),
		ECVF_Default);

	/**
	 * Puts a DETACHED root exactly on a pose, even when the move is under a micrometre.
	 *
	 * SetActorTransform ends in InternalSetWorldLocationAndRotation, which changes nothing -- not
	 * the relative location, not ComponentToWorld, not the body -- when
	 * NewLocation.Equals(GetRelativeLocation()) and the rotation is unchanged
	 * (SceneComponent.cpp 3315-3317), and Equals is per axis within UE_KINDA_SMALL_NUMBER, 1e-4 cm
	 * (Vector.h 371, 1599-1602). An eased move's last step is that small at a high frame rate: at
	 * an uncapped ~2000 fps six of twelve releases lost theirs, 0.01-0.55 um short of the target,
	 * and a path that loses its last step loses its last deceleration, which the felt figure -- a
	 * second difference -- then reads as a velocity step (20.00 m/s2 in one run, over the 19.62
	 * limit, on a release that peaks at 17.6). So a step the engine declines is finished with the
	 * two calls that function makes once its test passes (SceneComponent.cpp 3319, 3341); the
	 * header asks callers not to use SetRelativeLocation_Direct, and no public move skips the test.
	 * TeleportPhysics makes the propagation run whatever its own 1e-8 test says (SceneComponent.cpp
	 * 815), so the pose reaches the kinematic body as every eased step does. Compared exactly, so a
	 * move the engine made is left alone.
	 *
	 * Location only, and only for a detached root, where relative is world: both callers hold the
	 * rotation constant at that step or set it through SetActorTransform first, and a rotation
	 * under the rotator's tolerance moves no point of the hull by a measurable amount.
	 */
	void TeleportDetachedRootExactly(AActor& Owner, const FTransform& Pose)
	{
		Owner.SetActorTransform(Pose, /*bSweep=*/false, /*OutHit=*/nullptr, ETeleportType::TeleportPhysics);

		USceneComponent* Root = Owner.GetRootComponent();
		if (Root && !Root->GetAttachParent() && Root->GetComponentLocation() != Pose.GetLocation())
		{
			Root->SetRelativeLocation_Direct(Pose.GetLocation());
			Root->UpdateComponentToWorld(EUpdateTransformFlags::None, ETeleportType::TeleportPhysics);
		}
	}
}

USFDockingComponent::USFDockingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Before physics, with the pilot's post. Nothing orders the two against each other
	// and nothing needs to: the worst case is one frame in which the post still holds a
	// demand for a hull that has just stopped simulating, and locking the array clears
	// that demand out of the readout in the same call.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

StarForge::Docking::FDockingLimits USFDockingComponent::BuildLimits() const
{
	// The one place degrees become radians for this component, as USFTurretComponent
	// does it. Two conversions of the same authored number is how a tolerance ends up
	// applied in degrees to a value in radians without anything crashing.
	StarForge::Docking::FDockingLimits Limits;
	Limits.DistanceMetres = DistanceMetres;
	Limits.SpeedMetresPerSec = SpeedMetresPerSec;
	Limits.ForwardAngleRad = FMath::DegreesToRadians(ForwardAngleDeg);
	Limits.UpAngleRad = FMath::DegreesToRadians(UpAngleDeg);
	return Limits;
}

UPrimitiveComponent* USFDockingComponent::GetHull() const
{
	AActor* Owner = GetOwner();
	return Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
}

void USFDockingComponent::ResolvePorts()
{
	if (bPortsResolved)
	{
		return;
	}
	bPortsResolved = true;

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	Port = Owner->FindComponentByClass<USFDockingPortComponent>();
	Thrusters = Owner->FindComponentByClass<USFThrusterArrayComponent>();
	Frame = Owner->FindComponentByClass<USFShipFrameComponent>();

	// Counted rather than taken on the first hit. One station is the slice's assumption,
	// not a fact the level enforces, and a second one would be chosen by iteration order --
	// which is to say by nothing. The rule this wants eventually is "the nearest free port",
	// and it is not written yet; until it is, the ambiguity is said out loud rather than
	// resolved by accident.
	int32 StationCount = 0;
	FString ChosenName;
	for (TActorIterator<ASFTestStation> It(GetWorld()); It; ++It)
	{
		if (StationCount == 0)
		{
			StationPort = It->GetDockingPort();
			ChosenName = It->GetName();
		}
		++StationCount;
	}
	if (StationCount > 1)
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Docking: [%s] %d stations in this level; '%s' was chosen by iteration order. ")
			TEXT("Boarding will need the nearest free port instead, and that rule is unwritten."),
			*Owner->GetName(), StationCount, *ChosenName);
	}
	bHasStation = StationPort != nullptr;

	if (!Port)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Docking component on [%s] found no port; this ship can never dock."), *Owner->GetName());
	}

	// The window's speed is read off the frame and nowhere else, so with no frame it is not a
	// figure at all. ASFTestShip always carries one; the component is spawnable on anything.
	if (!Frame)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Docking component on [%s] found no frame component; its approach speed cannot be measured and this ship can never dock."),
			*Owner->GetName());
	}

	// Not an error. L_FrameSpike has no station on purpose, and a ship that cannot dock
	// there is correct rather than broken -- it just has to say so when asked.
	UE_LOG(LogStarForge, Display,
		TEXT("Docking: [%s] %s, window %.1f m, %.1f m/s, fwd %.0f deg, up %.0f deg, capture %.1f s."),
		*Owner->GetName(),
		bHasStation ? TEXT("station found") : TEXT("no station in this level"),
		DistanceMetres, SpeedMetresPerSec, ForwardAngleDeg, UpAngleDeg, CaptureSeconds);
}

void USFDockingComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolvePorts();
}

void USFDockingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Reset when the world ends rather than seeded in BeginPlay: a value typed between two PIE
	// sessions, in the editor's console before Play, is a command for the next world and has to
	// reach it as a change. Not on Destroyed or RemovedFromWorld, where the world goes on and the
	// value is still its own keypress; not on Quit, where no world follows in this process.
	//
	// ECVF_SetByConsole, because the console and -ExecCmds both write at that priority and Set
	// refuses anything lower, keeping the old value with a LogConsoleManager warning
	// (ConsoleManager.cpp 274-308). -ExecCmds applies on the first frame, after BeginPlay, so a
	// headless sf.Dock 0 or 2 still arrives as a change.
	if (EndPlayReason == EEndPlayReason::EndPlayInEditor || EndPlayReason == EEndPlayReason::LevelTransition)
	{
		const int32 LeftOver = CVarDock.GetValueOnGameThread();
		if (LeftOver != -1)
		{
			CVarDock->Set(-1, ECVF_SetByConsole);
			UE_LOG(LogStarForge, Display,
				TEXT("Dock: sf.Dock %d reset to -1 as this world ends, so the next world does not take it for a keypress."),
				LeftOver);
		}
	}

	Super::EndPlay(EndPlayReason);
}

const TCHAR* USFDockingComponent::RefusalName(StarForge::Docking::EDockingRefusal Refusal)
{
	switch (Refusal)
	{
	case StarForge::Docking::EDockingRefusal::None: return TEXT("none");
	case StarForge::Docking::EDockingRefusal::Distance: return TEXT("distance");
	case StarForge::Docking::EDockingRefusal::Speed: return TEXT("speed");
	case StarForge::Docking::EDockingRefusal::Forward: return TEXT("forward");
	case StarForge::Docking::EDockingRefusal::Up: return TEXT("up");
	}
	return TEXT("?");
}

bool USFDockingComponent::CanDockNow() const
{
	return bHasStation
		&& State == ESFDockingState::Free
		&& LastRefusal == StarForge::Docking::EDockingRefusal::None;
}

void USFDockingComponent::UpdateApproach()
{
	const AActor* Owner = GetOwner();
	if (!Port || !StationPort || !Owner || !Frame)
	{
		// A refusal is still the right answer -- CanDockNow must not go green on an
		// instrument that read nothing -- but none of the four window conditions is what
		// happened, and the zeroed approach below is not a measurement. The flag records
		// that, so the refusal line can say it could not measure instead of reading the
		// stored reason out and printing "distance 0.0 m over 3.0": a verdict against a
		// figure that is INSIDE its own limit.
		//
		// A missing frame is the same case. The speed is read off the frame alone, and a 0.0
		// standing in for it passes the 1.0 m/s condition whatever the hull is doing: a green
		// window on a speed nobody read, and a capture that takes a hull closing at 5 m/s and,
		// since the run-in starts from rest, stops it inside one frame.
		bApproachMeasured = false;
		LastApproachSI = StarForge::Docking::FDockingApproach();
		LastPortOffsetShipLocalMetres = FVector::ZeroVector;
		LastRefusal = StarForge::Docking::EDockingRefusal::Distance;
		return;
	}

	// The station does not move, so the relative speed is the ship's own. Read from the
	// frame component rather than from Chaos: the frame differences its owner's transform
	// and so gives the same answer for a hull that is simulating and one that is not,
	// which is what makes this line work in every state. When a second moving carrier
	// arrives, this is the line that has to become a difference of two velocities.
	//
	// The frame samples in TG_PostPhysics and this runs pre-physics, so the figure is one
	// frame old: 1.6 cm of travel at the 1 m/s limit and sixty frames a second.
	const double SpeedSI = Frame->GetLinearVelocityMetersPerSec().Size();

	const StarForge::Docking::FDockingApproach InCentimetres =
		StarForge::Docking::ComputeDockingApproach(
			Port->GetPortTransform(), StationPort->GetPortTransform(), SpeedSI);

	// The one crossing of the unit boundary in this component. Everything the maths is
	// then asked is asked in SI.
	LastApproachSI = InCentimetres;
	LastApproachSI.Distance = StarForge::Units::CmToMetres(InCentimetres.Distance);

	// Where the station's port is from the ship's, in the ship's own axes. The cabin has
	// no windows and the station is unlit from outside, so a pilot flies the approach on
	// numbers alone, and a distance without a direction is not a number he can act on:
	// +Y in this triple is the side the hatch is on, which is the D key. Metres, because
	// it is read beside the distance and the two have to agree in unit at a glance.
	const FVector PortToPortWorldCm =
		StationPort->GetPortTransform().GetLocation() - Port->GetPortTransform().GetLocation();
	const FVector PortToPortShipCm =
		Owner->GetActorTransform().InverseTransformVectorNoScale(PortToPortWorldCm);
	LastPortOffsetShipLocalMetres = FVector(
		StarForge::Units::CmToMetres(PortToPortShipCm.X),
		StarForge::Units::CmToMetres(PortToPortShipCm.Y),
		StarForge::Units::CmToMetres(PortToPortShipCm.Z));

	LastRefusal = StarForge::Docking::CanDock(LastApproachSI, BuildLimits());
	bApproachMeasured = true;
}

void USFDockingComponent::LogRefusal() const
{
	if (!bApproachMeasured)
	{
		// Handled here and not in the enum: EDockingRefusal is the maths module's
		// vocabulary for the four conditions of the window, and a missing port is an
		// engine-side fact about this component, not a thing the approach did -- the same
		// separation HasStation() already keeps. Which one is missing, because "could not
		// measure" on its own sends the reader to check all four.
		UE_LOG(LogStarForge, Warning,
			TEXT("Dock: [%s] refused -- the approach could not be measured: no %s."),
			*GetNameSafe(GetOwner()),
			!Port ? TEXT("port on this ship")
				: !StationPort ? TEXT("station port")
				: !GetOwner() ? TEXT("owner")
				: TEXT("frame component on this ship, so its speed is unmeasured"));
		return;
	}

	FString Detail;
	switch (LastRefusal)
	{
	case StarForge::Docking::EDockingRefusal::Distance:
		Detail = FString::Printf(TEXT("distance %.1f m over %.1f"), LastApproachSI.Distance, DistanceMetres);
		break;
	case StarForge::Docking::EDockingRefusal::Speed:
		Detail = FString::Printf(TEXT("speed %.1f m/s over %.1f"), LastApproachSI.SpeedMetresPerSec, SpeedMetresPerSec);
		break;
	case StarForge::Docking::EDockingRefusal::Forward:
		Detail = FString::Printf(TEXT("forward %.1f deg over %.0f"),
			FMath::RadiansToDegrees(LastApproachSI.ForwardAngleRad), ForwardAngleDeg);
		break;
	case StarForge::Docking::EDockingRefusal::Up:
		Detail = FString::Printf(TEXT("up %.1f deg over %.0f"),
			FMath::RadiansToDegrees(LastApproachSI.UpAngleRad), UpAngleDeg);
		break;
	default:
		Detail = TEXT("nothing, which should be impossible here");
		break;
	}

	// One line per command, and it names WHICH condition and by how much. A refusal a
	// pilot cannot act on is a refusal that sends him to correct the wrong thing.
	UE_LOG(LogStarForge, Warning, TEXT("Dock: [%s] refused -- %s"), *GetNameSafe(GetOwner()), *Detail);
}

bool USFDockingComponent::EnterCapture()
{
	AActor* Owner = GetOwner();
	UPrimitiveComponent* Hull = GetHull();
	if (!Owner || !Hull || !Port || !StationPort)
	{
		// Reaching this means a port or the owner went away between ResolvePorts and here,
		// which no run can currently do -- hence an error rather than a refusal. It names what
		// was missing, because "the capture did not start" on its own sends the reader to check
		// all four.
		UE_LOG(LogStarForge, Error,
			TEXT("Dock: [%s] capture did not start -- no %s."),
			*GetNameSafe(Owner),
			!Owner ? TEXT("owner")
				: !Hull ? TEXT("hull")
				: !Port ? TEXT("port on this ship")
				: TEXT("station port"));
		return false;
	}

	// Order matters and is this: lock, clear, stop simulating, then take the two ends.
	//
	// Locked first so that nothing can apply a wrench to a body that is about to become
	// kinematic; cleared immediately after so the readout stops reporting the last demand
	// in the same frame rather than a frame later, in which it would print live thrust on
	// a ship that has begun docking.
	if (Thrusters)
	{
		Thrusters->SetLocked(true);
		Thrusters->ClearDemand();
	}

	Hull->SetSimulatePhysics(false);

	CaptureStart = Owner->GetActorTransform();

	// The port's transform relative to the hull, taken as world-relative-to-actor rather
	// than through GetRelativeTransform(). The two are equal today, because the port is
	// attached straight to the root; they stop being equal the moment anything is
	// inserted between them, and nothing would announce that. This form asks the question
	// the maths actually needs: where is the port in the hull's frame.
	const FTransform PortLocalToHull =
		Port->GetPortTransform().GetRelativeTransform(Owner->GetActorTransform());

	CaptureTarget = StarForge::Docking::TargetHullPose(StationPort->GetPortTransform(), PortLocalToHull);

	CaptureElapsedSeconds = 0.0;
	State = ESFDockingState::Capturing;

	return true;
}

void USFDockingComponent::AdvanceCapture(double DeltaSeconds)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	CaptureElapsedSeconds += DeltaSeconds;

	const double Alpha = (CaptureSeconds > UE_DOUBLE_SMALL_NUMBER)
		? CaptureElapsedSeconds / CaptureSeconds
		: 1.0;

	if (Alpha >= 1.0)
	{
		CompleteCapture();
		return;
	}

	// Eased, not linear, though it does not buy as much as it looks. The frame component
	// differences its owner's position twice to get acceleration, and a linear run-in
	// steps the velocity from zero to its full value inside one frame: over three metres
	// in a second that is 180 m/s^2 at sixty frames. Smoothstep peaks at 6d/T^2 instead
	// -- 23.0 m/s^2 at the window's edge once the port's 4.80 m lever arm is counted into
	// d, which is still over spec 6's 19.62 ceiling. See CaptureSeconds for the whole
	// arithmetic and for why that is accepted rather than fixed.
	const double Eased = Alpha * Alpha * (3.0 - 2.0 * Alpha);

	// A teleport, like the carry itself. The hull is kinematic and the occupant rides it
	// through the frame's transform difference; a swept move here would drag the whole
	// ship against its own station.
	Owner->SetActorTransform(
		StarForge::Docking::CapturePose(CaptureStart, CaptureTarget, Eased),
		/*bSweep=*/false, /*OutHit=*/nullptr, ETeleportType::TeleportPhysics);
}

void USFDockingComponent::CompleteCapture()
{
	AActor* Owner = GetOwner();
	if (!Owner || !StationPort)
	{
		// Terminal if it is ever reached: the hull stays not simulating, not attached and mid
		// pose, and no path leads out of Capturing again. Left that way on purpose -- nothing
		// in the slice can destroy or stream out the station, and a recovery no run can reach
		// is mechanism nobody could test.
		return;
	}

	// Exactly the target, from the maths, not the last interpolated step. This pose is
	// what the attachment freezes for as long as the ship stays docked -- and "exactly" has
	// to be enforced: a last step under a micrometre is declined by the scene component, which
	// would dock the hull up to 1 um off its pose and lose the run-in's last deceleration
	// (see TeleportDetachedRootExactly).
	TeleportDetachedRootExactly(*Owner, CaptureTarget);

	// Attached to the socket, keeping the world transform we have just set. From here the
	// relative velocity is zero by construction rather than by convergence -- which is
	// exactly what spec 8 asks of the handover, and what a physics constraint could not
	// give.
	Owner->GetRootComponent()->AttachToComponent(
		StationPort, FAttachmentTransformRules::KeepWorldTransform);

	State = ESFDockingState::Docked;
	CaptureElapsedSeconds = CaptureSeconds;

	UE_LOG(LogStarForge, Display, TEXT("Dock: [%s] docked."), *Owner->GetName());
}

bool USFDockingComponent::RequestDock()
{
	if (State == ESFDockingState::Releasing)
	{
		UE_LOG(LogStarForge, Display, TEXT("Dock: [%s] ignored -- the ship is releasing."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	if (State != ESFDockingState::Free)
	{
		// Said out loud for the reason Toggle gives: a command that does nothing is otherwise
		// indistinguishable from one that never arrived. Edge-triggered, so this is one line
		// per press and cannot become a torrent.
		UE_LOG(LogStarForge, Display, TEXT("Dock: [%s] ignored -- the ship is already %s."),
			*GetNameSafe(GetOwner()),
			State == ESFDockingState::Docked ? TEXT("docked") : TEXT("capturing"));
		return false;
	}

	if (!bHasStation)
	{
		UE_LOG(LogStarForge, Warning, TEXT("Dock: [%s] refused -- no station"), *GetNameSafe(GetOwner()));
		return false;
	}

	UpdateApproach();

	if (LastRefusal != StarForge::Docking::EDockingRefusal::None)
	{
		LogRefusal();
		return false;
	}

	// The line below is read as a commitment -- the F key and any automation take this
	// bool for one -- so it is printed only by a capture that actually began.
	if (!EnterCapture())
	{
		return false;
	}

	UE_LOG(LogStarForge, Display,
		TEXT("Dock: [%s] capturing, %.1f m, %.1f m/s, fwd %.1f deg, up %.1f deg"),
		*GetNameSafe(GetOwner()),
		LastApproachSI.Distance, LastApproachSI.SpeedMetresPerSec,
		FMath::RadiansToDegrees(LastApproachSI.ForwardAngleRad),
		FMath::RadiansToDegrees(LastApproachSI.UpAngleRad));

	return true;
}

bool USFDockingComponent::RequestUndock()
{
	// Docked and nothing else. A capture is not cancellable (see Toggle), and letting go
	// of a hull half way between two poses would hand Chaos a body nobody positioned.
	// Refused out loud, as in RequestDock: silence here reads exactly like an unbound key.
	if (State != ESFDockingState::Docked)
	{
		UE_LOG(LogStarForge, Display, TEXT("Dock: [%s] undock ignored -- the ship is %s."),
			*GetNameSafe(GetOwner()),
			State == ESFDockingState::Free ? TEXT("not docked")
				: State == ESFDockingState::Releasing ? TEXT("already releasing")
				: TEXT("mid-capture"));
		return false;
	}

	AActor* Owner = GetOwner();
	UPrimitiveComponent* Hull = GetHull();
	if (!Owner || !Hull)
	{
		return false;
	}

	if (CVarDockRelease.GetValueOnGameThread() == 0)
	{
		// Detach FIRST, then simulate. SetSimulatePhysics(true) detaches an attached
		// component itself and says so in its own documentation (PrimitiveComponent.h
		// 1648-1656), and stopping simulation does not put it back -- which is also why
		// docking attaches explicitly. Doing the detach here means the code states what
		// happens instead of depending on a side effect that a later engine version could
		// reasonably drop.
		Owner->GetRootComponent()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		LetGo();
		return true;
	}

	if (!StationPort)
	{
		// Docked implies a station port -- both ways into Docked require one -- so this is
		// EnterCapture's error, not a refusal. The ship stays docked rather than being let go
		// flush against a station nobody can measure.
		UE_LOG(LogStarForge, Error, TEXT("Dock: [%s] release did not start -- no station port."),
			*Owner->GetName());
		return false;
	}

	// The standoff, measured on the two bodies as they are. Centimetres in, metres into the
	// maths, one crossing each way. The hull is its root box, taken as the sphere through its
	// corners so the answer does not depend on which way the pilot turns first; the station
	// is every one of its primitives that takes part in physics. The interior volumes are
	// query only and are left out: a hull may stand in a volume, it may not stand in a wall.
	const UBoxComponent* HullBox = Cast<UBoxComponent>(Hull);
	const double HullRadiusCm = HullBox ? HullBox->GetScaledBoxExtent().Size() : Hull->Bounds.SphereRadius;

	// The panels are boxed in the station's own frame, not the world's: their world bounds
	// grow with the station's yaw, and a place can yaw it. Rigid, so the box is in world
	// lengths and the hull's radius means the same thing inside it.
	const AActor* Station = StationPort->GetOwner();
	const FTransform StationFrameCm(Station->GetActorQuat(), Station->GetActorLocation());

	FBox StationLocalBoundsCm(ForceInit);
	TInlineComponentArray<UPrimitiveComponent*> StationPrimitives(Station);
	for (const UPrimitiveComponent* Primitive : StationPrimitives)
	{
		if (Primitive && CollisionEnabledHasPhysics(Primitive->GetCollisionEnabled()))
		{
			StationLocalBoundsCm += Primitive->CalcBounds(
				Primitive->GetComponentTransform().GetRelativeTransform(StationFrameCm)).GetBox();
		}
	}

	const auto ToMetres = [](const FVector& Cm)
	{
		return FVector(
			StarForge::Units::CmToMetres(Cm.X),
			StarForge::Units::CmToMetres(Cm.Y),
			StarForge::Units::CmToMetres(Cm.Z));
	};

	const FBox StationLocalBoundsMetres = StationLocalBoundsCm.IsValid
		? FBox(ToMetres(StationLocalBoundsCm.Min), ToMetres(StationLocalBoundsCm.Max))
		: FBox(ForceInit);

	const FVector AwayWorld = StationPort->GetPortTransform().GetUnitAxis(EAxis::X);

	const double StandoffMetres = StarForge::Docking::ReleaseStandoff(
		ToMetres(Hull->GetComponentLocation()),
		StarForge::Units::CmToMetres(HullRadiusCm),
		AwayWorld,
		FTransform(StationFrameCm.GetRotation(), ToMetres(StationFrameCm.GetTranslation())),
		StationLocalBoundsMetres);
	const double PeakMetresPerSecSq =
		StarForge::Docking::SmoothstepPeakAcceleration(StandoffMetres, CaptureSeconds);

	// Detached, but not simulating: the hull stays kinematic and the array stays locked, as
	// they have been since the capture, so the pilot's post stands aside for the whole
	// release exactly as it does during a capture.
	Owner->GetRootComponent()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

	ReleaseStart = Owner->GetActorTransform();
	ReleaseTarget = ReleaseStart;
	ReleaseTarget.AddToTranslation(AwayWorld * StarForge::Units::MetresToCm(StandoffMetres));

	CaptureElapsedSeconds = 0.0;
	bReleaseArrived = false;
	State = ESFDockingState::Releasing;

	// A non-positive CaptureSeconds is a one-frame jump, and the maths answers the largest
	// double for it; printed as a number that would be three hundred digits of nothing.
	const bool bInstant = PeakMetresPerSecSq >= TNumericLimits<double>::Max();
	UE_LOG(LogStarForge, Display,
		TEXT("Dock: [%s] releasing, %.2f m along the port axis over %.1f s, peak %s."),
		*Owner->GetName(), StandoffMetres, CaptureSeconds,
		bInstant ? TEXT("unbounded (a jump in one frame)")
			: *FString::Printf(TEXT("%.1f m/s2"), PeakMetresPerSecSq));

	// Spec 8's re-docking relies on a released ship standing inside its own window; sf.Dock
	// 2's cycle and a pilot pressing F twice both assume it.
	if (StandoffMetres > DistanceMetres)
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Dock: [%s] the release stands the ship %.2f m out, outside its own %.1f m docking window; it cannot dock again without flying back in."),
			*Owner->GetName(), StandoffMetres, DistanceMetres);
	}
	if (PeakMetresPerSecSq >= StarForge::ShipFrame::GravPlateLimitMetersPerSecSq)
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Dock: [%s] the release peaks at %s, at or over the %.2f m/s2 grav-plate limit; a man standing aboard will be knocked down."),
			*Owner->GetName(),
			bInstant ? TEXT("an unbounded acceleration") : *FString::Printf(TEXT("%.1f m/s2"), PeakMetresPerSecSq),
			StarForge::ShipFrame::GravPlateLimitMetersPerSecSq);
	}

	return true;
}

void USFDockingComponent::AdvanceRelease(double DeltaSeconds)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (bReleaseArrived)
	{
		// The tick after arrival, with the hull standing on ReleaseTarget and nothing moving
		// it: the body starts simulating at rest, clear of the station, with nothing under its
		// port face to drag across.
		LetGo();

		if (bAutoDockAfterRelease)
		{
			bAutoDockAfterRelease = false;
			SecondsUntilAutoDock = CaptureSeconds;
		}
		return;
	}

	CaptureElapsedSeconds += DeltaSeconds;

	const double Alpha = (CaptureSeconds > UE_DOUBLE_SMALL_NUMBER)
		? CaptureElapsedSeconds / CaptureSeconds
		: 1.0;

	if (Alpha >= 1.0)
	{
		// Exactly the target, as a kinematic teleport like every step before it, and the let-go
		// on the NEXT tick. Both used to happen here in one pre-physics tick, and the teleport
		// was sometimes not made at all. Measured with sf.FeltDebug, the hull root to 1e-9 cm
		// every frame, over twelve headless releases at an uncapped ~2000 fps: in six this step
		// moved the hull 1.04-2.00 um and it ended exactly on ReleaseTarget; in the other six it
		// did not move, 0.01-0.55 um short -- the scene component's 1e-4 cm dead band, which
		// TeleportDetachedRootExactly says how it is closed. (Chaos's own 1 um leniency,
		// ParticleDirtyFlags.h 94-97, serves only the rewind's desync test, RewindData.h 845, and a
		// kinematic SetGlobalPose has no early-out, ChaosEngineInterface.cpp 2544.) Relative is
		// world here because RequestUndock detached the root, and the rotation is ReleaseStart's
		// throughout, so the location is the whole pose.
		//
		// The let-go waits so that the switch to simulation never shares a tick with a move. The
		// body goes dynamic from the pose it already holds -- SetSimulatePhysics pushes no
		// transform (BodyInstance.cpp 2591) -- and there is nothing for the sync to pull back; nor
		// could it pull back less than 1 um, because OnSyncBodies skips a pose within
		// UE_KINDA_SMALL_NUMBER of the component's (PhysScene_Chaos.cpp 2483). State stays
		// Releasing through this tick, so Toggle, RequestDock, sf.Dock 2's countdown and the
		// readouts see nothing Free early. What moves in recorded figures: "undocked." comes one
		// frame later, f61 for f60 at 60 fps and f402 for f401 at 400
		// (2026-09-25-f3fix-dockcycle-*.log), and sf.Dock 2's re-capture with it, f121 for f120
		// and f803 for f802.
		TeleportDetachedRootExactly(*Owner, ReleaseTarget);

		bReleaseArrived = true;
		return;
	}

	// Eased for the capture's reason, and in translation only: the rotation is the docked
	// one throughout, so there is no angular term and the peak is the 6d/T^2 logged at the
	// start. A teleport, like the capture's, because the hull is kinematic and the occupant
	// rides it through the frame's transform difference.
	const double Eased = Alpha * Alpha * (3.0 - 2.0 * Alpha);

	FTransform Pose = ReleaseStart;
	Pose.SetTranslation(FMath::Lerp(ReleaseStart.GetTranslation(), ReleaseTarget.GetTranslation(), Eased));

	Owner->SetActorTransform(Pose, /*bSweep=*/false, /*OutHit=*/nullptr, ETeleportType::TeleportPhysics);
}

void USFDockingComponent::LetGo()
{
	AActor* Owner = GetOwner();
	UPrimitiveComponent* Hull = GetHull();
	if (!Owner || !Hull)
	{
		return;
	}

	Hull->SetSimulatePhysics(true);

	// Zeroed after the body is live, because a kinematic body has no velocity to set.
	// Spec 8: relative velocity after docking is zero, so it is zero at the moment of
	// letting go too -- the ship does not spring off the station.
	Hull->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Hull->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);

	if (Thrusters)
	{
		// The array reads its mass and inertia back from Chaos, and a hull that spent the
		// start of the session kinematic may have handed it a kinematic body's properties.
		// This is the instant those numbers start meaning something again, and nobody is
		// flying in it.
		Thrusters->Rebuild();
		Thrusters->SetLocked(false);
	}

	State = ESFDockingState::Free;
	CaptureElapsedSeconds = 0.0;

	UE_LOG(LogStarForge, Display, TEXT("Dock: [%s] undocked."), *Owner->GetName());
}

void USFDockingComponent::Toggle()
{
	switch (State)
	{
	case ESFDockingState::Free:
		RequestDock();
		break;
	case ESFDockingState::Docked:
		RequestUndock();
		break;
	case ESFDockingState::Releasing:
		// Not cancellable either, for the capture's reason: half way out the hull is
		// kinematic, detached and between two poses.
		UE_LOG(LogStarForge, Display, TEXT("Dock: [%s] release in progress; the command is ignored."),
			*GetNameSafe(GetOwner()));
		break;
	default:
		// A capture is not cancellable. Half way through, the hull is between two poses
		// with nothing owning it: not simulating, not attached, and not where either side
		// expects. Said out loud, because a key that does nothing is otherwise
		// indistinguishable from a key that is not bound.
		UE_LOG(LogStarForge, Display, TEXT("Dock: [%s] capture in progress; the command is ignored."),
			*GetNameSafe(GetOwner()));
		break;
	}
}

void USFDockingComponent::DockImmediately()
{
	ResolvePorts();

	if (!Port || !StationPort)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Dock: [%s] cannot spawn docked -- %s. The ship will start adrift."),
			*GetNameSafe(GetOwner()),
			Port ? TEXT("no station port") : TEXT("no port on this ship"));
		return;
	}

	// No run-in: the loop rig must not have a single frame in which the ship is adrift,
	// because the pawn's first tick has to find him standing in a docked cabin. Completing
	// a capture that never started would attach a still-simulating hull and call it docked,
	// which is the claim RequestDock has just stopped making; EnterCapture has already said
	// what was missing.
	if (!EnterCapture())
	{
		return;
	}

	CompleteCapture();
}

void USFDockingComponent::PollConsoleCommand(double DeltaSeconds)
{
	if (!bConsoleControlled)
	{
		return;
	}

	const int32 Command = CVarDock.GetValueOnGameThread();

	if (Command != LastDockCommand)
	{
		LastDockCommand = Command;

		switch (Command)
		{
		case 1:
			RequestDock();
			break;
		case 0:
			RequestUndock();
			break;
		case 2:
			// One capture after the ship is FREE: at once when sf.DockRelease 0 has let it go
			// in this call, at the end of the release otherwise.
			if (RequestUndock())
			{
				if (State == ESFDockingState::Free)
				{
					SecondsUntilAutoDock = CaptureSeconds;
				}
				else
				{
					bAutoDockAfterRelease = true;
				}
			}
			break;
		default:
			// -1 and anything unrecognised: no command. Deliberately silent -- this runs
			// every frame, and a line here would be sixty a second on the default value.
			break;
		}
	}

	if (SecondsUntilAutoDock > 0.0)
	{
		SecondsUntilAutoDock -= DeltaSeconds;
		if (SecondsUntilAutoDock <= 0.0)
		{
			SecondsUntilAutoDock = -1.0;
			RequestDock();
		}
	}
}

void USFDockingComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	PollConsoleCommand(DeltaTime);

	switch (State)
	{
	case ESFDockingState::Free:
		// Every frame, whether or not anybody intends to dock. The readout has to be able
		// to say CAN DOCK before the key is pressed, or a pilot learns whether he was in
		// the window only by being refused.
		UpdateApproach();
		break;
	case ESFDockingState::Capturing:
		AdvanceCapture(DeltaTime);
		break;
	case ESFDockingState::Releasing:
		AdvanceRelease(DeltaTime);
		break;
	default:
		break;
	}
}
