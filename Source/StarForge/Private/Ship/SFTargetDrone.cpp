#include "Ship/SFTargetDrone.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "StarForgeLog.h"
#include "Units.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/**
	 * Declared in the file that reads it, like every other sf.* variable: a lookup by
	 * name from elsewhere returns zero when it fails, and a zero here would be a
	 * withdrawn drone nobody asked to withdraw.
	 */
	TAutoConsoleVariable<int32> CVarTargetDrone(TEXT("sf.TargetDrone"), 1,
		TEXT("1: the target drone flies its orbit. 0: it is withdrawn, and the turret has to say it has no target."),
		ECVF_Default);

	/** Two metres across: the engine sphere is 100 cm, so scale is diameter in metres. */
	constexpr double DroneScale = 2.0;
}

ASFTargetDrone::ASFTargetDrone()
{
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetMobility(EComponentMobility::Movable);
	Body->SetRelativeScale3D(FVector(DroneScale));

	// A moving point with a mesh on it. It must not be able to stop, push or catch
	// anything: the ship flies through its orbit, and the occupant's sweeps must never
	// meet it.
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetSimulatePhysics(false);
	Body->SetEnableGravity(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Body->SetStaticMesh(SphereMesh.Object);
	}
}

void ASFTargetDrone::SetAnchor(AActor* InAnchor)
{
	Anchor = InAnchor;
}

void ASFTargetDrone::BeginPlay()
{
	Super::BeginPlay();

	SpawnTimeSeconds = GetWorld()->GetTimeSeconds();
	bWasPresent = IsPresent();

	UE_LOG(LogStarForge, Display,
		TEXT("Target drone spawned: orbit %.1f m radius at %.1f deg/s, tilted %.1f deg, anchored to %s, %s."),
		OrbitRadiusMetres, OrbitRateDegPerSec, OrbitTiltDegrees,
		Anchor.IsValid() ? *Anchor->GetName() : TEXT("the world origin (no anchor)"),
		bWasPresent ? TEXT("present") : TEXT("WITHDRAWN (sf.TargetDrone 0)"));

	SetActorLocation(EvaluatePathNow());
	SetActorHiddenInGame(!bWasPresent);
}

bool ASFTargetDrone::IsPresent() const
{
	return CVarTargetDrone.GetValueOnGameThread() != 0;
}

FVector ASFTargetDrone::EvaluatePathNow() const
{
	const double Elapsed = GetWorld()->GetTimeSeconds() - SpawnTimeSeconds;
	const double Phase = FMath::DegreesToRadians(OrbitRateDegPerSec * Elapsed);
	const double Tilt = FMath::DegreesToRadians(OrbitTiltDegrees);

	// A circle in a plane tilted about world X: the Y extent is foreshortened by the
	// tilt and the Z extent is what it gives up. Metres until the conversion below.
	const FVector OffsetMetres(
		OrbitRadiusMetres * FMath::Cos(Phase),
		OrbitRadiusMetres * FMath::Sin(Phase) * FMath::Cos(Tilt),
		OrbitRadiusMetres * FMath::Sin(Phase) * FMath::Sin(Tilt));

	const FVector OffsetCm(
		StarForge::Units::MetresToCm(OffsetMetres.X),
		StarForge::Units::MetresToCm(OffsetMetres.Y),
		StarForge::Units::MetresToCm(OffsetMetres.Z));

	const FVector Centre = Anchor.IsValid() ? Anchor->GetActorLocation() : FVector::ZeroVector;
	return Centre + OffsetCm;
}

void ASFTargetDrone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SetActorLocation(EvaluatePathNow());

	const bool bPresent = IsPresent();
	if (bPresent != bWasPresent)
	{
		// Said once per change. The turret's readout says it every second for as long
		// as it lasts; this line is for finding the moment in the log.
		UE_LOG(LogStarForge, Display, TEXT("Target drone %s."),
			bPresent ? TEXT("present again (sf.TargetDrone 1)") : TEXT("WITHDRAWN (sf.TargetDrone 0)"));
		SetActorHiddenInGame(!bPresent);
		bWasPresent = bPresent;
	}
}
