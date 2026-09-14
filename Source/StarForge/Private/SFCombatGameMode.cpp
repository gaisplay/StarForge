#include "SFCombatGameMode.h"

#include "Debug/SFFrameDriftHUD.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/SFCharacter.h"
#include "Ship/SFAutomationOperatorComponent.h"
#include "Ship/SFGunnerStationComponent.h"
#include "Ship/SFPilotStationComponent.h"
#include "Ship/SFPursuitOperatorComponent.h"
#include "Ship/SFTestShip.h"
#include "StarForgeLog.h"
#include "Units.h"

namespace
{
	/**
	 * Whether the combat rig spawns the pirate. Off, the player's automation is given no target,
	 * and with sf.Fire 1 the weapon must still fire -- the control that the weapon listens to the
	 * trigger and not to the aim.
	 *
	 * Read once, when the rig assembles itself, and that is before -ExecCmds is applied: set it
	 * before the level starts (in the editor's console before Play, or with an ini override on
	 * the command line), or type it and restart the level.
	 */
	TAutoConsoleVariable<int32> CVarPirate(TEXT("sf.Pirate"), 1,
		TEXT("1: the combat rig spawns the pirate. 0: it does not, and the player's automation has no target.\n")
		TEXT("Read when the rig assembles itself, before -ExecCmds applies: set it before the level starts."),
		ECVF_Default);

	namespace CombatRig
	{
		/**
		 * 1000 m ahead, which a shot crosses in a second, and 150 m above. Off the plane on purpose:
		 * in one plane the pitch bearing is zero from the first frame, and a pursuit operator with
		 * its pitch sign backwards would fly exactly like a correct one. From here it has 8.5 deg
		 * to take out, and the run shows whether it takes them out or doubles them.
		 */
		constexpr double PirateXMetres = 1000.0;
		constexpr double PirateZMetres = 150.0;

		/** Nose back toward the player's ship, so neither turret starts outside its yaw stops. */
		constexpr double PirateYawDeg = 180.0;

		/**
		 * Deferred, so Crewing, the drone and the trigger are on the hull before anything reads
		 * them. BeginPlay has not been dispatched yet either -- this runs before Super::StartPlay --
		 * but the deferred spawn makes that ordering not matter.
		 */
		ASFTestShip* SpawnCombatShip(UWorld* World, FName Name, const FTransform& At, ESFShipCrewing Crewing,
			bool bWeaponsFree)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
			Params.Name = Name;
			Params.bDeferConstruction = true;

			ASFTestShip* NewShip = World->SpawnActor<ASFTestShip>(ASFTestShip::StaticClass(), At, Params);
			if (!NewShip)
			{
				return nullptr;
			}

			NewShip->Crewing = Crewing;
			NewShip->bSpawnTargetDrone = false;
			NewShip->bWeaponsFree = bWeaponsFree;
			NewShip->FinishSpawning(At);
			return NewShip;
		}

		FString DescribeHolder(const USFShipStationComponent* Station)
		{
			return Station ? Station->GetOperatorDisplayName() : FString(TEXT("no such post"));
		}
	}
}

ASFCombatGameMode::ASFCombatGameMode()
{
	DefaultPawnClass = ASFCharacter::StaticClass();
	HUDClass = ASFFrameDriftHUD::StaticClass();
}

void ASFCombatGameMode::StartPlay()
{
	// Same reasoning as the loop rig: the scripted profile is a measurement instrument and this is
	// not a measurement. Written, then read back, because Set is advisory -- see SFLoopGameMode.cpp
	// for what outranks a write from code, and for why -ExecCmds, applied later, is not caught here.
	// The pirate does not answer this variable at all; it is the player's ship that would fly the
	// checkpoint manoeuvre through the fight.
	bool bProfileOff = false;
	if (IConsoleVariable* Profile = IConsoleManager::Get().FindConsoleVariable(TEXT("sf.ShipProfile")))
	{
		Profile->Set(0);
		const int32 Held = Profile->GetInt();
		bProfileOff = (Held == 0);
		if (!bProfileOff)
		{
			UE_LOG(LogStarForge, Error,
				TEXT("Combat rig: sf.ShipProfile still reads %d after being set to 0 -- a console value outranks a ")
				TEXT("write from code. The player's ship will fly the checkpoint manoeuvre through the fight."),
				Held);
		}
	}
	else
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Combat rig: sf.ShipProfile not found, so the scripted profile could not be turned off."));
	}

	UWorld* World = GetWorld();
	const bool bPirateWanted = CVarPirate.GetValueOnGameThread() != 0;

	// Player's ship at the origin, pirate 1000 m along +X and 150 m up, yawed 180 so its nose faces
	// back. Both spawn with no target drone: the thing to aim at is the other ship.
	// 1000 m is crossed by a shot in 1.0 s, so the run has something to watch from its first seconds.
	// Before Super::StartPlay, for the spike rig's reason: the pawn's BeginPlay has to find a frame.
	//
	// The player's ship holds fire; the person opens it with sf.Fire 1. Armed on both sides, two
	// identical hulls aiming at each other's centres put identical shots into identical faces and
	// power down in the same frame at 6.7 s, before the pursuit has come anywhere near its hold
	// distance -- the fight was a mirror, not a fight. The pirate stays armed.
	Ship = CombatRig::SpawnCombatShip(
		World, TEXT("Ship"), FTransform::Identity, ESFShipCrewing::PlayerShip, /*bWeaponsFree*/ false);

	if (bPirateWanted)
	{
		const FTransform PirateAt(
			FRotator(0.0, CombatRig::PirateYawDeg, 0.0),
			FVector(
				StarForge::Units::MetresToCm(CombatRig::PirateXMetres),
				0.0,
				StarForge::Units::MetresToCm(CombatRig::PirateZMetres)));
		Pirate = CombatRig::SpawnCombatShip(
			World, TEXT("Pirate"), PirateAt, ESFShipCrewing::Automated, /*bWeaponsFree*/ true);
	}

	Super::StartPlay();

	// After Super::StartPlay, because before it the components have not begun to live. A target
	// set earlier would survive -- neither the automation's BeginPlay nor the hull's touches it
	// when no drone is spawned -- but nothing is gained by relying on that.
	if (Ship && Ship->GetAutomationOperator())
	{
		Ship->GetAutomationOperator()->SetTargetActor(Pirate);
	}
	if (Pirate)
	{
		if (Pirate->GetAutomationOperator()) { Pirate->GetAutomationOperator()->SetTargetActor(Ship); }
		if (Pirate->GetPursuitOperator()) { Pirate->GetPursuitOperator()->SetTargetActor(Ship); }
	}

	bool bPawnInCabin = false;
	if (Ship)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (Pawn)
		{
			const FVector Cabin = Ship->GetInteriorSpawnWorldLocation();
			Pawn->SetActorLocation(Cabin);
			bPawnInCabin = Pawn->GetActorLocation().Equals(Cabin, 1.0);
			if (!bPawnInCabin)
			{
				UE_LOG(LogStarForge, Error,
					TEXT("Combat rig: the pawn did not reach the cabin -- asked for (%.0f %.0f %.0f), stands at (%.0f %.0f %.0f) cm."),
					Cabin.X, Cabin.Y, Cabin.Z,
					Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, Pawn->GetActorLocation().Z);
			}
		}
		else
		{
			UE_LOG(LogStarForge, Error,
				TEXT("Combat rig: no %s, so nobody was moved into the cabin."),
				PC ? TEXT("pawn") : TEXT("player controller"));
		}
	}

	// Five states, every one read back from the object it describes. An upper-case word in the line
	// is a state that did not hold, and the line then goes out as an error.
	bool bFault = false;

	FString ShipText;
	if (!Ship)
	{
		ShipText = TEXT("there is NO PLAYER'S SHIP");
		bFault = true;
	}
	else
	{
		ShipText = Ship->GetCrewing() == ESFShipCrewing::PlayerShip
			? TEXT("the player's ship is crewed") : TEXT("the player's ship is NOT CREWED BY THE PLAYER");
		bFault = bFault || Ship->GetCrewing() != ESFShipCrewing::PlayerShip;

		if (!Ship->bWeaponsFree)
		{
			ShipText += TEXT(" and holds fire until sf.Fire 1");
		}
		else
		{
			ShipText += TEXT(" but does NOT HOLD FIRE -- its automation shoots on its own");
			bFault = true;
		}

		const AActor* Aimed = Ship->GetAutomationOperator() ? Ship->GetAutomationOperator()->GetTargetActor() : nullptr;
		if (Aimed != Pirate.Get())
		{
			ShipText += FString::Printf(TEXT(" but its gunner tracks the WRONG TARGET (%s)"), *GetNameSafe(Aimed));
			bFault = true;
		}
	}

	FString PirateText;
	bool bPirateWithheld = false;
	if (!bPirateWanted)
	{
		// Asked for, so not a fault; but the run is the control, not a fight, and the line is not
		// allowed to read like one.
		PirateText = TEXT("no pirate, withheld by sf.Pirate 0, so the player's automation has no target");
		bPirateWithheld = true;
	}
	else if (!Pirate)
	{
		PirateText = TEXT("there is NO PIRATE");
		bFault = true;
	}
	else
	{
		const USFPilotStationComponent* PiratePilot = Pirate->GetPilotStation();
		const USFGunnerStationComponent* PirateGunner = Pirate->GetGunnerStation();
		const USFPursuitOperatorComponent* Pursuit = Pirate->GetPursuitOperator();
		const USFAutomationOperatorComponent* Automation = Pirate->GetAutomationOperator();

		const bool bAutomated = Pirate->GetCrewing() == ESFShipCrewing::Automated
			&& PiratePilot && Pursuit && PiratePilot->GetOperator().GetObject() == Pursuit
			&& PirateGunner && Automation && PirateGunner->GetOperator().GetObject() == Automation;

		PirateText = bAutomated
			? FString(TEXT("the pirate is automated"))
			: FString::Printf(TEXT("the pirate is NOT AUTOMATED (pilot's post held by %s, gunner's by %s)"),
				*CombatRig::DescribeHolder(PiratePilot), *CombatRig::DescribeHolder(PirateGunner));
		bFault = bFault || !bAutomated;

		PirateText += Pirate->bWeaponsFree ? TEXT(" and armed") : TEXT(" and NOT ARMED");
		bFault = bFault || !Pirate->bWeaponsFree;

		const AActor* Chased = Pursuit ? Pursuit->GetTargetActor() : nullptr;
		const AActor* Aimed = Automation ? Automation->GetTargetActor() : nullptr;
		if (Chased != Ship.Get() || Aimed != Ship.Get())
		{
			PirateText += FString::Printf(TEXT(" but chases the WRONG TARGET (%s) or aims at it (%s)"),
				*GetNameSafe(Chased), *GetNameSafe(Aimed));
			bFault = true;
		}
	}

	const FString SeparationText = (Ship && Pirate)
		? FString::Printf(TEXT("separation %.1f m"),
			StarForge::Units::CmToMetres(FVector::Dist(Ship->GetActorLocation(), Pirate->GetActorLocation())))
		: FString(TEXT("separation none"));

	bFault = bFault || !bProfileOff || !bPawnInCabin;

	const FString Report = FString::Printf(
		TEXT("Combat rig: scripted profile %s; %s; %s; the man %s; %s."),
		bProfileOff ? TEXT("off") : TEXT("STILL ON"),
		*ShipText,
		*PirateText,
		bPawnInCabin ? TEXT("stands in the cabin") : TEXT("is NOT in the cabin"),
		*SeparationText);

	if (bFault)
	{
		UE_LOG(LogStarForge, Error, TEXT("%s"), *Report);
	}
	else if (bPirateWithheld)
	{
		UE_LOG(LogStarForge, Warning, TEXT("%s"), *Report);
	}
	else
	{
		UE_LOG(LogStarForge, Display, TEXT("%s"), *Report);
	}
}
