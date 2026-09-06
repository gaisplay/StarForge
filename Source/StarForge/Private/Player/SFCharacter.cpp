#include "Player/SFCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/SFShipboardMovementComponent.h"
#include "EngineUtils.h"
#include "Ship/SFShipFrameComponent.h"

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
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (USFShipFrameComponent* Frame = It->FindComponentByClass<USFShipFrameComponent>())
			{
				Movement->SetShipFrame(Frame);
				break;
			}
		}
	}
}

void ASFCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PollMovementInput();
	PollLookInput(DeltaSeconds);
}

void ASFCharacter::PollMovementInput()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	const FRotator ViewYaw(0.0, PC->GetControlRotation().Yaw, 0.0);
	const FVector Forward = FRotationMatrix(ViewYaw).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(ViewYaw).GetUnitAxis(EAxis::Y);

	if (PC->IsInputKeyDown(EKeys::W)) { AddMovementInput(Forward, 1.0); }
	if (PC->IsInputKeyDown(EKeys::S)) { AddMovementInput(Forward, -1.0); }
	if (PC->IsInputKeyDown(EKeys::D)) { AddMovementInput(Right, 1.0); }
	if (PC->IsInputKeyDown(EKeys::A)) { AddMovementInput(Right, -1.0); }
}

void ASFCharacter::PollLookInput(float DeltaSeconds)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	const double Step = LookRateDegPerSec * DeltaSeconds;

	if (PC->IsInputKeyDown(EKeys::Left))  { PC->AddYawInput(-Step); }
	if (PC->IsInputKeyDown(EKeys::Right)) { PC->AddYawInput(Step); }
	if (PC->IsInputKeyDown(EKeys::Up))    { PC->AddPitchInput(-Step); }
	if (PC->IsInputKeyDown(EKeys::Down))  { PC->AddPitchInput(Step); }
}
