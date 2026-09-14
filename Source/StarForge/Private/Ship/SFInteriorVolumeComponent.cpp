#include "Ship/SFInteriorVolumeComponent.h"

#include "GameFramework/Actor.h"
#include "Player/SFCharacter.h"
#include "Player/SFShipboardMovementComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "StarForgeLog.h"
#include "Units.h"

namespace
{
namespace InteriorHandover
{
	/**
	 * The fastest two carriers may move relative to each other, where the person stands, for a
	 * volume to hand him from one to the other. Metres per second.
	 *
	 * A binding decides whose motion the carry follows from the next frame, so a handover between
	 * carriers moving apart at v teleports him into a frame moving at v relative to the one he was
	 * standing still in. The first combat run showed it at 106 m/s: a pirate coasting through the
	 * player's cabin claimed the man, who was left falling at 60 fps and carried off on the
	 * pirate's floor at 400. A collision is the same event at 50 m/s.
	 *
	 * The figure is not new. It is the docking window's speed limit,
	 * StarForge::Docking::FDockingLimits::SpeedMetresPerSec: the one number this design already has
	 * for two carriers being at rest with respect to each other, the speed under which a ship may be
	 * mated to a station at all. A docked pair reads zero -- an attached kinematic hull's frame
	 * velocity is the still station's -- so the hatch crossing of stage 3 passes by construction,
	 * and a capture run-in, which closes at up to a few metres per second, is refused, correctly:
	 * the ports are not mated yet.
	 *
	 * Measured at his position rather than between the centres, because a carrier that spins moves
	 * its deck at w x r, and two hulls whose centres are at rest can still slide past each other
	 * where he stands.
	 */
	constexpr double MaxRelativeSpeedMetresPerSec = 1.0;

	FVector PointVelocityMetresPerSec(const USFShipFrameComponent& Frame, const FVector& WorldPointCm)
	{
		const FVector OffsetCm = WorldPointCm - Frame.GetFrameTransform().GetLocation();
		const FVector OffsetMetres(
			StarForge::Units::CmToMetres(OffsetCm.X),
			StarForge::Units::CmToMetres(OffsetCm.Y),
			StarForge::Units::CmToMetres(OffsetCm.Z));
		return Frame.GetLinearVelocityMetersPerSec()
			+ FVector::CrossProduct(Frame.GetAngularVelocityRadPerSec(), OffsetMetres);
	}
}
}

USFInteriorVolumeComponent::USFInteriorVolumeComponent()
{
	// Able to tick, and not ticking: the tick exists only to retry a refused handover while the
	// person stays inside, and is switched on by the refusal and off when nobody is left refused.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Query only, and it answers to pawns alone. A volume that blocked anything would be
	// a second, invisible hull inside the real one; a volume that overlapped everything
	// would fire on every panel, marker and drone that passed through it.
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldStatic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SetGenerateOverlapEvents(true);

	// Not drawn, not hit by traces, and not something the eye or the aim can find.
	SetHiddenInGame(true);
	SetCanEverAffectNavigation(false);
}

USFShipFrameComponent* USFInteriorVolumeComponent::GetOwnerFrame() const
{
	if (!OwnerFrame)
	{
		if (const AActor* Owner = GetOwner())
		{
			OwnerFrame = Owner->FindComponentByClass<USFShipFrameComponent>();
		}
	}

	return OwnerFrame;
}

void USFInteriorVolumeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!GetOwnerFrame())
	{
		// A volume with no frame behind it would silently do nothing: people would walk
		// through it and stay bound to whatever they were bound to before, which is the
		// exact symptom of a handover that never fires.
		UE_LOG(LogStarForge, Error,
			TEXT("Interior volume on [%s] found no frame component; nobody entering it will be bound."),
			GetOwner() ? *GetOwner()->GetActorNameOrLabel() : TEXT("nothing"));
		return;
	}

	OnComponentBeginOverlap.AddDynamic(this, &USFInteriorVolumeComponent::HandleBeginOverlap);
	OnComponentEndOverlap.AddDynamic(this, &USFInteriorVolumeComponent::HandleEndOverlap);
}

double USFInteriorVolumeComponent::RelativeSpeedAtMetresPerSec(
	const USFShipFrameComponent& A, const USFShipFrameComponent& B, const FVector& WorldPointCm)
{
	return (InteriorHandover::PointVelocityMetresPerSec(A, WorldPointCm)
		- InteriorHandover::PointVelocityMetresPerSec(B, WorldPointCm)).Size();
}

void USFInteriorVolumeComponent::TryHandover(ASFCharacter* Character)
{
	USFShipFrameComponent* To = GetOwnerFrame();
	const USFShipboardMovementComponent* Movement = Character ? Character->GetShipboardMovement() : nullptr;
	if (!To || !Movement)
	{
		return;
	}

	const auto IsCharacter = [Character](const TWeakObjectPtr<ASFCharacter>& Entry) { return Entry.Get() == Character; };

	// Nothing to hand him from is his first binding, and the same frame is no change; neither is
	// a handover, and neither is gated. sf.BindFrame does not come through here at all: it is the
	// negative control, and a guard in front of it would stop it proving anything.
	const USFShipFrameComponent* From = Movement->GetShipFrame();
	if (From && From != To)
	{
		const double RelativeSpeed = RelativeSpeedAtMetresPerSec(*From, *To, Character->GetActorLocation());

		// Negated, so a speed that is not a number refuses rather than binds.
		if (!(RelativeSpeed <= InteriorHandover::MaxRelativeSpeedMetresPerSec))
		{
			// Once per episode: from the refused entry until he leaves this volume, or until the two
			// carriers come within the limit and the handover goes through. The next entry is a new
			// episode and is said again. The retry runs every tick in between, silently.
			if (!RefusedOccupants.ContainsByPredicate(IsCharacter))
			{
				RefusedOccupants.Add(Character);
				SetComponentTickEnabled(true);

				const FString FromName = From->GetOwner() ? From->GetOwner()->GetActorNameOrLabel() : FString(TEXT("none"));
				const FString ToName = To->GetOwner() ? To->GetOwner()->GetActorNameOrLabel() : FString(TEXT("none"));
				UE_LOG(LogStarForge, Warning,
					TEXT("Frame handover REFUSED: %s -> %s, the two carriers move %.2f m/s relative to each other where he ")
					TEXT("stands, over the %.1f m/s limit. He stays bound to %s; said once until he leaves %s's volume or the ")
					TEXT("two come within the limit."),
					*FromName, *ToName, RelativeSpeed, InteriorHandover::MaxRelativeSpeedMetresPerSec, *FromName, *ToName);
			}
			return;
		}
	}

	RefusedOccupants.RemoveAll(IsCharacter);
	Character->BindFrame(To);
}

void USFInteriorVolumeComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Backwards, because a retry that goes through removes its own entry.
	for (int32 i = RefusedOccupants.Num() - 1; i >= 0; --i)
	{
		ASFCharacter* Character = RefusedOccupants[i].Get();
		if (!Character || !IsOverlappingActor(Character))
		{
			RefusedOccupants.RemoveAt(i);
			continue;
		}
		TryHandover(Character);
	}

	if (RefusedOccupants.IsEmpty())
	{
		SetComponentTickEnabled(false);
	}
}

bool USFInteriorVolumeComponent::ContainsPoint(const FVector& WorldPoint) const
{
	// Unscaled on both sides: InverseTransformPosition divides the scale out, so the
	// local point is in unscaled units and the unscaled extent is what it belongs beside.
	// Mixing the two is how a box quietly grows by whatever its owner is scaled to.
	const FVector Local = GetComponentTransform().InverseTransformPosition(WorldPoint);
	const FVector Extent = GetUnscaledBoxExtent();

	return FMath::Abs(Local.X) <= Extent.X
		&& FMath::Abs(Local.Y) <= Extent.Y
		&& FMath::Abs(Local.Z) <= Extent.Z;
}

void USFInteriorVolumeComponent::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (ASFCharacter* Character = Cast<ASFCharacter>(OtherActor))
	{
		TryHandover(Character);
	}
}

void USFInteriorVolumeComponent::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	// Leaving still does not unbind (see the class comment). It only closes a refused episode.
	RefusedOccupants.RemoveAll([OtherActor](const TWeakObjectPtr<ASFCharacter>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == OtherActor;
	});
}
