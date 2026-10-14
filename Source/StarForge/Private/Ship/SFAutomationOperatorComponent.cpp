#include "Ship/SFAutomationOperatorComponent.h"

#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFGunnerStationComponent.h"
#include "Ship/SFTargetDrone.h"
#include "Ship/SFTestShip.h"
#include "Ship/SFDockingStation.h"
#include "StarForgeLog.h"

namespace
{
	/**
	 * Whether the automation takes the gunner seat at startup and keeps it. On by
	 * default: a mechanic behind an off-by-default switch is a mechanic nobody has.
	 * Turned off so a person can take the gunner seat instead, the way sf.ConsoleOperator
	 * frees the pilot's.
	 */
	TAutoConsoleVariable<int32> CVarAutomationOperator(TEXT("sf.AutomationOperator"), 1,
		TEXT("1: the ship's automation takes the gunner seat at startup and holds it. 0: it leaves the seat free."),
		ECVF_Default);

	/**
	 * Points the gunner's automation at something other than what the rig gave it. Empty is
	 * the default and changes nothing, so both existing rigs keep tracking their drone. It is
	 * what gives the stage's main instrument a target that does not move and has panels to
	 * stop a shot: the station.
	 *
	 * Acted on when the value changes, as sf.BindFrame is, and ignored by an automated hull. Cleared
	 * when the world ends (see EndPlay), so it never carries into the next one.
	 */
	TAutoConsoleVariable<FString> CVarAimAt(TEXT("sf.AimAt"), TEXT(""),
		TEXT("station: the gunner's automation tracks the station. ship: another ship, never its own.\n")
		TEXT("Empty: whatever the rig gave it -- the drone on both measurement rigs. Ignored by an automated hull.\n")
		TEXT("Cleared when the world ends."),
		ECVF_Default);
}

USFAutomationOperatorComponent::USFAutomationOperatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Before physics. Being in the same group as the gunner station orders nothing
	// between the two, so BeginPlay adds the station's tick prerequisite explicitly --
	// that, not this line, is what puts this tick first, which is where a seat change
	// has to be visible in the same frame it happens rather than the next.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USFAutomationOperatorComponent::SetTargetActor(AActor* InTarget)
{
	TargetActor = InTarget;
}

void USFAutomationOperatorComponent::PollAimAtCommand()
{
	const FString Command = CVarAimAt.GetValueOnGameThread();
	if (Command == LastAimAtCommand)
	{
		return;
	}
	LastAimAtCommand = Command;

	const AActor* Owner = GetOwner();
	ConsoleTarget = nullptr;

	if (Command.IsEmpty())
	{
		UE_LOG(LogStarForge, Display,
			TEXT("Automation: [%s] sf.AimAt cleared; tracking %s again, as the rig set it."),
			*GetNameSafe(Owner), TargetActor.IsValid() ? *TargetActor->GetName() : TEXT("nothing"));
		return;
	}

	const TCHAR* SoughtClass = nullptr;
	if (Command.Equals(TEXT("station"), ESearchCase::IgnoreCase))
	{
		// Any docking station, the first by name. The miss says "docking station" now, not
		// ASFTestStation: no evidence log carries the old text (station-kit plan, task 5).
		SoughtClass = TEXT("docking station");
		const TArray<AActor*> Stations = ISFDockingStation::FindAll(GetWorld(), Owner);
		if (Stations.Num() > 0)
		{
			ConsoleTarget = Stations[0];
		}
	}
	else if (Command.Equals(TEXT("ship"), ESearchCase::IgnoreCase))
	{
		// Another ship. Its own hull is never a target: every shot ignores the actor that
		// fired it, so the gunner would aim at the middle of his own cabin and hit nothing.
		SoughtClass = TEXT("ASFTestShip other than this one");
		for (TActorIterator<ASFTestShip> It(GetWorld()); It; ++It)
		{
			if (*It != Owner)
			{
				ConsoleTarget = *It;
				break;
			}
		}
	}
	else
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Automation: [%s] sf.AimAt '%s' is not station, ship or empty; the gunner has no target."),
			*GetNameSafe(Owner), *Command);
		return;
	}

	if (!ConsoleTarget.IsValid())
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Automation: [%s] sf.AimAt '%s': no %s in this level; the gunner has no target."),
			*GetNameSafe(Owner), *Command, SoughtClass);
		return;
	}

	UE_LOG(LogStarForge, Display,
		TEXT("Automation: [%s] sf.AimAt '%s': tracking %s."),
		*GetNameSafe(Owner), *Command, *ConsoleTarget->GetName());
}

void USFAutomationOperatorComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	USFGunnerStationComponent* Gunner =
		Owner ? Owner->FindComponentByClass<USFGunnerStationComponent>() : nullptr;

	if (!Gunner)
	{
		return;
	}

	// Makes the ordering the constructor's comment claims actually true, for the same
	// reason the console operator does it to the pilot station.
	Gunner->AddTickPrerequisiteComponent(this);

	// A hull that does not answer sf.* takes the seat without asking the variable: its
	// posts are held by automation by definition, which is what makes it automated.
	bWantedSeatLastTick = !bConsoleControlled || CVarAutomationOperator.GetValueOnGameThread() != 0;
	if (bWantedSeatLastTick)
	{
		Gunner->TryOccupy(this);
	}
}

void USFAutomationOperatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// sf.AimAt is acted on when it differs from LastAimAtCommand, which every new component starts
	// empty, and the variable outlives the world: left at "station" by one Play-in-Editor session,
	// RestartLevel or open, it would retarget the next world's gunner on its first tick with nobody
	// typing anything. So the world takes its value with it. At console priority, because a value
	// typed at the console holds that priority and one set beneath it is refused; and only when the
	// world itself ends, not when one hull leaves it. A value typed after this, between sessions, is
	// a change against the next component's empty start and acts on its first tick, as it should.
	if (bConsoleControlled
		&& (EndPlayReason == EEndPlayReason::EndPlayInEditor
			|| EndPlayReason == EEndPlayReason::LevelTransition
			|| EndPlayReason == EEndPlayReason::Quit))
	{
		CVarAimAt->Set(TEXT(""), ECVF_SetByConsole);
	}

	Super::EndPlay(EndPlayReason);
}

void USFAutomationOperatorComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Before the post ticks -- this component is its prerequisite -- so a change of target
	// is aimed at in the same frame it is typed.
	if (bConsoleControlled)
	{
		PollAimAtCommand();
	}

	AActor* Owner = GetOwner();
	USFGunnerStationComponent* Gunner =
		Owner ? Owner->FindComponentByClass<USFGunnerStationComponent>() : nullptr;
	if (!Gunner)
	{
		return;
	}

	// A runtime switch, not a startup default: -ExecCmds runs after BeginPlay, and a
	// person at the console has to be able to turn this off to take the seat.
	const bool bWantsSeat = !bConsoleControlled || CVarAutomationOperator.GetValueOnGameThread() != 0;
	const UObject* Holder = Gunner->GetOperator().GetObject();
	const bool bHoldsSeat = Holder == this;

	if (bWantsSeat && !bHoldsSeat)
	{
		// A free post is asked for every frame until it is held -- the console
		// operator's lesson, learned when a rate limiter sat out an empty seat for the
		// rest of a session. A post somebody else holds is asked for once per turn of
		// the switch, so the refusal in TryOccupy is heard once rather than sixty times
		// a second, and heard at all rather than pre-empted by an IsOccupied check.
		if (!Holder || !bWantedSeatLastTick)
		{
			Gunner->TryOccupy(this);
		}
	}
	else if (!bWantsSeat && bHoldsSeat)
	{
		Gunner->Vacate();
	}

	bWantedSeatLastTick = bWantsSeat;
}

bool USFAutomationOperatorComponent::ProvideTurretIntent(StarForge::FTurretIntent& Out)
{
	// Gunning whenever asked: this operator has no "not at the post" state of its own.
	// What it may lack is a target, and that is said in the intent rather than by
	// declining, because declining reads as an empty seat and an empty seat is a
	// different fact.
	const AActor* Target = LastAimAtCommand.IsEmpty() ? TargetActor.Get() : ConsoleTarget.Get();
	if (const ASFTargetDrone* Drone = Cast<const ASFTargetDrone>(Target))
	{
		Out.bHasTarget = Drone->IsPresent();
		Out.TargetWorldLocation = Out.bHasTarget ? Drone->EvaluatePathNow() : FVector::ZeroVector;
	}
	else
	{
		Out.bHasTarget = Target != nullptr;
		// The actor's location: on ASFTestShip that is the hull box's pivot, which is the
		// centre of the cube and the centre of mass. Aiming there aims at the middle of the
		// target, and which face the shot lands on then follows from where the shooter is --
		// which is what makes "get above him and hit the roof" a manoeuvre rather than a
		// setting.
		Out.TargetWorldLocation = Out.bHasTarget ? Target->GetActorLocation() : FVector::ZeroVector;
	}

	// Trigger held whenever there is something to point at. Whether the barrel is on
	// it is the turret's report, not this operator's decision, and it stays that way now
	// that the barrel has a muzzle: shots that leave while it is still slewing are how a
	// mount that never arrives shows itself.
	Out.bFiring = Out.bHasTarget;

	return true;
}
