#include "SFPlaceGameMode.h"

#include "Debug/SFFrameDriftHUD.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "NumberText.h"
#include "Place.h"
#include "Player/SFCharacter.h"
#include "Player/SFShipboardMovementComponent.h"
#include "Ship/SFDockingComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFTestShip.h"
#include "Ship/SFTestStation.h"
#include "StarForgeLog.h"
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
	 * sf.BindFrame and sf.AimAt are the precedent for a string variable; ParseMetres below is the
	 * parse (StarForge::Parse::TryParseFiniteNumber, never Atod), and what it parsed is printed back.
	 *
	 * Read ONCE, when the rig assembles itself, which is before -ExecCmds is applied: set it
	 * before the level starts, with "-ini:Engine:[ConsoleVariables]:sf.PlaceOffsetM=..." on the
	 * command line, or in the editor's console before Play. One -ini switch per variable: two
	 * joined by a comma are not taken. A change after the measurement is watched for and said
	 * once, as a warning, because it changes nothing and would otherwise say nothing.
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

	/**
	 * Which place asset the rig reads, by its bare name in /Game/Places/ -- DA_Place_Berth, not a
	 * path. A bare name because MSYS rewrites an argument of the form x=/path, and a path mangled on
	 * the way in would refuse the place for a reason nobody typed. Empty keeps DA_Place_OpenSpace,
	 * so the rig's default does not move by a line (station-kit spec 6.6).
	 *
	 * Read ONCE, in InitGame, for sf.PlaceOffsetM's reason: set it with
	 * "-ini:Engine:[ConsoleVariables]:sf.PlaceAsset=DA_Place_Berth" on the command line.
	 */
	TAutoConsoleVariable<FString> CVarPlaceAsset(TEXT("sf.PlaceAsset"), TEXT(""),
		TEXT("The place asset the place rig reads, by its bare name in /Game/Places/ (DA_Place_Berth). Empty: DA_Place_OpenSpace.\n")
		TEXT("Read in InitGame, before -ExecCmds applies: set it with -ini on the command line."),
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

		/**
		 * Where the asset is looked for, named once so the lookup and the warning cannot drift: the
		 * warning prints the package this path names rather than a copy of it.
		 */
		const TCHAR* AssetPath = TEXT("/Game/Places/DA_Place_OpenSpace.DA_Place_OpenSpace");

		/** Where sf.PlaceAsset's bare names are looked for. */
		const TCHAR* PlaceAssetDirectory = TEXT("/Game/Places");

		/**
		 * Reads a metre figure out of a string console variable.
		 *
		 * Returns false on empty, which is "no command", and on anything that is not a finite
		 * number written out, which is said out loud by the caller. A silent fall-back to the
		 * authored value would let a negative control be performed with a typo in it and count
		 * as having found nothing.
		 */
		bool ParseMetres(const FString& Raw, double& OutMetres)
		{
			if (Raw.IsEmpty())
			{
				return false;
			}

			// The strict reader, and the history of this call is the whole reason. Atod cannot
			// fail: it answers 0.0 for a string with no number in it at all, so the first form of
			// this rig accepted sf.PlaceOffsetM=nonsense, moved the place by zero metres, and
			// announced "the console changed the place" over a place the console had not touched.
			// That is worse than passing in silence -- it is a control reporting that it did the
			// thing it did not do.
			//
			// FDefaultValueHelper::ParseDouble came next and refused "nonsense", but every part of
			// its grammar is optional (IsStringValidFloat, DefaultValueHelper.cpp 268-308): "." and
			// "e10" passed, converted through the same Atod, and sf.PlaceOffsetM=. brought
			// the same false report of a move of zero metres back (review of 2026-09-23, reproduced
			// by a run). TryParseFiniteNumber needs a digit in the mantissa and in any exponent and
			// refuses an overflow to infinity, so "4.3e10" and "-1.5" are taken and those warn.
			return StarForge::Parse::TryParseFiniteNumber(Raw, OutMetres);
		}
	}
}

ASFPlaceGameMode::ASFPlaceGameMode()
{
	DefaultPawnClass = ASFCharacter::StaticClass();
	HUDClass = ASFFrameDriftHUD::StaticClass();
}

void ASFPlaceGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// The measurement lives here rather than in StartPlay because of the order the engine runs a
	// standalone -game session in. UEngine::LoadMap calls InitializeActorsForPlay, which calls
	// InitGame (World.cpp 6034); then LoadMap logs the local player in through SpawnPlayActor
	// (UnrealEngine.cpp 16647) -- PostLogin, HandleStartingNewPlayer, RestartPlayer, and the pawn
	// is spawned -- and only then does it call World->BeginPlay, which calls StartPlay
	// (World.cpp 6174). The first form of this rig measured in StartPlay and nulled
	// DefaultPawnClass there, which was already too late: every refused run carried an ASFCharacter
	// and its warning about standing in world space, under a line claiming no pawn existed.
	// InitGame is the only hook ahead of the login. Measured after the move: the size line is
	// written two lines before FindPlayerStart in the log, at both frame rates, and no character
	// exists on a refused place.
	PlaceName = PlaceRig::FallbackPlaceName;
	PlaceKind = ESFPlaceKind::OpenSpace;
	PlaceSystemId = NAME_None;
	PlaceOriginMetres = FVector::ZeroVector;
	PlaceRadiusMetres = PlaceRig::FallbackRadiusMetres;
	PlaceCarriers.Reset();

	// By path and guarded, for ASFTestShip's two reasons -- the map is empty, so there is no placed
	// actor to author this on, and the asset may not exist -- but here, on every InitGame, and not
	// through a static ConstructorHelpers finder in the constructor. That finder ran once per
	// process, when the class default object was built, so an editor started before the asset
	// existed went on building the place written in code, under the warning below that there was no
	// asset, in every Play until it was restarted. NoWarn because the miss is this rig's line to say.
	// sf.PlaceAsset, below, replaces it when it names another asset.
	PlaceData = LoadObject<USFPlaceDataAsset>(nullptr, PlaceRig::AssetPath, nullptr, LOAD_NoWarn);

	PlaceShipStart = FSFShipStart();
	AssetRefusal.Reset();
	RequestedAssetName = CVarPlaceAsset.GetValueOnGameThread();

	if (!RequestedAssetName.IsEmpty())
	{
		// A name, not a path. A slash means the value was meant as a path, which is exactly what
		// this variable refuses to carry; refused rather than trimmed, because a trimmed value
		// names a place nobody asked for.
		if (RequestedAssetName.Contains(TEXT("/")) || RequestedAssetName.Contains(TEXT("\\")))
		{
			AssetRefusal = TEXT("not a bare asset name");
		}
		else
		{
			const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"),
				PlaceRig::PlaceAssetDirectory, *RequestedAssetName, *RequestedAssetName);
			if (USFPlaceDataAsset* Named = LoadObject<USFPlaceDataAsset>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn))
			{
				PlaceData = Named;
			}
			else
			{
				AssetRefusal = TEXT("no such asset");
			}
		}
	}

	if (!AssetRefusal.IsEmpty())
	{
		// Refused before anything is measured, and nothing is put in its stead. The code-written
		// place and DA_Place_OpenSpace are other places, and their figures, assembled under this
		// name, would be credited to the asset that was asked for. No size line either: there is
		// nothing to measure, and a size line here would describe the wrong place.
		bPlaceMeasured = true;
		bPlaceFits = false;
		bPlaceFromAsset = false;

		// What the two size variables held at the refusal, so that OnPlaceVariableChanged answers a
		// real change and not a Set of the value they already had.
		OffsetRawAtMeasure = CVarPlaceOffsetM.GetValueOnGameThread();
		RadiusRawAtMeasure = CVarPlaceRadiusM.GetValueOnGameThread();

		const FString What = (AssetRefusal == TEXT("no such asset"))
			? FString::Printf(TEXT("names no place asset at %s/%s"), PlaceRig::PlaceAssetDirectory, *RequestedAssetName)
			: FString(TEXT("is not a bare asset name"));

		UE_LOG(LogStarForge, Error,
			TEXT("Place rig: sf.PlaceAsset='%s' %s -- REFUSED before anything was measured; nothing will be ")
			TEXT("assembled and the pawn class is withdrawn."),
			*RequestedAssetName, *What);
		return;
	}

	bPlaceFromAsset = (PlaceData != nullptr);
	if (bPlaceFromAsset)
	{
		PlaceName = PlaceData->PlaceName;
		PlaceKind = PlaceData->Kind;
		PlaceSystemId = PlaceData->SystemId;
		PlaceOriginMetres = PlaceData->OriginFromWorldOriginMetres;
		PlaceRadiusMetres = PlaceData->RadiusMetres;
		PlaceCarriers = PlaceData->Carriers;
		PlaceShipStart = PlaceData->ShipStart;
	}
	else
	{
		// Not a stub: the same place, described by the same fields, measured by the same
		// predicate. Said out loud because a rig that quietly builds something other than what
		// the data says is a rig whose every figure belongs to an unknown world.
		FSFPlaceCarrier Station;
		Station.CarrierClass = ASFTestStation::StaticClass();
		Station.CarrierName = PlaceRig::StationName;
		PlaceCarriers.Add(Station);

		UE_LOG(LogStarForge, Warning,
			TEXT("Place rig: no place asset at %s, so a place written ")
			TEXT("in code was used instead. Its figures are the asset's; the size line below is what ")
			TEXT("the two have to agree on."),
			*FPackageName::ObjectPathToPackageName(FString(PlaceRig::AssetPath)));
	}

	FString OverrideText;

	double OffsetM = 0.0;
	const FString OffsetRaw = CVarPlaceOffsetM.GetValueOnGameThread();
	OffsetRawAtMeasure = OffsetRaw;
	if (!OffsetRaw.IsEmpty())
	{
		if (PlaceRig::ParseMetres(OffsetRaw, OffsetM))
		{
			PlaceOriginMetres.X += OffsetM;
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
	RadiusRawAtMeasure = RadiusRaw;
	if (!RadiusRaw.IsEmpty())
	{
		if (PlaceRig::ParseMetres(RadiusRaw, OverrideRadiusM))
		{
			// Replaces rather than adds, because the authored radius is what a control needs to
			// be rid of: one that could only grow it could never ask for a smaller place.
			PlaceRadiusMetres = OverrideRadiusM;
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
	PlaceFit = StarForge::Place::MeasurePlace(PlaceOriginMetres, PlaceRadiusMetres, Limits);

	bPlaceFits = (PlaceFit.Refusal == StarForge::Place::EPlaceRefusal::None);
	bPlaceMeasured = true;

	// One line per place, fitting or not. Spec 10.4: a line that appears only in trouble cannot
	// be told apart from an instrument that never ran.
	const FString SizeLine = FString::Printf(
		TEXT("Place size: '%s' (%s, system '%s') origin (%.6e %.6e %.6e) m, radius %.6e m; ")
		TEXT("reach %.6e m of the %.6e m engine ceiling, margin %.6e m; ")
		TEXT("diameter %.6e m of the %.6e m crossing ceiling (%.1f s at %.6e m/s), margin %.6e m; ")
		TEXT("the slowest hull crosses it in %.6e s -- %s.%s"),
		*PlaceName.ToString(),
		USFPlaceDataAsset::KindName(PlaceKind),
		PlaceSystemId.IsNone() ? TEXT("unnamed") : *PlaceSystemId.ToString(),
		PlaceOriginMetres.X, PlaceOriginMetres.Y, PlaceOriginMetres.Z,
		PlaceRadiusMetres,
		PlaceFit.ReachMetres, PlaceFit.EngineCeilingMetres, PlaceFit.EngineMarginMetres,
		PlaceFit.DiameterMetres, PlaceFit.CrossingCeilingMetres,
		Limits.CrossingBudgetSeconds, Limits.SlowestHullSpeedMetresPerSec,
		PlaceFit.CrossingMarginMetres,
		PlaceFit.CrossingSeconds,
		bPlaceFits ? TEXT("fits") : TEXT("REFUSED"),
		bPlaceFits ? TEXT("") : *FString::Printf(TEXT(" Reason: %s."),
			StarForge::Place::RefusalName(PlaceFit.Refusal)));

	if (bPlaceFits)
	{
		UE_LOG(LogStarForge, Display, TEXT("%s"), *SizeLine);
	}
	else
	{
		// An Error, and deliberately so: spec 13 asks that a place beyond the border refuse to
		// assemble with a NAMED ERROR before anything undocks. The control run therefore carries
		// one, and the runbook lists it as legitimately red with the condition under which it
		// becomes a finding again -- the same treatment the combat rig's legitimately red lines get.
		UE_LOG(LogStarForge, Error, TEXT("%s"), *SizeLine);
	}
}

UClass* ASFPlaceGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// The engine's guard is real -- RestartPlayerAtPlayerStart spawns a default pawn only when
	// this hook answers a class (GameModeBase.cpp 1291) -- but it is asked at LOGIN, so the class
	// has to be gone by then. Withdrawing it here rather than assigning DefaultPawnClass keeps the
	// constructor's class untouched: on a place that fits, this answers exactly what it always did.
	if (bPlaceMeasured && !bPlaceFits)
	{
		return nullptr;
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
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
		// Kept for EndPlay, which puts it back: see SFLoopGameMode.cpp for why a 0 left behind
		// stops the spike rig's manoeuvre in every later session of the same editor.
		ProfileBeforeRig = Profile->GetInt();
		bRestoreProfile = true;

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

	// The place was measured in InitGame, before the local player logged in, and its size line is
	// already in the log by the time this runs. What is left here is the assembly.
	if (bPlaceFits)
	{
		for (const FSFPlaceCarrier& Carrier : PlaceCarriers)
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
			if (FromPlaceOrigin > PlaceRadiusMetres)
			{
				UE_LOG(LogStarForge, Warning,
					TEXT("Place carrier: '%s' stands %.6e m from the origin of place '%s', which is ")
					TEXT("outside its %.6e m radius. The size line above does not cover it."),
					*Carrier.CarrierName.ToString(), FromPlaceOrigin,
					*PlaceName.ToString(), PlaceRadiusMetres);
			}

			const FVector WorldCm(
				StarForge::Units::MetresToCm(PlaceOriginMetres.X + Carrier.LocationInPlaceMetres.X),
				StarForge::Units::MetresToCm(PlaceOriginMetres.Y + Carrier.LocationInPlaceMetres.Y),
				StarForge::Units::MetresToCm(PlaceOriginMetres.Z + Carrier.LocationInPlaceMetres.Z));

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

			// The name the actor GOT, which is the one every later line about it carries -- the
			// handover line above all. Requested mode hands out another when the asked-for name is
			// taken or none was asked for, and a line naming the request would then describe an
			// actor nobody else's line mentions. The request is added only when the two differ.
			const FString SpawnedName = Spawned->GetName();
			const FString AskedText = (Spawned->GetFName() == Carrier.CarrierName)
				? FString()
				: FString::Printf(TEXT(" (asked for '%s')"), *Carrier.CarrierName.ToString());

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
					*SpawnedName, *Carrier.CarrierClass->GetName());
			}

			UE_LOG(LogStarForge, Display,
				TEXT("Place carrier: '%s'%s of class %s in place '%s' at place-local (%.1f %.1f %.1f) m, ")
				TEXT("world (%.6e %.6e %.6e) cm, frame %s -- motionless (%s)."),
				*SpawnedName, *AskedText, *Carrier.CarrierClass->GetName(),
				*PlaceName.ToString(),
				Carrier.LocationInPlaceMetres.X, Carrier.LocationInPlaceMetres.Y, Carrier.LocationInPlaceMetres.Z,
				WorldCm.X, WorldCm.Y, WorldCm.Z,
				bHasFrame ? TEXT("present") : TEXT("MISSING"),
				USFPlaceDataAsset::KindName(PlaceKind));
		}
	}

	// Hoisted: the free start is measured from it, and the rig line reads the ship back against it.
	const FVector PlaceOriginCm(
		StarForge::Units::MetresToCm(PlaceOriginMetres.X),
		StarForge::Units::MetresToCm(PlaceOriginMetres.Y),
		StarForge::Units::MetresToCm(PlaceOriginMetres.Z));
	const bool bStartsFree = (PlaceShipStart.Mode == ESFShipStartMode::Free);

	bool bDocked = false;
	if (bPlaceFits)
	{
		FActorSpawnParameters ShipParams;
		ShipParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ShipParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		ShipParams.Name = TEXT("Ship");

		// Spawned at the place's origin and immediately moved by the docking maths. The docked
		// pose is not authored here for the loop rig's reason: it is derived from the two ports,
		// so moving a wall or a port moves the docked ship with it, and a constant would
		// silently stop tracking them. A place that asks for a free start (station-kit spec 6.6)
		// gets the pose it wrote, and nothing docks the ship.
		const FTransform ShipSpawn = bStartsFree
			? FTransform(PlaceShipStart.Rotation, PlaceOriginCm + FVector(
				StarForge::Units::MetresToCm(PlaceShipStart.LocationInPlaceMetres.X),
				StarForge::Units::MetresToCm(PlaceShipStart.LocationInPlaceMetres.Y),
				StarForge::Units::MetresToCm(PlaceShipStart.LocationInPlaceMetres.Z)))
			: FTransform(PlaceOriginCm);

		Ship = World->SpawnActor<ASFTestShip>(ASFTestShip::StaticClass(), ShipSpawn, ShipParams);

		// Before Super::StartPlay, exactly as on the loop rig: Super is what begins play on the
		// world and spawns the pawn, and docking afterwards would put the pawn in the world while
		// the ship still sat at the place's origin inside the station's interior volume, with
		// nothing deciding which of the two claimed him.
		if (Ship && Ship->GetDocking())
		{
			if (!bStartsFree)
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
		}
		else
		{
			UE_LOG(LogStarForge, Error,
				TEXT("Place rig: no ship or no docking component; the rig cannot start docked."));
		}
	}

	Super::StartPlay();

	// Watched from here, on a refused place as on one that fits -- a radius typed mid-run to rescue
	// a refusal is the likeliest change of all -- and from here because this mode has now begun
	// play, so EndPlay is certain to take the watch off (AActor::RouteEndPlay calls EndPlay only on
	// an actor that has begun).
	OffsetChangedHandle = CVarPlaceOffsetM.AsVariable()->OnChangedDelegate().AddUObject(
		this, &ASFPlaceGameMode::OnPlaceVariableChanged);
	RadiusChangedHandle = CVarPlaceRadiusM.AsVariable()->OnChangedDelegate().AddUObject(
		this, &ASFPlaceGameMode::OnPlaceVariableChanged);

	if (!bPlaceFits)
	{
		// One line, and it does not borrow the healthy line's three states. "The ship is ADRIFT",
		// "the man is NOT in the cabin" and "NOT BOUND to its frame" describe things that were
		// built and then broke; here they were never built, and saying them would read as four
		// findings where there is one. The engine says nothing at all about a pawn it did not
		// spawn, so the absence of the man can only be stated here.
		//
		// READ BACK, never asserted. The line used to claim "no pawn" on the strength of having
		// asked for one, and it was wrong in every refused run ever recorded: the login runs
		// before StartPlay, so the pawn was already standing in the empty world while the line
		// said it was not. A rig that states what it asked for instead of what it got is not an
		// instrument, and this is the one line a reader has to be able to trust on a refusal.
		const APlayerController* PC = World->GetFirstPlayerController();
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;

		const FString PawnText = Pawn
			? FString::Printf(
				TEXT("a PAWN STANDS in the empty world: %s was spawned despite the refusal, and its ")
				TEXT("instruments are reporting figures about a place that does not exist"),
				*GetNameSafe(Pawn))
			: FString(
				TEXT("no pawn: the pawn class was withdrawn before the local player logged in, so ")
				TEXT("the engine had none to spawn and nobody stands in an empty world measuring a ")
				TEXT("place that does not exist"));

		if (!AssetRefusal.IsEmpty())
		{
			// Its own line, because the size refusal's names the place and its size reason, and here
			// there is neither: the place is OpenSpace only by default, and "none" is not a reason.
			// The size refusal's line below stays as it was to the character -- the size controls of
			// stage 5 and their evidence logs stand on it.
			UE_LOG(LogStarForge, Error,
				TEXT("Place rig: place asset '%s' REFUSED (%s) -- NOTHING WAS ASSEMBLED. No carriers, no ship, ")
				TEXT("and %s."),
				*RequestedAssetName, *AssetRefusal, *PawnText);
			return;
		}

		UE_LOG(LogStarForge, Error,
			TEXT("Place rig: place '%s' REFUSED (%s) -- NOTHING WAS ASSEMBLED. No carriers, no ship, ")
			TEXT("and %s."),
			*PlaceName.ToString(), StarForge::Place::RefusalName(PlaceFit.Refusal), *PawnText);
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
			FVector Cabin = Ship->GetInteriorSpawnWorldLocation();
			if (!PlaceShipStart.OccupantOffsetShipMetres.IsZero())
			{
				// In the ship's axes, so "towards the hatch wall" means the same whatever way the
				// hull is turned. Added only when asked for: a zero added to a coordinate turns a
				// -0.0 into +0.0, and the docked rig must stand him on the very point it did.
				Cabin += Ship->GetActorQuat().RotateVector(FVector(
					StarForge::Units::MetresToCm(PlaceShipStart.OccupantOffsetShipMetres.X),
					StarForge::Units::MetresToCm(PlaceShipStart.OccupantOffsetShipMetres.Y),
					StarForge::Units::MetresToCm(PlaceShipStart.OccupantOffsetShipMetres.Z)));
			}
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
	const bool bCarriersShort = (SpawnedCarriers.Num() != PlaceCarriers.Num());
	if (bCarriersShort)
	{
		CarriersText = FString::Printf(TEXT("assembled with %d of %d carriers -- %d MISSING"),
			SpawnedCarriers.Num(), PlaceCarriers.Num(), PlaceCarriers.Num() - SpawnedCarriers.Num());
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

	// Read back from the hull, not from the asset: a ship asked to start somewhere and standing
	// somewhere else is exactly what this line exists to show.
	FString ShipText = bDocked ? FString(TEXT("spawns docked")) : FString(TEXT("is ADRIFT"));
	if (bStartsFree && Ship)
	{
		const FVector AtCm = Ship->GetActorLocation() - PlaceOriginCm;
		ShipText = FString::Printf(TEXT("starts free at place-local (%.1f %.1f %.1f) m, as the place asks"),
			StarForge::Units::CmToMetres(AtCm.X), StarForge::Units::CmToMetres(AtCm.Y),
			StarForge::Units::CmToMetres(AtCm.Z));
	}
	const bool bShipWrong = bStartsFree ? (Ship == nullptr) : !bDocked;

	// The startup line the runbook sends the operator to look for, and every state in it was read
	// back rather than attempted. A rig which cannot verify its own setup is not an instrument.
	const FString Report = FString::Printf(
		TEXT("Place rig: scripted profile %s; place '%s' read from %s and %s; the ship %s; the man %s."),
		bProfileOff ? TEXT("off") : TEXT("STILL ON"),
		*PlaceName.ToString(),
		bPlaceFromAsset ? *PlaceData->GetName() : TEXT("a place written in code"),
		*CarriersText,
		*ShipText,
		*ManText);

	if (!bProfileOff || bCarriersShort || bShipWrong || !bPawnInCabin || !bPawnBound)
	{
		UE_LOG(LogStarForge, Error, TEXT("%s"), *Report);
	}
	else if (!bPlaceFromAsset)
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

void ASFPlaceGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CVarPlaceOffsetM.AsVariable()->OnChangedDelegate().Remove(OffsetChangedHandle);
	CVarPlaceRadiusM.AsVariable()->OnChangedDelegate().Remove(RadiusChangedHandle);
	OffsetChangedHandle.Reset();
	RadiusChangedHandle.Reset();

	// At SetByCode, and only while nothing stronger holds the variable, for the reasons
	// SFLoopGameMode.cpp's EndPlay gives. No line.
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

StarForge::Place::EPlaceRefusal ASFPlaceGameMode::GetPlaceRefusal() const
{
	return (bPlaceMeasured && !bPlaceFits) ? PlaceFit.Refusal : StarForge::Place::EPlaceRefusal::None;
}

void ASFPlaceGameMode::OnPlaceVariableChanged(IConsoleVariable* Variable)
{
	const bool bOffset = (Variable == CVarPlaceOffsetM.AsVariable());
	if (!bOffset && Variable != CVarPlaceRadiusM.AsVariable())
	{
		return;
	}

	// The delegate fires on every Set, the same value included, and a value the place was measured
	// with is not one it ignored. Said once per variable per world: the point is that the switch did
	// nothing, and one line makes it.
	bool& bReported = bOffset ? bOffsetChangeReported : bRadiusChangeReported;
	const FString& AtMeasure = bOffset ? OffsetRawAtMeasure : RadiusRawAtMeasure;
	const FString Now = Variable->GetString();
	if (bReported || Now == AtMeasure)
	{
		return;
	}
	bReported = true;

	// A place refused by name was never measured, and there is no size line for it to describe.
	if (!AssetRefusal.IsEmpty())
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Place rig: %s changed to '%s' -- it is read once, before the local player logs in, and this ")
			TEXT("world's place was refused by name (sf.PlaceAsset='%s') before anything was measured, so nothing ")
			TEXT("changes until the level restarts."),
			bOffset ? TEXT("sf.PlaceOffsetM") : TEXT("sf.PlaceRadiusM"), *Now, *RequestedAssetName);
		return;
	}

	UE_LOG(LogStarForge, Warning,
		TEXT("Place rig: %s changed to '%s' after the place was measured with '%s' -- it is read once, ")
		TEXT("before the local player logs in, so this world stays the place the size line describes ")
		TEXT("until the level restarts."),
		bOffset ? TEXT("sf.PlaceOffsetM") : TEXT("sf.PlaceRadiusM"), *Now, *AtMeasure);
}
