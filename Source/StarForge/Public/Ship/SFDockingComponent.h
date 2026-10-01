#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DockingMath.h"
#include "SFDockingComponent.generated.h"

class UPrimitiveComponent;
class USFDockingPortComponent;
class USFShipFrameComponent;
class USFThrusterArrayComponent;

/**
 * Four states, and every change between Free and Docked goes through a run of its own:
 * Free -> Capturing -> Docked on the way in, Docked -> Releasing -> Free on the way out.
 *
 * Undocking used to be letting go where the ship lay, and that is finding F3 of checkpoint
 * 8: docked, the hull's port face lies flush on the tunnel's end faces, and a hull let go
 * there and thrust sideways drags that face across them until Chaos resolves a false
 * edge-edge penetration in one step and knocks a standing man down. The release eases the
 * hull straight out along the port's axis until it is clear, and only then lets go.
 * Releasing is appended rather than placed beside Capturing so the existing values keep
 * their numbers.
 *
 * Two paths skip a run on purpose, and anything that waits for a run to finish must know
 * them: sf.DockRelease 0, the negative control, goes Docked -> Free inside one call, and
 * DockImmediately goes through Capturing to Docked inside one call with no run-in.
 */
UENUM()
enum class ESFDockingState : uint8
{
	Free,
	Capturing,
	Docked,
	Releasing
};

/**
 * Docking, on the moving side.
 *
 * It lives on the ship because the ship is what moves. The station has a port and no
 * component of this kind at all: giving both sides a mechanism would give the pair two
 * opinions about who is attached to whom.
 *
 * Docking is commanded, not automatic. Spec 8's first paragraphs read as automatic and
 * were decided otherwise on 2026-09-10: the approach conditions open a WINDOW, and one
 * key from the pilot's seat -- F, or sf.Dock for a headless run -- goes through it. With
 * one station in the slice, docking by accident while flying past the port is the only
 * way to spoil a manual checkpoint.
 *
 * "Docked" means a kinematic attachment. The hull stops simulating, the thrusters are
 * locked, the pose is run in over a second and the root is attached to the station's
 * socket. The relative velocity afterwards is zero BY CONSTRUCTION, which is what spec 8
 * requires of the airlock handover, rather than an approximation of zero. A physics
 * constraint was rejected for exactly that: its solver jitters, the residual velocity is
 * never zero, and "visually unnoticeable" stops being true.
 *
 * Undocking is the capture played backwards. The hull is detached but stays kinematic and
 * locked, so the pilot's post stands aside exactly as it does during a capture, and it is
 * eased out along the station port's outward axis by the standoff that puts a sphere round
 * the hull clear of the station's physical panels -- over CaptureSeconds, with no constant
 * of its own. Only then does it simulate, from rest, and the pilot gets the stick. The
 * standoff is derived from the two bodies' geometry, not authored: 2.93 m on the rig,
 * inside the 3.0 m window, so the ship can dock again without flying anywhere.
 * sf.DockRelease 0 lets go at the docked pose, as before -- the negative control for F3.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFDockingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFDockingComponent();

	virtual void BeginPlay() override;

	/** Puts sf.Dock back to -1 when the world ends, so the next world does not replay it. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Spec 8's starting tolerances. Ship data, so they are authored, not compiled in. */
	UPROPERTY(EditAnywhere, Category = "StarForge|Docking")
	double DistanceMetres = 3.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Docking")
	double SpeedMetresPerSec = 1.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Docking")
	double ForwardAngleDeg = 10.0;

	UPROPERTY(EditAnywhere, Category = "StarForge|Docking")
	double UpAngleDeg = 10.0;

	/**
	 * How long the run-in takes, in seconds. The release takes the same time, being the
	 * run-in played backwards, and lets go on the tick after it arrives; unlike the run-in it
	 * moves in a straight line with no turn, and its peak 6d/T^2 is logged and checked
	 * against the grav-plate ceiling when it starts.
	 *
	 * The run-in is eased, so its peak acceleration is 6d/T^2 -- an order of magnitude
	 * below a linear run-in, which would step the velocity from zero to full inside one
	 * frame (180 m/s^2 over three metres at sixty frames). It does NOT bring the capture
	 * under the 19.62 m/s^2 grav-plate ceiling of spec 6, and the arithmetic is written out
	 * here rather than rounded off, because an earlier version of this comment claimed a
	 * margin that does not exist:
	 *
	 *   - d is the HULL's path, not the distance between the ports. The port sits
	 *     sqrt(400^2 + 265^2) = 4.80 m off the hull's centre, so turning through the
	 *     window's 10 degrees of allowed misalignment adds 2 * 4.80 * sin(5 deg) = 0.84 m.
	 *     At the window's edge d is 3.84 m and 6d/T^2 is 23.0 m/s^2.
	 *   - The angular term is 6 * 10 deg / T^2 = 1.047 rad/s^2, and it peaks in the SAME
	 *     frame: smoothstep's second derivative is largest at alpha 0 and alpha 1, not in
	 *     the middle. As a felt acceleration it is a figure PER RADIUS, so the radius goes
	 *     with it or the number means nothing: 4.19 m/s^2 four metres off the axis of the
	 *     turn, and four metres is not the far side of this cabin. A man in the corner
	 *     stands sqrt(2) * 4 = 5.66 m off the centreline and feels 5.9.
	 *   - The first frame is a step of its own: stopping the simulation kills the hull's
	 *     velocity in one frame, which at the window's 1 m/s limit is 60 m/s^2 at 60 fps.
	 *
	 * None of that is compensated, deliberately. The command comes from the pilot's seat and
	 * a strapped-in occupant cannot be knocked down; the slice has nobody else aboard.
	 * Deriving this figure from the actual path would be mechanism for a case the slice
	 * cannot reach -- the moment a standing passenger exists, it stops being optional.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Docking")
	double CaptureSeconds = 1.0;

	/** The window in the form the maths wants: SI and radians. One conversion, one place. */
	StarForge::Docking::FDockingLimits BuildLimits() const;

	/** Starts a capture if the approach is inside the window. False, with a reason logged, if not. */
	bool RequestDock();

	/**
	 * Starts the release: detach, ease out along the port's axis, and only then simulate,
	 * zero the velocities and unlock. With sf.DockRelease 0, lets go at once instead. False
	 * if not docked.
	 */
	bool RequestUndock();

	/** What the F key does: dock if free, undock if docked, ignore during a capture or release. */
	void Toggle();

	/**
	 * Docks with no run-in at all, for a ship that is spawned already attached.
	 *
	 * Used by the loop rig, which must not have a single frame in which the ship is
	 * adrift: the pawn's very first tick has to find him in a docked cabin. It resolves
	 * its own ports rather than relying on BeginPlay, because the game mode spawns and
	 * docks before the world has begun play at all.
	 */
	void DockImmediately();

	ESFDockingState GetState() const { return State; }
	bool IsFree() const { return State == ESFDockingState::Free; }
	bool IsDocked() const { return State == ESFDockingState::Docked; }

	/** False when no station was found at all -- a legal state, and not a refusal reason. */
	bool HasStation() const { return bHasStation; }

	/** True when a dock command issued this instant would be accepted. */
	bool CanDockNow() const;

	/** The last approach measured, with Distance already in METRES. */
	StarForge::Docking::FDockingApproach GetLastApproach() const { return LastApproachSI; }

	StarForge::Docking::EDockingRefusal GetLastRefusal() const { return LastRefusal; }

	/**
	 * From the ship's port to the station's, in the ship's own axes and in metres, as of
	 * the last approach. The cabin has no windows and the station is unlit from outside,
	 * so this triple is the only thing that tells a pilot which way to fly: +Y is the side
	 * the hatch is on. Zero when the ports are mated.
	 */
	FVector GetPortOffsetShipLocalMetres() const { return LastPortOffsetShipLocalMetres; }

	double GetCaptureSeconds() const { return CaptureSeconds; }
	double GetCaptureElapsedSeconds() const { return CaptureElapsedSeconds; }

	/** Lower case: these words are read inside a sentence, not scanned for in a log. */
	static const TCHAR* RefusalName(StarForge::Docking::EDockingRefusal Refusal);

	/**
	 * Whether this instance answers the unnamed sf.* family. See ESFShipCrewing.
	 *
	 * True by default, so a hull nobody told anything behaves exactly as it did when there
	 * was only one ship in the world -- which is what keeps both existing rigs unchanged.
	 */
	void SetConsoleControlled(bool bIn) { bConsoleControlled = bIn; }

private:
	bool bConsoleControlled = true;

	/** Idempotent; called from BeginPlay and from DockImmediately, whichever comes first. */
	void ResolvePorts();

	/** Measures the approach and stores it in SI. Only meaningful while Free. */
	void UpdateApproach();

	/**
	 * Locks, stops simulating, and takes the two ends of the run-in.
	 *
	 * False when it did not get that far, so that no caller announces a capture that never
	 * began: the bool RequestDock returns is read by a key and by automation.
	 */
	bool EnterCapture();

	/** Sets the exact mated pose, attaches, and says so. */
	void CompleteCapture();

	void AdvanceCapture(double DeltaSeconds);

	/**
	 * Eases the hull from ReleaseStart to ReleaseTarget, sets the target exactly on arrival,
	 * and lets go on the tick after. The arrival branch says why the let-go waits.
	 */
	void AdvanceRelease(double DeltaSeconds);

	/**
	 * Simulate, zero both velocities, rebuild and unlock the array, and say "undocked".
	 *
	 * One function for both ways out of Docked -- the end of a release and sf.DockRelease 0's
	 * instant let-go -- so the negative control cannot drift away from the thing it controls.
	 * The hull must already be detached.
	 */
	void LetGo();

	/** Acts on a CHANGE of sf.Dock, and runs the debug cycle's timer. */
	void PollConsoleCommand(double DeltaSeconds);

	void LogRefusal() const;

	/** The simulated body: the owner's root, same idiom as the thruster array's. */
	UPrimitiveComponent* GetHull() const;

	UPROPERTY()
	TObjectPtr<USFDockingPortComponent> Port;

	UPROPERTY()
	TObjectPtr<USFDockingPortComponent> StationPort;

	UPROPERTY()
	TObjectPtr<USFThrusterArrayComponent> Thrusters;

	UPROPERTY()
	TObjectPtr<USFShipFrameComponent> Frame;

	ESFDockingState State = ESFDockingState::Free;
	bool bHasStation = false;
	bool bPortsResolved = false;

	StarForge::Docking::FDockingApproach LastApproachSI;

	/** From the ship's port to the station's, ship axes, metres. See the getter. */
	FVector LastPortOffsetShipLocalMetres = FVector::ZeroVector;

	/**
	 * Starts at Distance rather than None: before the first measurement nothing is known,
	 * and None would mean "you may dock" -- the readout's green, on an instrument that
	 * has never read anything. The same defect as a drift of zero with no measured time.
	 */
	StarForge::Docking::EDockingRefusal LastRefusal = StarForge::Docking::EDockingRefusal::Distance;

	/**
	 * Whether the refusal above stands on a figure anything actually measured.
	 *
	 * False until the first complete pass of UpdateApproach, and false again whenever a
	 * port or the frame is missing and it returns early. The refusal it leaves behind is a
	 * real refusal -- nothing may dock on an unread instrument -- but it is not one of the
	 * four window conditions, and reporting it as one prints a figure nobody measured.
	 */
	bool bApproachMeasured = false;

	FTransform CaptureStart = FTransform::Identity;
	FTransform CaptureTarget = FTransform::Identity;

	/**
	 * Times whichever run is in progress, the capture or the release: both last
	 * CaptureSeconds, and the readouts print either through GetCaptureElapsedSeconds.
	 */
	double CaptureElapsedSeconds = 0.0;

	FTransform ReleaseStart = FTransform::Identity;
	FTransform ReleaseTarget = FTransform::Identity;

	/**
	 * The hull stands on ReleaseTarget and the next tick lets go. A flag rather than a second
	 * look at the elapsed time, because with a non-positive CaptureSeconds the release arrives
	 * on its first tick and "already past the end" would let go before the target was set.
	 */
	bool bReleaseArrived = false;

	/** Last value read from sf.Dock, so the command acts on an edge and not on a level. */
	int32 LastDockCommand = -1;

	/** Countdown for sf.Dock 2's re-dock; negative when no cycle is running. */
	double SecondsUntilAutoDock = -1.0;

	/**
	 * sf.Dock 2 asked for a re-dock and the ship is still releasing. The countdown starts
	 * when it is free, not at the command: counted from the command, a one-second release
	 * and a one-second countdown would ask for the dock on the release's arrival tick, one
	 * before the let-go, and be ignored as releasing.
	 */
	bool bAutoDockAfterRelease = false;
};
