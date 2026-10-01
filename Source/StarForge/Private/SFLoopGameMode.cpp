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
	bool bProfileOff = false;
	if (IConsoleVariable* Profile = IConsoleManager::Get().FindConsoleVariable(TEXT("sf.ShipProfile")))
	{
		// Kept for EndPlay. The variable belongs to the process, not to this world: left at 0, every
		// later session in the same editor -- the spike rig's above all, whose default is the
		// scripted manoeuvre -- would fly on thrust from a zero intent and hold the ship at rest,
		// and a drift of 0.000 cm over a ship that never moved reads as a pass.
		ProfileBeforeRig = Profile->GetInt();
		bRestoreProfile = true;

		Profile->Set(0);

		// Read back, because Set is advisory rather than authoritative. It writes with
		// ECVF_SetByCode, and a value already held by a stronger source keeps its own,
		// silently -- there is no return value and no log line. Only ECVF_SetByConsole
		// outranks code (IConsoleManager.h 155-187; the command line, contrary to the
		// obvious guess, does NOT), so a value typed at the console earlier in this
		// session defeats the line above. The write is not the evidence that the profile
		// is off; the value the variable holds afterwards is.
		//
		// Measured rather than reasoned: -ExecCmds is the console, but its list runs AFTER
		// StartPlay, so it does not lose to this write -- it lands later and overwrites.
		// This read-back cannot catch that case and does not pretend to; what keeps the
		// rig honest there is one level down, in TickScriptedProfile, which refuses a hull
		// that is not simulating or whose array is locked -- a docked ship is both.
		const int32 Held = Profile->GetInt();
		bProfileOff = (Held == 0);
		if (!bProfileOff)
		{
			UE_LOG(LogStarForge, Error,
				TEXT("Loop rig: sf.ShipProfile still reads %d after being set to 0 -- a console ")
				TEXT("value outranks a write from code and this one has been dropped. ")
				TEXT("The ship will fly the checkpoint manoeuvre beside the station."),
				Held);
		}
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
	bool bDocked = false;
	if (Ship && Ship->GetDocking())
	{
		Ship->GetDocking()->DockImmediately();

		// Asked of the component afterwards rather than assumed from the call: docking
		// immediately still has to resolve two ports, and a station that failed to spawn
		// leaves it Free with the ship adrift at the origin.
		bDocked = Ship->GetDocking()->IsDocked();
		if (!bDocked)
		{
			UE_LOG(LogStarForge, Error,
				TEXT("Loop rig: the ship did not dock and is adrift at the origin."));
		}
	}
	else
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Loop rig: no ship or no docking component; the loop cannot start docked."));
	}

	Super::StartPlay();

	bool bPawnInCabin = false;
	if (Ship)
	{
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (Pawn)
		{
			// He appears at the world origin, which on this map is inside the station,
			// and is moved into the cabin here. That move is a genuine crossing of a
			// frame boundary and produces the run's first Frame handover line --
			// Station -> Ship, zero centimetres, because neither carrier is moving.
			// It is the mechanism working, not a defect, and the log says so.
			const FVector Cabin = Ship->GetInteriorSpawnWorldLocation();
			Pawn->SetActorLocation(Cabin);

			// Read back for the same reason the profile is: the rig's closing line names
			// where the man is standing, and a move nobody checked is not a place.
			bPawnInCabin = Pawn->GetActorLocation().Equals(Cabin, 1.0);
			if (!bPawnInCabin)
			{
				UE_LOG(LogStarForge, Error,
					TEXT("Loop rig: the pawn did not reach the cabin -- asked for (%.0f %.0f %.0f), ")
					TEXT("stands at (%.0f %.0f %.0f) cm."),
					Cabin.X, Cabin.Y, Cabin.Z,
					Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, Pawn->GetActorLocation().Z);
			}
		}
		else
		{
			// Silence here would leave him at the world origin, which on this rig is
			// INSIDE THE STATION rather than in the ship -- the checkpoint would then
			// measure a man in the wrong room while every other line read healthy.
			UE_LOG(LogStarForge, Error,
				TEXT("Loop rig: no %s, so nobody was moved into the cabin; ")
				TEXT("anyone who appears later stands at the world origin, inside the station."),
				PC ? TEXT("pawn") : TEXT("player controller"));
		}
	}

	// The startup line the runbook sends the operator to look for, and it now reports
	// three states that were read back rather than three that were attempted. A rig
	// which cannot verify its own setup is not an instrument: every one of the three
	// above can fail silently, and a line that says "docked" over a ship adrift beside
	// the station corrupts the whole run that trusted it.
	const FString Report = FString::Printf(
		TEXT("Loop rig: scripted profile %s; the ship %s; the man %s."),
		bProfileOff ? TEXT("off") : TEXT("STILL ON"),
		bDocked ? TEXT("spawns docked") : TEXT("is ADRIFT"),
		bPawnInCabin ? TEXT("stands in the cabin") : TEXT("is NOT in the cabin"));

	if (bProfileOff && bDocked && bPawnInCabin)
	{
		UE_LOG(LogStarForge, Display, TEXT("%s"), *Report);
	}
	else
	{
		UE_LOG(LogStarForge, Error, TEXT("%s"), *Report);
	}
}

void ASFLoopGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// At SetByCode, the priority the write in StartPlay left the variable at, which is how the
	// engine's own TGuardConsoleVariable restores too. Set takes only an equal or stronger source
	// (CanChange, ConsoleManager.cpp 275-311), so the default's own SetByConstructor would be
	// refused, and a refusal changes nothing and writes a LogConsoleManager warning. A value typed
	// at the console since -- -ExecCmds included -- outranks code and stands, which is right: a
	// person chose that one. The priority is asked first so that standing aside is not a warning.
	// No line of ours: a standalone run ends through here too, after its last measured line.
	if (bRestoreProfile)
	{
		IConsoleVariable* Profile = IConsoleManager::Get().FindConsoleVariable(TEXT("sf.ShipProfile"));
		if (Profile && ((uint32)Profile->GetFlags() & ECVF_SetByMask) <= (uint32)ECVF_SetByCode)
		{
			Profile->Set(ProfileBeforeRig, ECVF_SetByCode);
		}
		bRestoreProfile = false;
	}

	Super::EndPlay(EndPlayReason);
}
