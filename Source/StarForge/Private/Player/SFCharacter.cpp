#include "Player/SFCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/SFShipboardMovementComponent.h"
#include "EngineUtils.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFShipStationComponent.h"
#include "ShipFrameMath.h"
#include "StarForgeLog.h"

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

	// The rig has exactly one ship, so wiring is unambiguous. Boarding volumes
	// arrive with docking in the world phase.
	if (USFShipboardMovementComponent* Movement = GetShipboardMovement())
	{
		USFShipFrameComponent* FoundFrame = nullptr;

		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (USFShipFrameComponent* Frame = It->FindComponentByClass<USFShipFrameComponent>())
			{
				FoundFrame = Frame;
				break;
			}
		}

		if (FoundFrame)
		{
			Movement->SetShipFrame(FoundFrame);
		}
		else
		{
			// A rig that silently measures nothing is worse than one that fails.
			UE_LOG(LogTemp, Warning,
				TEXT("SFCharacter: found no USFShipFrameComponent in the world. The "
					 "occupant will stand in world space while the ship flies away, and "
					 "every drift reading will be a zero that means nothing."));
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

void ASFCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PollMovementInput();
	PollLookInput(DeltaSeconds);
	PollInteract();
}

void ASFCharacter::PollMovementInput()
{
	if (IsSeated())
	{
		return;
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

void ASFCharacter::ToggleSeat()
{
	if (SeatedStation)
	{
		SeatedStation->Vacate();
		SeatedStation = nullptr;

		// Walking resumes exactly where he was. He is already in the ship's frame and
		// the carry has been holding him all along; standing up changes what reads his
		// input, not where he is. Nothing to restore on the movement component either:
		// sitting never took him out of MOVE_Walking, so he never left it.
		return;
	}

	USFShipFrameComponent* Frame = GetShipboardMovement() ? GetShipboardMovement()->GetShipFrame() : nullptr;
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

	for (USFShipStationComponent* Station : Stations)
	{
		if (Station->IsOccupied() || !Station->IsWithinReach(GetActorLocation()))
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
		UE_LOG(LogStarForge, Display, TEXT("No free station within reach."));
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
	if (USFShipboardMovementComponent* Movement = GetShipboardMovement())
	{
		Movement->StopMovementImmediately();
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

	Out.LinearAxis = FVector(Forward, Right, Up);

	double Roll = 0.0;
	double Pitch = 0.0;
	double Yaw = 0.0;

	if (PC->IsInputKeyDown(EKeys::Q)) { Roll -= 1.0; }
	if (PC->IsInputKeyDown(EKeys::R))  { Roll += 1.0; }
	if (PC->IsInputKeyDown(EKeys::Up)) { Pitch += 1.0; }
	if (PC->IsInputKeyDown(EKeys::Down)) { Pitch -= 1.0; }
	if (PC->IsInputKeyDown(EKeys::Left)) { Yaw -= 1.0; }
	if (PC->IsInputKeyDown(EKeys::Right)) { Yaw += 1.0; }

	// (X roll, Y pitch, Z yaw), the same order everything else uses.
	Out.AngularAxis = FVector(Roll, Pitch, Yaw);
	Out.bFlightAssistEnabled = true;

	return true;
}
