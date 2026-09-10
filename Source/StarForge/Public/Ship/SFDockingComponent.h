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
 * Three states and one step between each pair. Free to Docked always goes through
 * Capturing; Docked to Free is one step, because undocking is letting go.
 */
UENUM()
enum class ESFDockingState : uint8
{
	Free,
	Capturing,
	Docked
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
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFDockingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFDockingComponent();

	virtual void BeginPlay() override;
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
	 * How long the run-in takes, in seconds.
	 *
	 * Not free to shorten. The run-in is eased, so its peak acceleration is 6d/T^2 for a
	 * path of d; at the window's edge of 3 m and one second that is 18.0 m/s^2, under the
	 * 19.62 grav-plate ceiling of spec 6 by eight per cent. Below about 0.958 s -- or with
	 * DistanceMetres above about 3.27 -- the capture itself starts knocking standing
	 * occupants off their feet.
	 */
	UPROPERTY(EditAnywhere, Category = "StarForge|Docking")
	double CaptureSeconds = 1.0;

	/** The window in the form the maths wants: SI and radians. One conversion, one place. */
	StarForge::Docking::FDockingLimits BuildLimits() const;

	/** Starts a capture if the approach is inside the window. False, with a reason logged, if not. */
	bool RequestDock();

	/** Lets go: detach, simulate, zero the velocities, unlock. False if not docked. */
	bool RequestUndock();

	/** What the F key does: dock if free, undock if docked, ignore during a capture. */
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

private:
	/** Idempotent; called from BeginPlay and from DockImmediately, whichever comes first. */
	void ResolvePorts();

	/** Measures the approach and stores it in SI. Only meaningful while Free. */
	void UpdateApproach();

	/** Locks, stops simulating, and takes the two ends of the run-in. */
	void EnterCapture();

	/** Sets the exact mated pose, attaches, and says so. */
	void CompleteCapture();

	void AdvanceCapture(double DeltaSeconds);

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

	FTransform CaptureStart = FTransform::Identity;
	FTransform CaptureTarget = FTransform::Identity;
	double CaptureElapsedSeconds = 0.0;

	/** Last value read from sf.Dock, so the command acts on an edge and not on a level. */
	int32 LastDockCommand = -1;

	/** Countdown for sf.Dock 2's re-dock; negative when no cycle is running. */
	double SecondsUntilAutoDock = -1.0;
};
