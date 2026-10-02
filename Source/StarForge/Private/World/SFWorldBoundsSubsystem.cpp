#include "World/SFWorldBoundsSubsystem.h"

#include "Engine/World.h"
#include "EngineDefines.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "StarForgeLog.h"
#include "Units.h"

#include <cmath>

namespace
{
	const TCHAR* const DisableChecksName = TEXT("WorldBounds.DisableChecks");

	IConsoleVariable* FindDisableChecks()
	{
		return IConsoleManager::Get().FindConsoleVariable(DisableChecksName);
	}

	/**
	 * The world box's half-size, derived from the engine's own constant rather than written as
	 * "2^42", so a future engine that moves it cannot make the line lie. Not a power of two, and
	 * the raw figure is printed instead.
	 */
	FString WorldBoxText()
	{
		int Exponent = 0;
		const double Mantissa = std::frexp(static_cast<double>(HALF_WORLD_MAX), &Exponent);

		if (Mantissa == 0.5)
		{
			return FString::Printf(TEXT("2^%d cm"), Exponent - 1);
		}

		return FString::Printf(TEXT("%.0f cm"), static_cast<double>(HALF_WORLD_MAX));
	}
}

void USFWorldBoundsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (IConsoleVariable* Variable = FindDisableChecks())
	{
		DisableChecksChangedHandle =
			Variable->OnChangedDelegate().AddUObject(this, &USFWorldBoundsSubsystem::OnDisableChecksChanged);
	}
}

void USFWorldBoundsSubsystem::Deinitialize()
{
	if (IConsoleVariable* Variable = FindDisableChecks())
	{
		Variable->OnChangedDelegate().Remove(DisableChecksChangedHandle);
	}
	DisableChecksChangedHandle.Reset();

	Super::Deinitialize();
}

void USFWorldBoundsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	Report();
}

bool USFWorldBoundsSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void USFWorldBoundsSubsystem::OnDisableChecksChanged(IConsoleVariable* Variable)
{
	// The subsystem's own flag rather than the world's HasBegunPlay: the world reports begun only
	// from inside the game mode's StartPlay, after this subsystem's first line, and a change in
	// between would be a change nobody was told about.
	if (HasCalledBeginPlay())
	{
		Report();
	}
}

void USFWorldBoundsSubsystem::Report()
{
	// The settings CheckStillInWorld itself reads (Actor.cpp 2319), unchecked so that a missing
	// one is a line in the log rather than an assert.
	const AWorldSettings* WorldSettings = GetWorld()->GetWorldSettings(
		/*bCheckStreamingPersistent=*/ true, /*bChecked=*/ false);

	if (!WorldSettings)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("World bounds: NO WORLD SETTINGS in this world -- whether anything is destroyed at ")
			TEXT("KillZ or outside the world box cannot be read."));
		return;
	}

	const bool bChecksOn = WorldSettings->AreWorldBoundsChecksEnabled();
	const bool bMapFlag = WorldSettings->bEnableWorldBoundsChecks;

	const IConsoleVariable* Variable = FindDisableChecks();
	const FString VariableText = Variable ? FString::FromInt(Variable->GetInt()) : FString(TEXT("ABSENT"));

	if (bHasReported && bChecksOn == bReportedChecksOn && bMapFlag == bReportedMapFlag
		&& VariableText == ReportedVariableText)
	{
		return;
	}

	bHasReported = true;
	bReportedChecksOn = bChecksOn;
	bReportedMapFlag = bMapFlag;
	ReportedVariableText = VariableText;

	const double KillZMetres = StarForge::Units::CmToMetres(WorldSettings->KillZ);
	const FString WorldBox = WorldBoxText();
	const TCHAR* MapFlagText = bMapFlag ? TEXT("on") : TEXT("off");

	if (bChecksOn)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("World bounds: checks ON (WorldBounds.DisableChecks %s, map flag %s) -- a simulating ")
			TEXT("body below KillZ %.2f m is destroyed without a word, and one outside %s with a ")
			TEXT("single LogActor warning."),
			*VariableText, MapFlagText, KillZMetres, *WorldBox);
	}
	else
	{
		UE_LOG(LogStarForge, Display,
			TEXT("World bounds: checks off (WorldBounds.DisableChecks %s, map flag %s) -- nothing is ")
			TEXT("destroyed at KillZ %.2f m or outside %s in this world."),
			*VariableText, MapFlagText, KillZMetres, *WorldBox);
	}
}
