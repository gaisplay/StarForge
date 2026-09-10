#include "Ship/SFDockingPortComponent.h"

#include "GameFramework/Actor.h"
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

	UE_LOG(LogStarForge, Display,
		TEXT("Docking port on %s at hull-local (%.0f %.0f %.0f) cm, out along (%.2f %.2f %.2f), deck up (%.2f %.2f %.2f)."),
		GetOwner() ? *GetOwner()->GetActorNameOrLabel() : TEXT("nothing"),
		Location.X, Location.Y, Location.Z,
		Out.X, Out.Y, Out.Z, Up.X, Up.Y, Up.Z);
}
