#include "Ship/SFDockingComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/EngineTypes.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFDockingPortComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFTestStation.h"
#include "Ship/SFThrusterArrayComponent.h"
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
	 */
	TAutoConsoleVariable<int32> CVarDock(
		TEXT("sf.Dock"), -1,
		TEXT("1: dock, if the approach is inside the window. 0: undock. 2: undock and dock again after CaptureSeconds (the headless cycle). -1: no command.\n")
		TEXT("Acted on when the value CHANGES, so this is a keypress and not a state."),
		ECVF_Default);
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

	// One station in the slice, so the first one found is the one. A second would need a
	// choice, and a choice needs a rule nobody has written yet -- said here rather than
	// left for whoever adds the second station to discover.
	for (TActorIterator<ASFTestStation> It(GetWorld()); It; ++It)
	{
		StationPort = It->GetDockingPort();
		break;
	}
	bHasStation = StationPort != nullptr;

	if (!Port)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Docking component found no port on its owner; this ship can never dock."));
	}

	// Not an error. L_FrameSpike has no station on purpose, and a ship that cannot dock
	// there is correct rather than broken -- it just has to say so when asked.
	UE_LOG(LogStarForge, Display,
		TEXT("Docking: %s, window %.1f m, %.1f m/s, fwd %.0f deg, up %.0f deg, capture %.1f s."),
		bHasStation ? TEXT("station found") : TEXT("no station in this level"),
		DistanceMetres, SpeedMetresPerSec, ForwardAngleDeg, UpAngleDeg, CaptureSeconds);
}

void USFDockingComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolvePorts();
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
	if (!Port || !StationPort || !Owner)
	{
		// A refusal is still the right answer -- CanDockNow must not go green on an
		// instrument that read nothing -- but none of the four window conditions is what
		// happened, and the zeroed approach below is not a measurement. The flag records
		// that, so the refusal line can say it could not measure instead of reading the
		// stored reason out and printing "distance 0.0 m over 3.0": a verdict against a
		// figure that is INSIDE its own limit.
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
	const double SpeedSI = Frame ? Frame->GetLinearVelocityMetersPerSec().Size() : 0.0;

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
		// measure" on its own sends the reader to check all three.
		UE_LOG(LogStarForge, Warning,
			TEXT("Dock: refused -- the approach could not be measured: no %s."),
			!Port ? TEXT("port on this ship")
				: !StationPort ? TEXT("station port")
				: TEXT("owner"));
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
	UE_LOG(LogStarForge, Warning, TEXT("Dock: refused -- %s"), *Detail);
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
			TEXT("Dock: capture did not start -- no %s."),
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
	// what the attachment freezes for as long as the ship stays docked.
	Owner->SetActorTransform(
		CaptureTarget, /*bSweep=*/false, /*OutHit=*/nullptr, ETeleportType::TeleportPhysics);

	// Attached to the socket, keeping the world transform we have just set. From here the
	// relative velocity is zero by construction rather than by convergence -- which is
	// exactly what spec 8 asks of the handover, and what a physics constraint could not
	// give.
	Owner->GetRootComponent()->AttachToComponent(
		StationPort, FAttachmentTransformRules::KeepWorldTransform);

	State = ESFDockingState::Docked;
	CaptureElapsedSeconds = CaptureSeconds;

	UE_LOG(LogStarForge, Display, TEXT("Dock: docked."));
}

bool USFDockingComponent::RequestDock()
{
	if (State != ESFDockingState::Free)
	{
		// Said out loud for the reason Toggle gives: a command that does nothing is otherwise
		// indistinguishable from one that never arrived. Edge-triggered, so this is one line
		// per press and cannot become a torrent.
		UE_LOG(LogStarForge, Display, TEXT("Dock: ignored -- the ship is already %s."),
			State == ESFDockingState::Docked ? TEXT("docked") : TEXT("capturing"));
		return false;
	}

	if (!bHasStation)
	{
		UE_LOG(LogStarForge, Warning, TEXT("Dock: refused -- no station"));
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
		TEXT("Dock: capturing, %.1f m, %.1f m/s, fwd %.1f deg, up %.1f deg"),
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
		UE_LOG(LogStarForge, Display, TEXT("Dock: undock ignored -- the ship is %s."),
			State == ESFDockingState::Free ? TEXT("not docked") : TEXT("mid-capture"));
		return false;
	}

	AActor* Owner = GetOwner();
	UPrimitiveComponent* Hull = GetHull();
	if (!Owner || !Hull)
	{
		return false;
	}

	// Detach FIRST, then simulate. SetSimulatePhysics(true) detaches an attached
	// component itself and says so in its own documentation (PrimitiveComponent.h
	// 1648-1656), and stopping simulation does not put it back -- which is also why
	// docking attaches explicitly. Doing the detach here means the code states what
	// happens instead of depending on a side effect that a later engine version could
	// reasonably drop.
	Owner->GetRootComponent()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

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

	UE_LOG(LogStarForge, Display, TEXT("Dock: undocked."));
	return true;
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
	default:
		// A capture is not cancellable. Half way through, the hull is between two poses
		// with nothing owning it: not simulating, not attached, and not where either side
		// expects. Said out loud, because a key that does nothing is otherwise
		// indistinguishable from a key that is not bound.
		UE_LOG(LogStarForge, Display, TEXT("Dock: capture in progress; the command is ignored."));
		break;
	}
}

void USFDockingComponent::DockImmediately()
{
	ResolvePorts();

	if (!Port || !StationPort)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Dock: cannot spawn docked -- %s. The ship will start adrift."),
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
			if (RequestUndock())
			{
				SecondsUntilAutoDock = CaptureSeconds;
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
	default:
		break;
	}
}
