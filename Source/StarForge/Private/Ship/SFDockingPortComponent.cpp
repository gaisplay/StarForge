#include "Ship/SFDockingPortComponent.h"

#include "GameFramework/Actor.h"
#include "Ship/SFDockingComponent.h"
#include "Ship/SFDockingStation.h"
#include "StarForgeLog.h"

USFDockingPortComponent::USFDockingPortComponent()
{
	// Read by whoever needs it, on their tick. A socket has nothing to do on its own.
	PrimaryComponentTick.bCanEverTick = false;
}

void USFDockingPortComponent::BeginPlay()
{
	Super::BeginPlay();

	// Once at startup, like the turret mount does, so a run's log says what geometry it
	// was flown with. The axes are printed as vectors rather than as a rotator because
	// the mating condition is stated in axes, and a rotator has to be turned into axes
	// in a reader's head before it can be checked against that condition -- which is
	// exactly where a yaw of -90 and a yaw of +90 stop being distinguishable at a glance.
	const FQuat Relative = GetRelativeRotation().Quaternion();
	const FVector Out = Relative.GetAxisX();
	const FVector Up = Relative.GetAxisZ();
	const FVector Location = GetRelativeLocation();

	// "hull-local" is fixed in the format on purpose, though the station prints through
	// this same line and has no hull. The runbook quotes the line verbatim and the
	// operator matches it by eye; a word that changed with the carrier would break that
	// match for nothing, and the coordinates are the component's own relative location
	// either way.
	UE_LOG(LogStarForge, Display,
		TEXT("Docking port on %s at hull-local (%.0f %.0f %.0f) cm, out along (%.2f %.2f %.2f), deck up (%.2f %.2f %.2f)."),
		GetOwner() ? *GetOwner()->GetActorNameOrLabel() : TEXT("nothing"),
		Location.X, Location.Y, Location.Z,
		Out.X, Out.Y, Out.Z, Up.X, Up.Y, Up.Z);

	// A port nobody can use says so. It belongs to a station, which a hull docks to, or to a ship,
	// whose docking component is what docks; anything else was built with a port and without the
	// declaration that makes it a station, and every search for stations would pass it by in silence.
	const AActor* Owner = GetOwner();
	if (Owner && !Owner->Implements<USFDockingStation>() && !Owner->FindComponentByClass<USFDockingComponent>())
	{
		UE_LOG(LogStarForge, Warning,
			TEXT("Docking port on '%s' belongs to neither a docking station nor a docking ship: no hull can dock to it, and it docks to nothing."),
			*Owner->GetName());
	}
}
