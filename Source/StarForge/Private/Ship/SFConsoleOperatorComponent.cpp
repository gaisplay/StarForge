#include "Ship/SFConsoleOperatorComponent.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFPilotStationComponent.h"
#include "StarForgeLog.h"

namespace
{
	// Declared in the file that reads them. They used to live in SFTestShip.cpp, which
	// stopped reading them when the flight loop moved out, and were resolved from here
	// by name -- a lookup that returns zero when it fails. A zeroed axis is a centred
	// stick, and a centred stick with assist on is a command to brake, so a missing
	// variable would have quietly ordered the ship to stop.
	TAutoConsoleVariable<float> CVarIntentX(TEXT("sf.Intent.X"), 0.0f,
		TEXT("Linear intent along the ship forward axis, -1 to 1."), ECVF_Default);
	TAutoConsoleVariable<float> CVarIntentY(TEXT("sf.Intent.Y"), 0.0f,
		TEXT("Linear intent along the ship right axis, -1 to 1."), ECVF_Default);
	TAutoConsoleVariable<float> CVarIntentZ(TEXT("sf.Intent.Z"), 0.0f,
		TEXT("Linear intent along the ship up axis, -1 to 1."), ECVF_Default);

	// Named for what they do, stored in angular velocity order: X roll, Y pitch,
	// Z yaw. The names and the order agree here so nobody has to remember which.
	TAutoConsoleVariable<float> CVarIntentRoll(TEXT("sf.Intent.Roll"), 0.0f,
		TEXT("Roll intent about the ship X axis, -1 to 1."), ECVF_Default);
	TAutoConsoleVariable<float> CVarIntentPitch(TEXT("sf.Intent.Pitch"), 0.0f,
		TEXT("Pitch intent about the ship Y axis, -1 to 1."), ECVF_Default);
	TAutoConsoleVariable<float> CVarIntentYaw(TEXT("sf.Intent.Yaw"), 0.0f,
		TEXT("Yaw intent about the ship Z axis, -1 to 1."), ECVF_Default);

	TAutoConsoleVariable<int32> CVarIntentAssist(TEXT("sf.Intent.Assist"), 1,
		TEXT("1: the stick asks for a speed. 0: it asks for thrust."), ECVF_Default);

	/**
	 * True while the rig is flying its own scripted manoeuvre.
	 *
	 * sf.ShipProfile belongs to the ship, so it is reached by name from here. The
	 * lookup is cached and complains once if it fails, because the answer matters: a
	 * silent false would let this operator fly during a scripted run.
	 */
	bool IsScriptedProfileRunning()
	{
		static IConsoleVariable* Profile =
			IConsoleManager::Get().FindConsoleVariable(TEXT("sf.ShipProfile"));

		if (!Profile)
		{
			static bool bComplained = false;
			if (!bComplained)
			{
				bComplained = true;
				UE_LOG(LogStarForge, Error,
					TEXT("sf.ShipProfile not found; the console operator cannot tell whether the "
						 "rig is flying itself and will assume it is not."));
			}
			return false;
		}

		return Profile->GetInt() != 0;
	}

	/**
	 * Whether the console operator takes the pilot seat at startup.
	 *
	 * On by default, because the headless flight checks depend on somebody flying.
	 * Turned off so a person can take the seat instead -- without it, testing a
	 * player sitting down would mean commenting out code and rebuilding, and a check
	 * that requires editing what it checks is not a check.
	 */
	TAutoConsoleVariable<int32> CVarConsoleOperator(TEXT("sf.ConsoleOperator"), 1,
		TEXT("1: the console operator takes the pilot seat at startup. 0: it leaves the seat free."),
		ECVF_Default);
}

USFConsoleOperatorComponent::USFConsoleOperatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Before physics, and so before the pilot station's own tick, which is where a
	// seat change has to be visible in the same frame it happens rather than the next.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USFConsoleOperatorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (CVarConsoleOperator.GetValueOnGameThread() == 0)
	{
		return;
	}

	if (AActor* Owner = GetOwner())
	{
		if (USFPilotStationComponent* Pilot = Owner->FindComponentByClass<USFPilotStationComponent>())
		{
			Pilot->TryOccupy(this);
		}
	}
}

void USFConsoleOperatorComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	USFPilotStationComponent* Pilot = Owner->FindComponentByClass<USFPilotStationComponent>();
	if (!Pilot)
	{
		return;
	}

	// This is the whole point of ticking: sf.ConsoleOperator has to work as a runtime
	// switch, not just a startup default. -ExecCmds runs after BeginPlay, so reading
	// the cvar once at startup can never be toggled by a person typing at the console.
	const bool bWantsSeat = CVarConsoleOperator.GetValueOnGameThread() != 0;
	const bool bHoldsSeat = Pilot->GetOperator().GetObject() == this;

	if (bWantsSeat && !Pilot->IsOccupied())
	{
		Pilot->TryOccupy(this);
	}
	else if (!bWantsSeat && bHoldsSeat)
	{
		Pilot->Vacate();
	}
}

bool USFConsoleOperatorComponent::ProvideFlightIntent(StarForge::FFlightIntent& Out)
{
	// Not flying while the rig flies itself. Returning a zeroed intent instead would
	// be a command to stop, and the station would apply braking force and torque to a
	// hull whose velocity the scripted profile is setting directly -- so the manoeuvre
	// every recorded drift figure was measured against would no longer be that
	// manoeuvre. Declining is also simply the truth: the console is not flying.
	if (IsScriptedProfileRunning())
	{
		return false;
	}

	Out.LinearAxis = FVector(
		CVarIntentX.GetValueOnGameThread(),
		CVarIntentY.GetValueOnGameThread(),
		CVarIntentZ.GetValueOnGameThread());

	// Angular order is (X roll, Y pitch, Z yaw); the variables are named for what
	// they do and stored in that order, so the two agree without anyone remembering.
	Out.AngularAxis = FVector(
		CVarIntentRoll.GetValueOnGameThread(),
		CVarIntentPitch.GetValueOnGameThread(),
		CVarIntentYaw.GetValueOnGameThread());

	Out.bFlightAssistEnabled = CVarIntentAssist.GetValueOnGameThread() != 0;

	return true;
}
