#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/DefaultValueHelper.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "StarForgeLog.h"
#include "TimerManager.h"

namespace
{
	/**
	 * The longest stall sf.Hitch will make. Past AWorldSettings::MaxUndilatedFrameTime -- 0.4 s,
	 * BaseGame.ini 199, applied by FixupDeltaSeconds (WorldSettings.cpp 334-342) -- the world cuts
	 * the frame to that length whatever the wall clock did, so a longer stall measures nothing more
	 * about the frame. The bound sits above the clamp so a stall past it can still be asked for, which
	 * is how the clamp itself is seen, and low enough to refuse the typo -- 40000 for 40 -- that would
	 * freeze the process for tens of seconds with nothing in the log to say why.
	 */
	constexpr double MaxHitchMilliseconds = 1000.0;

	/**
	 * What can keep a stall out of the frame after it, named only when it is set and could have cut
	 * this stall. The engine has five: a fixed step (UnrealEngine.cpp 3005-3023), a fixed frame rate
	 * (3141-3146), UGameEngine::MaxDeltaTime (3168-3196), the world's MaxUndilatedFrameTime
	 * (FixupDeltaSeconds, above) and time dilation, which shortens the world's frame by its own factor
	 * (LevelTick.cpp 1596). A custom time step and t.OverrideFPS set the delta outright and are not
	 * read here, so a cut none of the five explains says so rather than naming nothing.
	 */
	FString DescribeFrameCuts(const UWorld* World, double StalledMilliseconds)
	{
		TArray<FString> Cuts;
		if (FApp::IsBenchmarking() || FApp::UseFixedTimeStep())
		{
			Cuts.Add(FString::Printf(TEXT("the step fixed at %.3f ms (-benchmark or -UseFixedTimeStep)"),
				FApp::GetFixedDeltaTime() * 1000.0));
		}
		if (GEngine && GEngine->bUseFixedFrameRate)
		{
			Cuts.Add(FString::Printf(TEXT("the frame rate fixed at %.1f fps (bUseFixedFrameRate)"), GEngine->FixedFrameRate));
		}
		const UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		if (GameEngine && GameEngine->MaxDeltaTime > 0.0f && GameEngine->MaxDeltaTime * 1000.0 < StalledMilliseconds)
		{
			Cuts.Add(FString::Printf(TEXT("the engine's %.1f ms (MaxDeltaTime)"), GameEngine->MaxDeltaTime * 1000.0));
		}
		if (const AWorldSettings* WorldSettings = World ? World->GetWorldSettings(/*bCheckStreamingPersistent=*/ false, /*bChecked=*/ false) : nullptr)
		{
			if (WorldSettings->MaxUndilatedFrameTime * 1000.0 < StalledMilliseconds)
			{
				Cuts.Add(FString::Printf(TEXT("the world's %.1f ms (MaxUndilatedFrameTime)"), WorldSettings->MaxUndilatedFrameTime * 1000.0));
			}
			const float Dilation = WorldSettings->GetEffectiveTimeDilation();
			if (Dilation < 1.0f)
			{
				Cuts.Add(FString::Printf(TEXT("time dilated by %.3f"), Dilation));
			}
		}
		return Cuts.IsEmpty()
			? FString(TEXT("something this line does not read (a custom time step, or t.OverrideFPS)"))
			: FString::Join(Cuts, TEXT(", "));
	}

	/**
	 * The line, read off the frame that carried the stall: the world's frame as every TickComponent
	 * is handed it, dilated and clamped (LevelTick.cpp 1596-1603), and the part of it Chaos
	 * integrated, min(dt, MaxPhysicsDeltaTime) (ChaosScene.cpp 344). A frame physics integrated whole
	 * is not the long frame, however long it was, and the line says which one this was.
	 */
	void ReportHitch(const UWorld* World, double AskedMilliseconds, double StalledMilliseconds)
	{
		const double FrameMilliseconds = (World ? World->GetDeltaSeconds() : FApp::GetDeltaTime()) * 1000.0;
		const double MaxPhysicsMilliseconds = UPhysicsSettings::Get()->MaxPhysicsDeltaTime * 1000.0;
		const double PhysicsMilliseconds = (MaxPhysicsMilliseconds > 0.0)
			? FMath::Min(FrameMilliseconds, MaxPhysicsMilliseconds)
			: FrameMilliseconds;

		const FString Cut = (FrameMilliseconds < StalledMilliseconds)
			? FString::Printf(TEXT(", shorter than the stall: cut by %s"), *DescribeFrameCuts(World, StalledMilliseconds))
			: FString();

		UE_LOG(LogStarForge, Display,
			TEXT("Hitch: stalled the game thread %.1f ms (asked %.1f ms). The frame that carried it lasted %.1f ms%s%s, ")
			TEXT("and physics advanced %.1f ms of it (MaxPhysicsDeltaTime %.1f ms) -- %s."),
			StalledMilliseconds, AskedMilliseconds, FrameMilliseconds,
			World ? TEXT("") : TEXT(" (the engine's frame: no world to read)"),
			*Cut, PhysicsMilliseconds, MaxPhysicsMilliseconds,
			(PhysicsMilliseconds < FrameMilliseconds)
				? TEXT("the long frame was made: physics integrated less time than the frame lasted")
				: TEXT("no long frame: physics integrated the whole of it, so this run says nothing about one"));
	}

	void HitchNow(const UWorld* World, double Milliseconds)
	{
		const double Before = FPlatformTime::Seconds();
		FPlatformProcess::Sleep(static_cast<float>(Milliseconds / 1000.0));
		const double StalledMilliseconds = (FPlatformTime::Seconds() - Before) * 1000.0;

		// Written on the next engine frame and read off it, because only that frame knows what became
		// of the stall. Predicted from the settings, the line would have to know every way the engine
		// cuts a frame -- the five DescribeFrameCuts reads and at least two it does not -- and one
		// missed would announce a long frame that never happened, in the one instrument that exists
		// to make one. Tested against -benchmark alone, three of the five were. The engine times a frame
		// first thing in FEngineLoop::Tick and counts it last (LaunchEngineLoop.cpp 5701, 6131), so a
		// stall anywhere in frame N is in frame N + 1's delta; the core ticker runs after that frame's
		// world has ticked (6103), and it fires every frame whether or not the world is paused.
		const uint64 StallFrame = GFrameCounter;
		FTSTicker::GetCoreTicker().AddTicker(TEXT("sf.Hitch"), 0.0f,
			[WeakWorld = TWeakObjectPtr<const UWorld>(World), Milliseconds, StalledMilliseconds, StallFrame](float)
			{
				if (GFrameCounter <= StallFrame)
				{
					return true;
				}
				ReportHitch(WeakWorld.Get(), Milliseconds, StalledMilliseconds);
				return false;
			});
	}

	void HitchFromConsole(const TArray<FString>& Args, UWorld* World)
	{
		// ParseDouble and not Atod, for the reason SFPlaceGameMode gives: Atod answers zero for a
		// string with no number in it, and a stall of zero would pass for a stall nobody measured.
		double Milliseconds = 0.0;
		double AfterSeconds = 0.0;
		const bool bParsed = (Args.Num() == 1 || Args.Num() == 2)
			&& FDefaultValueHelper::ParseDouble(Args[0], Milliseconds)
			&& (Args.Num() == 1 || FDefaultValueHelper::ParseDouble(Args[1], AfterSeconds));
		if (!bParsed)
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("sf.Hitch needs a stall in milliseconds and optionally a delay in seconds of game time; nothing stalled."));
			return;
		}

		if (!FMath::IsFinite(Milliseconds) || !(Milliseconds > 0.0) || Milliseconds > MaxHitchMilliseconds)
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("sf.Hitch %s: a stall has to be more than 0 and at most %.0f ms -- past the world's frame clamp a longer ")
				TEXT("one measures nothing more; nothing stalled."),
				*Args[0], MaxHitchMilliseconds);
			return;
		}

		if (!FMath::IsFinite(AfterSeconds) || !(AfterSeconds >= 0.0))
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("sf.Hitch %s: a delay of %s is not a finite non-negative number of seconds; nothing stalled."),
				*Args[0], *Args[1]);
			return;
		}

		if (AfterSeconds == 0.0)
		{
			HitchNow(World, Milliseconds);
			return;
		}

		if (!World)
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("sf.Hitch %s: no world to count %s s of game time in; nothing stalled."), *Args[0], *Args[1]);
			return;
		}

		// A delay in game time for sf.KillThruster's reason: -ExecCmds runs its whole list on the
		// first frame, before anything is moving, and a long frame worth measuring has to land on a
		// hull already under way.
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle,
			FTimerDelegate::CreateLambda([WeakWorld = TWeakObjectPtr<UWorld>(World), Milliseconds]()
			{
				if (const UWorld* LiveWorld = WeakWorld.Get())
				{
					HitchNow(LiveWorld, Milliseconds);
				}
			}),
			static_cast<float>(AfterSeconds), false);

		UE_LOG(LogStarForge, Display,
			TEXT("sf.Hitch %.1f: scheduled %.2f s of game time from now."), Milliseconds, AfterSeconds);
	}

	/**
	 * Stalls the game thread once, so the next frame is long: the first way to measure the second
	 * case of finding F4 -- a frame past MaxPhysicsDeltaTime, in which physics integrates less time
	 * than the frame lasts. USFShipFrameComponent divides a simulated step by the physics step for
	 * that reason, and without a long frame on demand the division is a prediction.
	 *
	 * Only a run WITHOUT -benchmark can use it: -benchmark fixes the step, and the line, which reads
	 * back the frame that carried the stall, says so. A command rather than a variable, for sf.KillThruster's reason: it acts once per
	 * entry, so several entries in one -ExecCmds list are several stalls, each with its own delay.
	 */
	FAutoConsoleCommandWithWorldAndArgs CmdHitch(
		TEXT("sf.Hitch"),
		TEXT("sf.Hitch Milliseconds [Seconds]: stall the game thread once for that long -- now, or after that many seconds of game time --\n")
		TEXT("so the next frame is long. Changes nothing under -benchmark or -UseFixedTimeStep, which fix the step; the line, written on the\n")
		TEXT("next frame, reads that frame back and says what physics made of it. At most 1000 ms."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HitchFromConsole));
}
