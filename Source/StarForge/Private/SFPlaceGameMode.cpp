#include "SFPlaceGameMode.h"

#include "Debug/SFFrameDriftHUD.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Place.h"
#include "Player/SFCharacter.h"
#include "Player/SFShipboardMovementComponent.h"
#include "Ship/SFDockingComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFTestShip.h"
#include "Ship/SFTestStation.h"
#include "StarForgeLog.h"
#include "UObject/ConstructorHelpers.h"
#include "Units.h"
#include "World/SFPlaceDataAsset.h"

namespace
{
	/**
	 * Moves the place along +X by this many metres, on top of what the asset authored. Empty
	 * leaves the authored origin alone.
	 *
	 * A STRING holding a number, not a float. TAutoConsoleVariable<float> rounds to the nearest
	 * representable float, and at 4.3e10 the gap between neighbouring floats is about 4 km --
	 * a quarter of the rig place's own radius, in the one figure this whole stage turns on.
	 * sf.BindFrame and sf.AimAt are the precedent for a string variable; FCString::Atod is the
	 * parse, and what it parsed is printed back.
	 *
	 * Read ONCE, when the rig assembles itself, which is before -ExecCmds is applied: set it
	 * before the level starts, with "-ini:Engine:[ConsoleVariables]:sf.PlaceOffsetM=..." on the
	 * command line, or in the editor's console before Play. One -ini switch per variable: two
	 * joined by a comma are not taken.
	 */
	TAutoConsoleVariable<FString> CVarPlaceOffsetM(TEXT("sf.PlaceOffsetM"), TEXT(""),
		TEXT("Metres to move the place along +X, on top of the authored origin. Empty: leave it alone.\n")
		TEXT("Read when the rig assembles itself, before -ExecCmds applies: set it before the level starts."),
		ECVF_Default);

	/**
	 * Replaces the place's authored radius, in metres. Empty leaves it alone.
	 *
	 * Two variables rather than one, because the predicate has two inputs and the two ceilings
	 * are broken by different ones: the engine's by moving the place, the crossing budget's by
	 * growing it. A control that can only break one cannot tell the two refusals apart.
	 */
	TAutoConsoleVariable<FString> CVarPlaceRadiusM(TEXT("sf.PlaceRadiusM"), TEXT(""),
		TEXT("Metres of radius, replacing the authored one. Empty: leave it alone.\n")
		TEXT("Read when the rig assembles itself, before -ExecCmds applies."),
		ECVF_Default);

	namespace PlaceRig
	{
		/**
		 * The place the rig falls back on when no asset is authored: the same one
		 * PopulateOpenSpaceWithOneStation fills in. Same figures, so the size line the fallback
		 * prints and the size line the asset prints must match CHARACTER FOR CHARACTER -- which
		 * is how the asset is accepted in task 6 without a second instrument.
		 */
		const FName FallbackPlaceName = TEXT("OpenSpace");
		constexpr double FallbackRadiusMetres = 18000.0;
		const FName StationName = TEXT("Station");

		/** Where the asset is looked for, named once so the finder and the warning cannot drift. */
		const TCHAR* AssetPath = TEXT("/Game/Places/DA_Place_OpenSpace.DA_Place_OpenSpace");

		/**
		 * Reads a metre figure out of a string console variable.
		 *
		 * Returns false on empty, which is "no command", and on anything that does not parse to
		 * a finite number, which is said out loud by the caller. A silent fall-back to the
		 * authored value would let a negative control be performed with a typo in it and count
		 * as having found nothing.
		 *
		 * One guard covers infinity and NaN both: FMath::IsFinite tests that the exponent is not
		 * all ones (GenericPlatformMath.h 587), and a NaN's is.
		 */
		bool ParseMetres(const FString& Raw, double& OutMetres)
		{
			if (Raw.IsEmpty())
			{
				return false;
			}

			const double Parsed = FCString::Atod(*Raw);
			if (!FMath::IsFinite(Parsed))
			{
				return false;
			}

			OutMetres = Parsed;
			return true;
		}
	}
}

ASFPlaceGameMode::ASFPlaceGameMode()
{
	DefaultPawnClass = ASFCharacter::StaticClass();
	HUDClass = ASFFrameDriftHUD::StaticClass();

	// By path and guarded, exactly as ASFTestShip resolves DA_TestShip, and for the same two
	// reasons: the map is empty so there is no placed actor to author this on, and the asset
	// does not exist the first time this file compiles. An unresolved finder leaves PlaceData
	// null, which StartPlay reports in a warning and answers with a place written in code.
	static ConstructorHelpers::FObjectFinder<USFPlaceDataAsset> PlaceAsset(PlaceRig::AssetPath);

	if (PlaceAsset.Succeeded())
	{
		PlaceData = PlaceAsset.Object;
	}
}

void ASFPlaceGameMode::StartPlay()
{
	// The scripted profile is a measurement instrument and this is not a measurement: left on,
	// the ship would fly the checkpoint manoeuvre at 400 m/s while attached to a station.
	// Written, then read back, because Set is advisory -- SFLoopGameMode.cpp carries the whole
	// reasoning, including why -ExecCmds, applied later, is not caught by a read-back here.
	bool bProfileOff = false;
	if (IConsoleVariable* Profile = IConsoleManager::Get().FindConsoleVariable(TEXT("sf.ShipProfile")))
	{
		Profile->Set(0);

		const int32 Held = Profile->GetInt();
		bProfileOff = (Held == 0);
		if (!bProfileOff)
		{
			UE_LOG(LogStarForge, Error,
				TEXT("Place rig: sf.ShipProfile still reads %d after being set to 0 -- a console ")
				TEXT("value outranks a write from code and this one has been dropped. ")
				TEXT("The ship will fly the checkpoint manoeuvre beside the station."),
				Held);
		}
	}
	else
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Place rig: sf.ShipProfile not found, so the scripted profile could not be turned off. ")
			TEXT("If the ship starts flying the checkpoint manoeuvre at the station, this line is why."));
	}

	UWorld* World = GetWorld();

	FName PlaceName = PlaceRig::FallbackPlaceName;
	ESFPlaceKind Kind = ESFPlaceKind::OpenSpace;
	FName SystemId = NAME_None;
	FVector OriginMetres = FVector::ZeroVector;
	double RadiusMetres = PlaceRig::FallbackRadiusMetres;
	TArray<FSFPlaceCarrier> Carriers;

	const bool bFromAsset = (PlaceData != nullptr);
	if (bFromAsset)
	{
		PlaceName = PlaceData->PlaceName;
		Kind = PlaceData->Kind;
		SystemId = PlaceData->SystemId;
		OriginMetres = PlaceData->OriginFromWorldOriginMetres;
		RadiusMetres = PlaceData->RadiusMetres;
		Carriers = PlaceData->Carriers;
	}
	else
	{
		// Not a stub: the same place, described by the same fields, measured by the same
		// predicate. Said out loud because a rig that quietly builds something other than what
		// the data says is a rig whose every figure belongs to an unknown world.
		FSFPlaceCarrier Station;
		Station.CarrierClass = ASFTestStation::StaticClass();
		Station.CarrierName = PlaceRig::StationName;
		Carriers.Add(Station);

		UE_LOG(LogStarForge, Warning,
			TEXT("Place rig: no place asset at /Game/Places/DA_Place_OpenSpace, so a place written ")
			TEXT("in code was used instead. Its figures are the asset's; the size line below is what ")
			TEXT("the two have to agree on."));
	}

	FString OverrideText;

	double OffsetM = 0.0;
	const FString OffsetRaw = CVarPlaceOffsetM.GetValueOnGameThread();
	if (!OffsetRaw.IsEmpty())
	{
		if (PlaceRig::ParseMetres(OffsetRaw, OffsetM))
		{
			OriginMetres.X += OffsetM;
			OverrideText += FString::Printf(
				TEXT(" sf.PlaceOffsetM moved it %+.6e m along +X."), OffsetM);
		}
		else
		{
			// Loud, because a control performed with a typo in it counts as a control that
			// found nothing -- and looks exactly like one that found nothing.
			UE_LOG(LogStarForge, Warning,
				TEXT("Place rig: sf.PlaceOffsetM='%s' is not a finite number of metres; the ")
				TEXT("authored origin stands."), *OffsetRaw);
		}
	}

	double OverrideRadiusM = 0.0;
	const FString RadiusRaw = CVarPlaceRadiusM.GetValueOnGameThread();
	if (!RadiusRaw.IsEmpty())
	{
		if (PlaceRig::ParseMetres(RadiusRaw, OverrideRadiusM))
		{
			// Replaces rather than adds, because the authored radius is what a control needs to
			// be rid of: one that could only grow it could never ask for a smaller place.
			RadiusMetres = OverrideRadiusM;
			OverrideText += FString::Printf(
				TEXT(" sf.PlaceRadiusM replaced the radius with %+.6e m."), OverrideRadiusM);
		}
		else
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("Place rig: sf.PlaceRadiusM='%s' is not a finite number of metres; the ")
				TEXT("authored radius stands."), *RadiusRaw);
		}
	}

	if (!OverrideText.IsEmpty())
	{
		// Printed back rather than trusted: the size line below measures what these left behind,
		// and a control whose input nobody read is a control that proves nothing.
		UE_LOG(LogStarForge, Display,
			TEXT("Place rig: the console changed the place before it was measured.%s"), *OverrideText);
	}

	// The limits are the engine's and the owner's, not this place's. See FPlaceLimits.
	const StarForge::Place::FPlaceLimits Limits;
	const StarForge::Place::FPlaceFit Fit =
		StarForge::Place::MeasurePlace(OriginMetres, RadiusMetres, Limits);

	const bool bFits = (Fit.Refusal == StarForge::Place::EPlaceRefusal::None);

	// One line per place, fitting or not. Spec 10.4: a line that appears only in trouble cannot
	// be told apart from an instrument that never ran.
	const FString SizeLine = FString::Printf(
		TEXT("Place size: '%s' (%s, system '%s') origin (%.6e %.6e %.6e) m, radius %.6e m; ")
		TEXT("reach %.6e m of the %.6e m engine ceiling, margin %.6e m; ")
		TEXT("diameter %.6e m of the %.6e m crossing ceiling (%.1f s at %.6e m/s), margin %.6e m; ")
		TEXT("the slowest hull crosses it in %.6e s -- %s.%s"),
		*PlaceName.ToString(),
		USFPlaceDataAsset::KindName(Kind),
		SystemId.IsNone() ? TEXT("unnamed") : *SystemId.ToString(),
		OriginMetres.X, OriginMetres.Y, OriginMetres.Z,
		RadiusMetres,
		Fit.ReachMetres, Fit.EngineCeilingMetres, Fit.EngineMarginMetres,
		Fit.DiameterMetres, Fit.CrossingCeilingMetres,
		Limits.CrossingBudgetSeconds, Limits.SlowestHullSpeedMetresPerSec,
		Fit.CrossingMarginMetres,
		Fit.CrossingSeconds,
		bFits ? TEXT("fits") : TEXT("REFUSED"),
		bFits ? TEXT("") : *FString::Printf(TEXT(" Reason: %s."),
			StarForge::Place::RefusalName(Fit.Refusal)));

	if (bFits)
	{
		UE_LOG(LogStarForge, Display, TEXT("%s"), *SizeLine);
	}
	else
	{
		// An Error, and deliberately so: spec 13 asks that a place beyond the border refuse to
		// assemble with a NAMED ERROR before anything undocks. The control run therefore carries
		// one, and the runbook lists it as legitimately red with the condition under which it
		// becomes a finding again -- the same treatment L_Combat's three red lines get.
		UE_LOG(LogStarForge, Error, TEXT("%s"), *SizeLine);
	}

	if (!bFits)
	{
		// Nothing is built. No carriers, no ship, and no pawn: with DefaultPawnClass nulled
		// before Super, there is nobody standing in an empty world whose instruments would
		// print figures about a place that does not exist.
		DefaultPawnClass = nullptr;
	}

	if (bFits)
	{
		for (const FSFPlaceCarrier& Carrier : Carriers)
		{
			if (!Carrier.CarrierClass)
			{
				UE_LOG(LogStarForge, Error,
					TEXT("Place carrier: '%s' in place '%s' has no class and was not spawned."),
					*Carrier.CarrierName.ToString(), *PlaceName.ToString());
				continue;
			}

			// The place's reach bounds every point INSIDE it, which is exactly why one check per
			// place is enough. A carrier authored outside the radius breaks that sentence, and
			// with it the meaning of the size line above.
			const double FromPlaceOrigin = Carrier.LocationInPlaceMetres.Size();
			if (FromPlaceOrigin > RadiusMetres)
			{
				UE_LOG(LogStarForge, Warning,
					TEXT("Place carrier: '%s' stands %.6e m from the origin of place '%s', which is ")
					TEXT("outside its %.6e m radius. The size line above does not cover it."),
					*Carrier.CarrierName.ToString(), FromPlaceOrigin,
					*PlaceName.ToString(), RadiusMetres);
			}

			const FVector WorldCm(
				StarForge::Units::MetresToCm(OriginMetres.X + Carrier.LocationInPlaceMetres.X),
				StarForge::Units::MetresToCm(OriginMetres.Y + Carrier.LocationInPlaceMetres.Y),
				StarForge::Units::MetresToCm(OriginMetres.Z + Carrier.LocationInPlaceMetres.Z));

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			// Requested rather than Required_Fatal, as on the loop rig: a taken name is not
			// worth a crash, and nothing depends on it beyond the readability of a log line.
			Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
			Params.Name = Carrier.CarrierName;

			AActor* Spawned = World->SpawnActor<AActor>(
				Carrier.CarrierClass, FTransform(Carrier.Rotation, WorldCm), Params);

			if (!Spawned)
			{
				UE_LOG(LogStarForge, Error,
					TEXT("Place carrier: '%s' of class %s did not spawn."),
					*Carrier.CarrierName.ToString(), *Carrier.CarrierClass->GetName());
				continue;
			}

			SpawnedCarriers.Add(Spawned);

			// What actually makes an actor a carrier is the frame facade: it is what answers
			// "where is this frame" and what the carry composes against. An actor without one
			// looks like a carrier on screen and is not one, and the only other way to find out
			// is to try standing on it at 400 m/s.
			const bool bHasFrame = (Spawned->FindComponentByClass<USFShipFrameComponent>() != nullptr);
			if (!bHasFrame)
			{
				UE_LOG(LogStarForge, Warning,
					TEXT("Place carrier: '%s' of class %s has no USFShipFrameComponent, so nobody ")
					TEXT("can stand in its frame."),
					*Carrier.CarrierName.ToString(), *Carrier.CarrierClass->GetName());
			}

			UE_LOG(LogStarForge, Display,
				TEXT("Place carrier: '%s' of class %s in place '%s' at place-local (%.1f %.1f %.1f) m, ")
				TEXT("world (%.6e %.6e %.6e) cm, frame %s -- motionless (%s)."),
				*Carrier.CarrierName.ToString(), *Carrier.CarrierClass->GetName(),
				*PlaceName.ToString(),
				Carrier.LocationInPlaceMetres.X, Carrier.LocationInPlaceMetres.Y, Carrier.LocationInPlaceMetres.Z,
				WorldCm.X, WorldCm.Y, WorldCm.Z,
				bHasFrame ? TEXT("present") : TEXT("MISSING"),
				USFPlaceDataAsset::KindName(Kind));
		}
	}

	bool bDocked = false;
	if (bFits)
	{
		FActorSpawnParameters ShipParams;
		ShipParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ShipParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		ShipParams.Name = TEXT("Ship");

		// Spawned at the place's origin and immediately moved by the docking maths. The docked
		// pose is not authored here for the loop rig's reason: it is derived from the two ports,
		// so moving a wall or a port moves the docked ship with it, and a constant would
		// silently stop tracking them.
		const FVector PlaceOriginCm(
			StarForge::Units::MetresToCm(OriginMetres.X),
			StarForge::Units::MetresToCm(OriginMetres.Y),
			StarForge::Units::MetresToCm(OriginMetres.Z));

		Ship = World->SpawnActor<ASFTestShip>(
			ASFTestShip::StaticClass(), FTransform(PlaceOriginCm), ShipParams);

		// Before Super::StartPlay, exactly as on the loop rig: Super is what begins play on the
		// world and spawns the pawn, and docking afterwards would put the pawn in the world while
		// the ship still sat at the place's origin inside the station's interior volume, with
		// nothing deciding which of the two claimed him.
		if (Ship && Ship->GetDocking())
		{
			Ship->GetDocking()->DockImmediately();

			// Asked of the component afterwards rather than assumed from the call: docking
			// immediately still has to resolve two ports, and a station that failed to spawn
			// leaves it Free with the ship adrift at the place's origin.
			bDocked = Ship->GetDocking()->IsDocked();
			if (!bDocked)
			{
				UE_LOG(LogStarForge, Error,
					TEXT("Place rig: the ship did not dock and is adrift at the origin of the place."));
			}
		}
		else
		{
			UE_LOG(LogStarForge, Error,
				TEXT("Place rig: no ship or no docking component; the rig cannot start docked."));
		}
	}

	Super::StartPlay();

	if (!bFits)
	{
		// One line, and it does not borrow the healthy line's three states. "The ship is ADRIFT",
		// "the man is NOT in the cabin" and "NOT BOUND to its frame" describe things that were
		// built and then broke; here they were never built, and saying them would read as four
		// findings where there is one. The engine says nothing at all about a pawn it did not
		// spawn -- the branch is closed by a guard on the empty class (GameModeBase.cpp 1264) --
		// so the absence of the man can only be stated here.
		UE_LOG(LogStarForge, Error,
			TEXT("Place rig: place '%s' REFUSED (%s) -- NOTHING WAS ASSEMBLED. No carriers, no ship, ")
			TEXT("and no pawn: DefaultPawnClass was nulled before Super::StartPlay so that nobody ")
			TEXT("stands in an empty world measuring a place that does not exist."),
			*PlaceName.ToString(), StarForge::Place::RefusalName(Fit.Refusal));
		return;
	}

	bool bPawnInCabin = false;
	bool bPawnBound = false;
	FString BoundToName = TEXT("nothing");
	if (Ship)
	{
		APlayerController* PC = World->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (Pawn)
		{
			// He appears at the world origin, which on this rig is inside the station, and is
			// moved into the cabin here. That move is a genuine crossing of a frame boundary and
			// produces the run's first Frame handover line -- Station -> Ship, zero centimetres,
			// because neither carrier is moving.
			const FVector Cabin = Ship->GetInteriorSpawnWorldLocation();
			Pawn->SetActorLocation(Cabin);

			// Read back for the same reason the profile is: the rig's closing line names where
			// the man is standing, and a move nobody checked is not a place.
			bPawnInCabin = Pawn->GetActorLocation().Equals(Cabin, 1.0);
			if (!bPawnInCabin)
			{
				UE_LOG(LogStarForge, Error,
					TEXT("Place rig: the pawn did not reach the cabin -- asked for (%.0f %.0f %.0f), ")
					TEXT("stands at (%.0f %.0f %.0f) cm."),
					Cabin.X, Cabin.Y, Cabin.Z,
					Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, Pawn->GetActorLocation().Z);
			}

			// Standing in the cabin is not the whole claim on a rig with two frames a man could
			// be bound to. The carry follows the frame he is bound to, not the room he is in, so
			// the binding is read back as well.
			const ASFCharacter* Occupant = Cast<ASFCharacter>(Pawn);
			const USFShipboardMovementComponent* Movement = Occupant ? Occupant->GetShipboardMovement() : nullptr;
			const USFShipFrameComponent* BoundTo = Movement ? Movement->GetShipFrame() : nullptr;
			bPawnBound = BoundTo && BoundTo == Ship->GetFrameComponent();
			BoundToName = BoundTo ? GetNameSafe(BoundTo->GetOwner()) : FString(TEXT("nothing"));
		}
		else
		{
			// Silence here would leave him at the world origin, which on this rig is INSIDE THE
			// STATION rather than in the ship -- the checkpoint would then measure a man in the
			// wrong room while every other line read healthy.
			UE_LOG(LogStarForge, Error,
				TEXT("Place rig: no %s, so nobody was moved into the cabin; ")
				TEXT("anyone who appears later stands at the world origin, inside the station."),
				PC ? TEXT("pawn") : TEXT("player controller"));
		}
	}

	// Counted from what stands in the world rather than from what the data asked for: the two
	// differing is itself the finding, and a line reading the asset back to itself is not one.
	FString CarriersText;
	const bool bCarriersShort = (SpawnedCarriers.Num() != Carriers.Num());
	if (bCarriersShort)
	{
		CarriersText = FString::Printf(TEXT("assembled with %d of %d carriers -- %d MISSING"),
			SpawnedCarriers.Num(), Carriers.Num(), Carriers.Num() - SpawnedCarriers.Num());
	}
	else
	{
		CarriersText = FString::Printf(TEXT("assembled with %d carrier%s"),
			SpawnedCarriers.Num(), SpawnedCarriers.Num() == 1 ? TEXT("") : TEXT("s"));
	}

	const FString ManText = FString::Printf(TEXT("%s and %s"),
		bPawnInCabin ? TEXT("stands in the cabin") : TEXT("is NOT in the cabin"),
		bPawnBound ? TEXT("is bound to its frame")
			: *FString::Printf(TEXT("is NOT BOUND to its frame (bound to %s)"), *BoundToName));

	// The startup line the runbook sends the operator to look for, and every state in it was read
	// back rather than attempted. A rig which cannot verify its own setup is not an instrument.
	const FString Report = FString::Printf(
		TEXT("Place rig: scripted profile %s; place '%s' read from %s and %s; the ship %s; the man %s."),
		bProfileOff ? TEXT("off") : TEXT("STILL ON"),
		*PlaceName.ToString(),
		bFromAsset ? *PlaceData->GetName() : TEXT("a place written in code"),
		*CarriersText,
		bDocked ? TEXT("spawns docked") : TEXT("is ADRIFT"),
		*ManText);

	if (!bProfileOff || bCarriersShort || !bDocked || !bPawnInCabin || !bPawnBound)
	{
		UE_LOG(LogStarForge, Error, TEXT("%s"), *Report);
	}
	else if (!bFromAsset)
	{
		// Asked for by circumstance rather than broken, the way sf.Pirate 0 is on the combat rig:
		// the world is the one the data describes, but the data is not yet an asset anyone edited.
		UE_LOG(LogStarForge, Warning, TEXT("%s"), *Report);
	}
	else
	{
		UE_LOG(LogStarForge, Display, TEXT("%s"), *Report);
	}
}
