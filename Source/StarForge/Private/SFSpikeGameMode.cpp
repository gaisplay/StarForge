#include "SFSpikeGameMode.h"

#include "Debug/SFFrameDriftHUD.h"
#include "GameFramework/PlayerController.h"
#include "Player/SFCharacter.h"
#include "Ship/SFTestShip.h"

ASFSpikeGameMode::ASFSpikeGameMode()
{
	DefaultPawnClass = ASFCharacter::StaticClass();
	HUDClass = ASFFrameDriftHUD::StaticClass();
}

void ASFSpikeGameMode::StartPlay()
{
	// The ship must exist before the default pawn spawns, so that the character's
	// BeginPlay finds a frame to ride.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Ship = GetWorld()->SpawnActor<ASFTestShip>(
		ASFTestShip::StaticClass(), FTransform::Identity, SpawnParams);

	Super::StartPlay();

	if (Ship)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				Pawn->SetActorLocation(Ship->GetInteriorSpawnWorldLocation());
			}
		}
	}
}
