#include "Ship/SFInteriorVolumeComponent.h"

#include "GameFramework/Actor.h"
#include "Player/SFCharacter.h"
#include "Ship/SFShipFrameComponent.h"
#include "StarForgeLog.h"

USFInteriorVolumeComponent::USFInteriorVolumeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

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
		Character->BindFrame(GetOwnerFrame());
	}
}
