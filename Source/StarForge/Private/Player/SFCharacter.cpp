#include "Player/SFCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Player/SFShipboardMovementComponent.h"
#include "EngineUtils.h"
#include "Ship/SFDockingComponent.h"
#include "Ship/SFInteriorVolumeComponent.h"
#include "Ship/SFPilotStationComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFShipStationComponent.h"
#include "Ship/SFTestShip.h"
#include "Ship/SFTestStation.h"
#include "ShipFrameMath.h"
#include "StarForgeLog.h"
#include "Units.h"

namespace
{
	/**
	 * Rebinds the occupant to a named frame regardless of where he is standing.
	 *
	 * The negative control for the handover, and the same trick as sf.ShipGravity 0 for
	 * the carry. With the ship docked it is a legitimate handover and everything stays
	 * quiet; with the ship undocked and under thrust it is a knowingly wrong binding, and
	 * the cross-check has to complain within ten frames and the carry has to stop shortly
	 * after. An instrument that stays calm through that is blind.
	 *
	 * Declared here because this is the file that reads it. Empty means "no command", and
	 * it acts on a change of the value rather than on its level.
	 */
	TAutoConsoleVariable<FString> CVarBindFrame(
		TEXT("sf.BindFrame"), TEXT(""),
		TEXT("ship|station: bind the occupant to that carrier's frame wherever he is standing. Empty: no command.\n")
		TEXT("A negative control. Bound to the wrong carrier in flight, the cross-check must warn and the carry must stop."),
		ECVF_Default);
}

ASFCharacter::ASFCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USFShipboardMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.0, 0.0, 64.0));
	Camera->bUsePawnControlRotation = true;

	bUseControllerRotationYaw = false;
}

USFShipboardMovementComponent* ASFCharacter::GetShipboardMovement() const
{
	return Cast<USFShipboardMovementComponent>(GetCharacterMovement());
}

void ASFCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (USFShipboardMovementComponent* Movement = GetShipboardMovement())
	{
		USFShipFrameComponent* FoundFrame = nullptr;

		// First, the engine's own answer. Free, and right at any settled moment.
		TArray<UPrimitiveComponent*> Overlaps;
		GetCapsuleComponent()->GetOverlappingComponents(Overlaps);
		for (UPrimitiveComponent* Component : Overlaps)
		{
			if (const USFInteriorVolumeComponent* Volume = Cast<USFInteriorVolumeComponent>(Component))
			{
				FoundFrame = Volume->GetOwnerFrame();
				break;
			}
		}

		// Second, the same question asked geometrically. Overlap bookkeeping at BeginPlay
		// depends on the order components registered, and "whose reference frame am I in"
		// is not allowed to depend on that. A point in a box is the same answer without
		// the timing.
		if (!FoundFrame)
		{
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				TArray<USFInteriorVolumeComponent*> Volumes;
				It->GetComponents<USFInteriorVolumeComponent>(Volumes);
				for (USFInteriorVolumeComponent* Volume : Volumes)
				{
					if (Volume->ContainsPoint(GetActorLocation()))
					{
						FoundFrame = Volume->GetOwnerFrame();
						break;
					}
				}
				if (FoundFrame)
				{
					break;
				}
			}
		}

		if (FoundFrame)
		{
			Movement->SetShipFrame(FoundFrame);
		}
		else
		{
			// Third and last: any frame at all, loudly. A man outside every volume still
			// has to be carried by something, and the warning is what says the binding was
			// a guess rather than an answer.
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				if (USFShipFrameComponent* Frame = It->FindComponentByClass<USFShipFrameComponent>())
				{
					Movement->SetShipFrame(Frame);
					UE_LOG(LogStarForge, Warning,
						TEXT("SFCharacter bound to %s by search, not by volume: he spawned outside every ")
						TEXT("interior volume. The binding is a guess."),
						*It->GetActorNameOrLabel());
					FoundFrame = Frame;
					break;
				}
			}
		}

		if (!FoundFrame)
		{
			// A rig that silently measures nothing is worse than one that fails.
			UE_LOG(LogStarForge, Warning,
				TEXT("SFCharacter: found no USFShipFrameComponent in the world. The occupant will ")
				TEXT("stand in world space while the ship flies away, and every drift reading will be ")
				TEXT("a zero that means nothing."));
		}
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
		{
			// ProcessViewRotation clamps roll and pitch to +/-89.9 degrees every tick.
			// The carry composes the ship's full rotation into the control rotation, so
			// under a 180 deg/s roll the roll saturates in about half a second and the
			// ship-local view is lost. An occupant riding a ship has no world horizon to
			// be kept level against.
			// The engine idiom for "unlimited" is 0 .. 359.999 -- how it leaves yaw by
			// default. Do NOT write -180 .. 180 here: FMath::ClampAngle takes its half
			// width as ClampAxis(Max - Min) * 0.5, and ClampAxis(360) is Fmod(360, 360)
			// = 0, so a symmetric range collapses the half width to zero and pins every
			// angle to 180 degrees -- worse than the clamp it was meant to lift.
			CameraManager->ViewRollMin = 0.0f;
			CameraManager->ViewRollMax = 359.999f;
			CameraManager->ViewPitchMin = 0.0f;
			CameraManager->ViewPitchMax = 359.999f;
		}
	}
}

void ASFCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// A station holds its operator by pointer, and a destroyed character never lets go
	// of one on its own: the post would read as occupied by a pawn that no longer
	// exists, refuse every later operator by that name, and fly nothing -- an
	// unoccupied seat that reports itself taken. Standing up on the way out costs one
	// branch and removes the whole class of it.
	//
	// Only the post this character actually holds, though. Nothing today can seat him
	// on a station held by somebody else, but Vacate does not ask who it is throwing
	// out, so the invariant is spelled here rather than left to stay true by luck.
	// The pointer is dropped either way: a seat we do not hold is not one to keep a
	// claim on.
	if (SeatedStation)
	{
		if (SeatedStation->GetOperator().GetObject() == this)
		{
			SeatedStation->Vacate();
		}

		SeatedStation = nullptr;

		// Unstrapped wherever the seat is given up, not only where a person stands up
		// by hand. Left set, the flag outlives the seat and makes him permanently
		// immune to the knockdown while the readout says "footing intact (strapped
		// in)" -- the mechanic silently off, with an instrument saying all is well.
		if (USFShipboardMovementComponent* Movement = GetShipboardMovement())
		{
			Movement->SetStrappedIn(false);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ASFCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PollMovementInput();
	PollLookInput(DeltaSeconds);
	PollInteract();
	PollDockInput();
	PollBindFrameCommand();
}

void ASFCharacter::BindFrame(USFShipFrameComponent* Frame)
{
	USFShipboardMovementComponent* Movement = GetShipboardMovement();
	if (!Movement || !Frame)
	{
		return;
	}

	// Re-entering the same volume must not restart the drift instrument. The volume fires
	// on every entry, and a man stepping in and out of his own doorway would otherwise
	// erase a good measurement with each step.
	if (Movement->GetShipFrame() == Frame)
	{
		return;
	}

	Movement->SetShipFrame(Frame);
}

void ASFCharacter::PollMovementInput()
{
	if (IsSeated())
	{
		return;
	}

	// Legs do not work lying down. Refused here, at the input, rather than through the
	// movement mode: while he is down the mode is falling, and walking input there is
	// air control -- the stock five per cent of it is exactly the kind of motion that
	// would read as drift with nobody's name on it.
	if (const USFShipboardMovementComponent* Movement = GetShipboardMovement())
	{
		if (Movement->IsKnockedDown())
		{
			return;
		}
	}

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// The basis has to lie in the ship's floor plane, not the world's. The carry
	// sets the capsule's rotation to the ship's every tick, so the capsule's own up
	// vector is the deck normal. A world-level yaw basis would drive the character
	// into the deck whenever the ship is rolled, and degenerates near 90 degrees.
	const FVector DeckNormal = GetActorUpVector();
	const FVector ViewForward = PC->GetControlRotation().Vector();

	FVector Forward = FVector::VectorPlaneProject(ViewForward, DeckNormal).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		// Looking straight down at the deck or up at the ceiling leaves nothing to
		// project; fall back to the way the body is already facing.
		Forward = GetActorForwardVector();
	}

	const FVector Right = FVector::CrossProduct(DeckNormal, Forward);

	if (PC->IsInputKeyDown(EKeys::W)) { AddMovementInput(Forward, 1.0); }
	if (PC->IsInputKeyDown(EKeys::S)) { AddMovementInput(Forward, -1.0); }
	if (PC->IsInputKeyDown(EKeys::D)) { AddMovementInput(Right, 1.0); }
	if (PC->IsInputKeyDown(EKeys::A)) { AddMovementInput(Right, -1.0); }
}

void ASFCharacter::PollLookInput(float DeltaSeconds)
{
	if (IsSeated())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	const double Step = LookRateDegPerSec * DeltaSeconds;

	double Yaw = 0.0;
	double Pitch = 0.0;

	if (PC->IsInputKeyDown(EKeys::Left))  { Yaw -= Step; }
	if (PC->IsInputKeyDown(EKeys::Right)) { Yaw += Step; }
	if (PC->IsInputKeyDown(EKeys::Up))    { Pitch += Step; }
	if (PC->IsInputKeyDown(EKeys::Down))  { Pitch -= Step; }

	if (FMath::IsNearlyZero(Yaw) && FMath::IsNearlyZero(Pitch))
	{
		return;
	}

	// Deliberately not AddYawInput/AddPitchInput. Those feed RotationInput, which
	// UpdateRotation adds to the world Euler components of the control rotation --
	// world pitch about world Y, world yaw about world Z. The view is carried in the
	// ship's frame, so once the ship is rolled those axes are not the occupant's
	// up-down and left-right at all, and on a deck rolling at 180 deg/s the axis that
	// "look up" moves along precesses continuously: holding the key swept the view
	// round in a circle instead of raising it.
	//
	// The capsule is rotated to the ship's every tick by the carry, so its up vector
	// is the deck normal and is the axis to turn about.
	const FQuat NewView = StarForge::ShipFrame::ApplyLookStep(
		PC->GetControlRotation().Quaternion(), GetActorUpVector(), Yaw, Pitch);

	PC->SetControlRotation(NewView.Rotator());
}

void ASFCharacter::PollInteract()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// On the edge, not while held: a held key would seat and unseat him every frame,
	// and the log would fill with a station changing hands sixty times a second.
	const bool bDown = PC->IsInputKeyDown(EKeys::E);
	if (bDown && !bInteractWasDown)
	{
		ToggleSeat();
	}
	bInteractWasDown = bDown;
}

void ASFCharacter::PollDockInput()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// On the edge, like the interact key: held, it would dock and undock every frame.
	const bool bDown = PC->IsInputKeyDown(EKeys::F);
	if (bDown && !bDockWasDown)
	{
		// Docking is commanded from the pilot's seat and from nowhere else. Spec 8 puts it
		// there because it is a flight decision: the pilot is the one who can see the
		// approach, and a passenger who could dock from the back of the cabin would be
		// commanding a manoeuvre he has no instrument for.
		USFPilotStationComponent* Pilot = Cast<USFPilotStationComponent>(GetSeatedStation());
		if (!Pilot)
		{
			// Said out loud, because a key that does nothing is otherwise the same as a
			// key that is not bound at all -- and this one was pressed on purpose.
			UE_LOG(LogStarForge, Display, TEXT("Docking is commanded from the pilot's seat."));
		}
		else if (USFDockingComponent* Docking = Pilot->GetOwner()->FindComponentByClass<USFDockingComponent>())
		{
			Docking->Toggle();
		}
		else
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("F pressed in the pilot's seat, but this ship has no docking component."));
		}
	}
	bDockWasDown = bDown;
}

void ASFCharacter::PollBindFrameCommand()
{
	const FString Command = CVarBindFrame.GetValueOnGameThread();
	if (Command == LastBindFrameCommand)
	{
		return;
	}
	LastBindFrameCommand = Command;

	if (Command.IsEmpty())
	{
		return;
	}

	USFShipFrameComponent* Frame = nullptr;
	if (Command.Equals(TEXT("ship"), ESearchCase::IgnoreCase))
	{
		for (TActorIterator<ASFTestShip> It(GetWorld()); It; ++It)
		{
			Frame = It->GetFrameComponent();
			break;
		}
	}
	else if (Command.Equals(TEXT("station"), ESearchCase::IgnoreCase))
	{
		for (TActorIterator<ASFTestStation> It(GetWorld()); It; ++It)
		{
			Frame = It->GetFrameComponent();
			break;
		}
	}

	if (!Frame)
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("sf.BindFrame '%s': no such carrier in this level. Nothing was rebound."), *Command);
		return;
	}

	BindFrame(Frame);

	UE_LOG(LogStarForge, Display,
		TEXT("Frame bound by console to %s (sf.BindFrame). This ignores where he is standing, which is "
			 "the point: it is a negative control."),
		Frame->GetOwner() ? *Frame->GetOwner()->GetActorNameOrLabel() : TEXT("nothing"));
}

void ASFCharacter::ToggleSeat()
{
	if (SeatedStation)
	{
		SeatedStation->Vacate();
		SeatedStation = nullptr;

		// Unstrapped the moment he stands. Under a manoeuvre over the limit the next
		// footing update knocks him down, which is what spec 6 says happens to a man
		// who leaves his seat in one.
		if (USFShipboardMovementComponent* Movement = GetShipboardMovement())
		{
			Movement->SetStrappedIn(false);
		}

		// Walking resumes exactly where he was. He is already in the ship's frame and
		// the carry has been holding him all along; standing up changes what reads his
		// input, not where he is. Nothing to restore on the movement component either:
		// sitting never took him out of MOVE_Walking, so he never left it.
		return;
	}

	USFShipboardMovementComponent* Movement = GetShipboardMovement();
	if (Movement && Movement->IsKnockedDown())
	{
		// A man on the deck cannot reach for a harness. Said out loud, because the key
		// doing nothing is otherwise the "out of reach" case, and the two differ.
		UE_LOG(LogStarForge, Display, TEXT("No seat while knocked down."));
		return;
	}

	USFShipFrameComponent* Frame = Movement ? Movement->GetShipFrame() : nullptr;
	if (!Frame || !Frame->GetOwner())
	{
		return;
	}

	// The nearest station within reach, so two seats side by side do not fight.
	USFShipStationComponent* Best = nullptr;
	double BestDistanceSq = TNumericLimits<double>::Max();

	// The winner's seat position is kept from the comparison rather than worked out
	// again after the seat is taken: one expression, so the place he is measured from
	// and the place he is moved to cannot drift apart under a later edit.
	FVector BestSeatWorld = FVector::ZeroVector;

	TArray<USFShipStationComponent*> Stations;
	Frame->GetOwner()->GetComponents<USFShipStationComponent>(Stations);

	// Occupied posts are considered, not skipped. Skipping them made TryOccupy's
	// refusal unreachable from the only path a person uses, and once a second seat
	// existed it did worse than stay quiet: standing at the pilot's console with the
	// console operator in it, E walked the search past the refusal and sat him at the
	// gunner's post a metre and a half aft. He would have been told nothing, and the
	// checkpoint step that presses E expecting to be refused would have recorded a
	// seat taken as a refusal given. The nearest post answers for itself.
	for (USFShipStationComponent* Station : Stations)
	{
		if (!Station->IsWithinReach(GetActorLocation()))
		{
			continue;
		}

		const FVector SeatWorld =
			Frame->GetOwner()->GetActorTransform().TransformPosition(Station->GetSeatLocationCm());
		const double DistanceSq = FVector::DistSquared(SeatWorld, GetActorLocation());

		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			BestSeatWorld = SeatWorld;
			Best = Station;
		}
	}

	if (!Best)
	{
		UE_LOG(LogStarForge, Display, TEXT("No station within reach."));
		return;
	}

	if (!Best->TryOccupy(this))
	{
		return;
	}

	SeatedStation = Best;

	// Moved into the seat once; from here the carry holds him there, the same carry
	// that held him standing. A second holding mechanism beside a proven one is a
	// second place for the two to disagree.
	SetActorLocation(BestSeatWorld, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);

	// Residual velocity is killed, but the movement mode is deliberately left alone.
	// It has to stay MOVE_Walking: OnMovementModeChanged clears the movement base for
	// every non-walking mode, and with no base MaybeUpdateBasedMovement never calls
	// UpdateBasedMovement -- which is the one function this project overrides, and the
	// only thing that carries an occupant with the ship. MOVE_None is worse still,
	// since PerformMovement returns before it even reaches that call. Either way the
	// carry stops: a seated pilot would be left at a fixed world location while the
	// ship departs at up to 400 m/s, his ship-frame view no longer carried, and
	// DetectCarryGap would bank the whole absence as skipped carries when he stood.
	// What stops him walking is IsSeated(), checked at the top of PollMovementInput --
	// not the movement mode.
	if (Movement)
	{
		Movement->StopMovementImmediately();

		// Strapped in: seated, he is never knocked down, whatever the felt figure says.
		// Checkpoint 4's run C -- seated through the full profile -- depends on it.
		Movement->SetStrappedIn(true);
	}
}

bool ASFCharacter::ProvideFlightIntent(StarForge::FFlightIntent& Out)
{
	// Only while seated. Standing at the console and pressing W is walking, not
	// flying, and an operator who has left his post must say he is not flying rather
	// than hand back a centred stick -- with assist on that is an order to stop.
	if (!IsSeated())
	{
		return false;
	}

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return false;
	}

	double Forward = 0.0;
	double Right = 0.0;
	double Up = 0.0;

	if (PC->IsInputKeyDown(EKeys::W)) { Forward += 1.0; }
	if (PC->IsInputKeyDown(EKeys::S)) { Forward -= 1.0; }
	if (PC->IsInputKeyDown(EKeys::D)) { Right += 1.0; }
	if (PC->IsInputKeyDown(EKeys::A)) { Right -= 1.0; }
	if (PC->IsInputKeyDown(EKeys::SpaceBar)) { Up += 1.0; }
	if (PC->IsInputKeyDown(EKeys::LeftControl)) { Up -= 1.0; }

	double Roll = 0.0;
	double Pitch = 0.0;
	double Yaw = 0.0;

	if (PC->IsInputKeyDown(EKeys::Q)) { Roll -= 1.0; }
	if (PC->IsInputKeyDown(EKeys::R))  { Roll += 1.0; }
	if (PC->IsInputKeyDown(EKeys::Up)) { Pitch += 1.0; }
	if (PC->IsInputKeyDown(EKeys::Down)) { Pitch -= 1.0; }
	if (PC->IsInputKeyDown(EKeys::Left)) { Yaw -= 1.0; }
	if (PC->IsInputKeyDown(EKeys::Right)) { Yaw += 1.0; }

	const UPlayerInput* Input = PC->PlayerInput;

	auto Axis = [Input](const FKey& Key) -> double
	{
		// Raw, deliberately. GetKeyValue returns the value *after* the engine's own
		// per-axis dead zone and rescale, and BaseInput.ini sets that to 0.25 for every
		// gamepad stick with nothing in our DefaultInput.ini overriding it. So the engine
		// would remove and rescale the travel before our tested dead zone ever saw the
		// value: the effective centre would be 0.25 + 0.75*StickDeadZone, and half stick
		// would arrive as a quarter of the command — exactly the linearity this axis
		// exists to provide. Setting StickDeadZone to zero would not restore it either,
		// because the engine's transform is still there underneath. Reading raw makes the
		// dead zone we can test the only one in the path.
		return Input ? static_cast<double>(Input->GetRawKeyValue(Key)) : 0.0;
	};

	const double StickX = StarForge::Units::ApplyDeadZone(Axis(EKeys::Gamepad_LeftY), StickDeadZone);
	const double StickY = StarForge::Units::ApplyDeadZone(Axis(EKeys::Gamepad_LeftX), StickDeadZone);
	const double StickPitch = StarForge::Units::ApplyDeadZone(Axis(EKeys::Gamepad_RightY), StickDeadZone);
	const double StickYaw = StarForge::Units::ApplyDeadZone(Axis(EKeys::Gamepad_RightX), StickDeadZone);

	// Keys and stick add rather than replace each other, so a keyboard and a pad work
	// at once and nobody has to decide which one is in charge; the clamp is what keeps
	// the sum inside the range the flight loop is entitled to assume.
	Out.LinearAxis = FVector(
		FMath::Clamp(Forward + StickX, -1.0, 1.0),
		FMath::Clamp(Right + StickY, -1.0, 1.0),
		FMath::Clamp(Up, -1.0, 1.0));

	// (X roll, Y pitch, Z yaw), the same order everything else uses.
	Out.AngularAxis = FVector(
		FMath::Clamp(Roll, -1.0, 1.0),
		FMath::Clamp(Pitch + StickPitch, -1.0, 1.0),
		FMath::Clamp(Yaw + StickYaw, -1.0, 1.0));

	Out.bFlightAssistEnabled = true;

	return true;
}
