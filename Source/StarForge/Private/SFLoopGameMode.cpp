#include "SFLoopGameMode.h"

#include "Debug/SFFrameDriftHUD.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/SFCharacter.h"
#include "Ship/SFDockingComponent.h"
#include "Ship/SFTestShip.h"
#include "Ship/SFTestStation.h"
#include "StarForgeLog.h"

ASFLoopGameMode::ASFLoopGameMode()
{
	DefaultPawnClass = ASFCharacter::StaticClass();
	HUDClass = ASFFrameDriftHUD::StaticClass();
}

void ASFLoopGameMode::StartPlay()
{
	// The scripted profile is a measurement instrument and this is not a measurement:
	// left on, the ship would fly the checkpoint manoeuvre at 400 m/s while attached to a
	// station, which is not a state anything here is written for.
	//
	// This is the only place in the project that reaches a console variable by name, and
	// it does so because it WRITES one that belongs to another file. The rule everywhere
	// else -- declare it where you read it -- exists because a failed lookup returns null
	// and a missing value reads as zero, silently. Here the miss is said out loud, in
	// error, because the alternative is a loop rig quietly flying the wrong profile.
	if (IConsoleVariable* Profile = IConsoleManager::Get().FindConsoleVariable(TEXT("sf.ShipProfile")))
	{
		Profile->Set(0);
	}
	else
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Loop rig: sf.ShipProfile not found, so the scripted profile could not be turned off. ")
			TEXT("If the ship starts flying the checkpoint manoeuvre at the station, this line is why."));
	}

	FActorSpawnParameters StationParams;
	StationParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Requested rather than the default Required_Fatal: a taken name is not worth a crash,
	// and nothing depends on the name beyond the readability of one log line.
	StationParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

	// Named, because the frame-handover line is the thing a person reads at the moment of
	// the transition, and "Station -> Ship" reads twice as well as "SFTestStation_0 ->
	// SFTestShip_0". Nothing else depends on these names.
	StationParams.Name = TEXT("Station");
	Station = GetWorld()->SpawnActor<ASFTestStation>(
		ASFTestStation::StaticClass(), FTransform::Identity, StationParams);

	FActorSpawnParameters ShipParams;
	ShipParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ShipParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	ShipParams.Name = TEXT("Ship");

	// Spawned at the origin and immediately moved by the docking maths. The pose is not
	// authored here on purpose: it is derived from the two ports, so moving a wall or a
	// port moves the docked ship with it, and a constant here would silently stop
	// tracking them.
	Ship = GetWorld()->SpawnActor<ASFTestShip>(
		ASFTestShip::StaticClass(), FTransform::Identity, ShipParams);

	// Before Super::StartPlay, and that ordering is the whole trick. Super is what begins
	// play on the world and spawns the pawn; dock after it and the pawn appears while the
	// ship is still at the origin, inside the station's interior volume as well as its
	// own, with nothing deciding which of the two claims him. Docking here means there is
	// no frame in which the ship is adrift, and DockImmediately resolves its own ports
	// because BeginPlay has not run yet.
	if (Ship && Ship->GetDocking())
	{
		Ship->GetDocking()->DockImmediately();
	}
	else
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Loop rig: no ship or no docking component; the loop cannot start docked."));
	}

	Super::StartPlay();

	if (Ship)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				// He appears at the world origin, which on this map is inside the station,
				// and is moved into the cabin here. That move is a genuine crossing of a
				// frame boundary and produces the run's first Frame handover line --
				// Station -> Ship, zero centimetres, because neither carrier is moving.
				// It is the mechanism working, not a defect, and the log says so.
				Pawn->SetActorLocation(Ship->GetInteriorSpawnWorldLocation());
			}
		}
	}

	UE_LOG(LogStarForge, Display,
		TEXT("Loop rig: scripted profile forced off; the ship spawns docked."));
}
