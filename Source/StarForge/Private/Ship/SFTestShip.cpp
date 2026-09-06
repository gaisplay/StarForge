#include "Ship/SFTestShip.h"

#include "Components/StaticMeshComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** One engine cube is 100 cm on a side, so scale equals metres. */
	constexpr double CubeEdgeCm = 100.0;
	constexpr double PanelThickness = 0.2;
}

ASFTestShip::ASFTestShip()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Frame = CreateDefaultSubobject<USFShipFrameComponent>(TEXT("Frame"));

	const double Span = InteriorHalfExtentCm / CubeEdgeCm * 2.0;
	const double Half = InteriorHalfExtentCm;

	AddBoxPanel(TEXT("Floor"), FVector(0.0, 0.0, -Half), FVector(Span, Span, PanelThickness));
	AddBoxPanel(TEXT("Ceiling"), FVector(0.0, 0.0, Half), FVector(Span, Span, PanelThickness));
	AddBoxPanel(TEXT("WallXPos"), FVector(Half, 0.0, 0.0), FVector(PanelThickness, Span, Span));
	AddBoxPanel(TEXT("WallXNeg"), FVector(-Half, 0.0, 0.0), FVector(PanelThickness, Span, Span));
	AddBoxPanel(TEXT("WallYPos"), FVector(0.0, Half, 0.0), FVector(Span, PanelThickness, Span));
	AddBoxPanel(TEXT("WallYNeg"), FVector(0.0, -Half, 0.0), FVector(Span, PanelThickness, Span));
}

UStaticMeshComponent* ASFTestShip::AddBoxPanel(FName Name, const FVector& RelativeLocation, const FVector& RelativeScale)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	UStaticMeshComponent* Panel = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Panel->SetupAttachment(Root);
	Panel->SetRelativeLocation(RelativeLocation);
	Panel->SetRelativeScale3D(RelativeScale);
	Panel->SetMobility(EComponentMobility::Movable);
	Panel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Panel->SetCollisionObjectType(ECC_WorldDynamic);
	Panel->SetCollisionResponseToAllChannels(ECR_Block);

	if (CubeMesh.Succeeded())
	{
		Panel->SetStaticMesh(CubeMesh.Object);
	}

	Panels.Add(Panel);
	return Panel;
}

FVector ASFTestShip::GetInteriorSpawnWorldLocation() const
{
	// Just above the floor plate, on the pivot.
	const FVector Local(0.0, 0.0, -InteriorHalfExtentCm + 100.0);
	return GetActorTransform().TransformPosition(Local);
}

void ASFTestShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (DeltaSeconds <= UE_SMALL_NUMBER)
	{
		return;
	}

	ElapsedSeconds += DeltaSeconds;

	const double Phase = 2.0 * UE_DOUBLE_PI * ElapsedSeconds / ProfilePeriodSeconds;

	// A full-amplitude sinusoid along X: speed peaks at the design ceiling and
	// acceleration reverses hardest at the zero crossings, which is where a
	// passenger is most likely to be thrown or left behind.
	const FVector VelocityCms(PeakSpeedCms * FMath::Sin(Phase), 0.0, 0.0);
	AddActorWorldOffset(VelocityCms * DeltaSeconds, /*bSweep=*/false);

	LinearAccelMetersPerSecSq = (VelocityCms - PreviousVelocityCms) / DeltaSeconds / 100.0;
	PreviousVelocityCms = VelocityCms;

	// All three angular axes loaded at once, out of phase, so no accidental
	// symmetry hides a bug.
	const FRotator RatePerSecond(
		PeakPitchYawRateDegPerSec * FMath::Sin(Phase * 0.5),
		PeakPitchYawRateDegPerSec * FMath::Cos(Phase * 0.5),
		PeakRollRateDegPerSec);

	AddActorLocalRotation((RatePerSecond * DeltaSeconds).Quaternion());
}
