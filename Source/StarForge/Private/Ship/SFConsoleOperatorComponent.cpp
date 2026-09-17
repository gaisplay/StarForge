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

	// Before physics. Being in the same group as the pilot station orders nothing
	// between the two, so BeginPlay adds the station's tick prerequisite explicitly --
	// that, not this line, is what puts this tick first, which is where a seat change
	// has to be visible in the same frame it happens rather than the next.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USFConsoleOperatorComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	USFPilotStationComponent* Pilot =
		Owner ? Owner->FindComponentByClass<USFPilotStationComponent>() : nullptr;

	if (!Pilot)
	{
		return;
	}

	// Makes the ordering the constructor's comment claims actually true. TG_PrePhysics
	// orders neither component against the other, and the station is constructed first,
	// so without this the real order is the reverse of what is wanted: a seat change
	// made here would be seen by the station a frame late, and for that frame the post
	// would still be flying on behalf of an operator that had just given it up.
	Pilot->AddTickPrerequisiteComponent(this);

	if (!bConsoleControlled)
	{
		UE_LOG(LogStarForge, Display,
			TEXT("Console operator on [%s] stands down: this hull does not answer sf.*."),
			*GetOwner()->GetName());
		return;
	}

	if (CVarConsoleOperator.GetValueOnGameThread() == 0)
	{
		return;
	}

	Pilot->TryOccupy(this);
}

void USFConsoleOperatorComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bConsoleControlled)
	{
		return;
	}

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
	UObject* Holder = Pilot->GetOperator().GetObject();
	const bool bHoldsSeat = Holder == this;

	if (bWantsSeat && !bHoldsSeat)
	{
		if (!Holder)
		{
			// A free post is asked for every frame until it is held, and never through
			// the limiter. TryOccupy against an empty station succeeds and logs no
			// refusal, so there is nothing here to rate-limit; the limiter exists only
			// for the standing condition of being refused by somebody.
			//
			// Routing this case through it was a way to lose the seat for the rest of
			// the session. The limiter remembers the holder weakly, so a destroyed one
			// reads back as null -- which is also what a free seat reads as. The two
			// compared equal, the ask was never repeated, and the empty post stayed
			// empty in silence.
			bAskedForSeat = false;
			LastSeatHolder = nullptr;
			Pilot->TryOccupy(this);
		}
		// Asked for even when the post is visibly taken, so the refusal in TryOccupy is
		// heard. Pre-checking IsOccupied() here meant an operator turned on against a
		// seated player waited in perfect silence, which from the log is exactly what a
		// component that never ticked looks like.
		//
		// Once per change of holder, though, not once per frame: the refusal is a
		// standing condition, and a standing condition logged at sixty hertz buries
		// every other line in the file. A new holder is new news and is said again.
		else if (!bAskedForSeat || LastSeatHolder.Get() != Holder)
		{
			bAskedForSeat = true;
			LastSeatHolder = Holder;
			Pilot->TryOccupy(this);
		}
	}
	else
	{
		// Either we hold it or we no longer want it; the next refusal is fresh news.
		bAskedForSeat = false;
		LastSeatHolder = nullptr;

		if (!bWantsSeat && bHoldsSeat)
		{
			Pilot->Vacate();
		}
	}
}

bool USFConsoleOperatorComponent::ProvideFlightIntent(StarForge::FFlightIntent& Out)
{
	// The "not while the rig flies itself" test used to be here and is now on the pilot
	// station, where it binds every operator instead of this one. It is also the only
	// thing this component ever knew about the rig's console variables, and the next
	// plan reuses this component: an operator that flies from cvars has no business
	// knowing that a throwaway measurement rig exists.
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
