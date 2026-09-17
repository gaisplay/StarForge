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

	// Named as the loop rig names it, so a line from this rig and a line from that one carry
	// the same label. Requested, not Required_Fatal: a taken name is not worth a crash.
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	SpawnParams.Name = TEXT("Ship");

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
