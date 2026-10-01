#include "Ship/SFProjectile.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Ship/SFDamageableComponent.h"
#include "Ship/SFShipFrameComponent.h"
#include "StarForgeLog.h"
#include "Units.h"

namespace
{
	/**
	 * The tunnelling control. It acts on every projectile in the world rather than on one
	 * ship's: it is a question about the mechanism, not a command addressed to a hull.
	 */
	TAutoConsoleVariable<int32> CVarProjectileSweep(TEXT("sf.ProjectileSweep"), 1,
		TEXT("1: a projectile sweeps the whole segment it flew this frame. 0: it tests only the end of the step --\n")
		TEXT("the negative control, which must miss a target thinner than one frame's travel."),
		ECVF_Default);
}

ASFProjectile::ASFProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetMobility(EComponentMobility::Movable);
}

void ASFProjectile::Launch(AActor* InFiredBy, const FVector& MuzzleWorld, const FVector& VelocityWorldCms,
	double InDamage, double InLifeSeconds)
{
	FiredBy = InFiredBy;
	LaunchPosition = MuzzleWorld;

	State.Position = MuzzleWorld;
	State.Velocity = VelocityWorldCms;
	State.AgeSeconds = 0.0;

	Damage = InDamage;
	LifeSeconds = InLifeSeconds;

	SetActorLocation(MuzzleWorld);
}

void ASFProjectile::SetShotRecord(int32 InShotNumber, const FString& InTurretStateAtShot)
{
	ShotNumber = InShotNumber;
	TurretStateAtShot = InTurretStateAtShot;
}

double ASFProjectile::PhysicsStepSeconds(double FrameSeconds)
{
	// The engine's own rule: the settings PhysLevel.cpp 197-212 reads every frame, clamped the way
	// FChaosScene::SetUpForFrame clamps them (ChaosScene.cpp 338-345). At any frame no longer than
	// MaxPhysicsDeltaTime, 1/30 s by default, FMath::Min hands back the frame itself, so at 60 and
	// 400 fps a shot steps exactly as it did when it was stepped by the frame.
	const UPhysicsSettings* Physics = UPhysicsSettings::Get();
	const double MaxSeconds = Physics->bSubstepping
		? Physics->MaxSubsteps * Physics->MaxSubstepDeltaTime
		: Physics->MaxPhysicsDeltaTime;
	return MaxSeconds > 0.0 ? FMath::Min(FrameSeconds, MaxSeconds) : FrameSeconds;
}

void ASFProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// On the physics step, not the frame. Every hull this shot can meet advanced by that and no
	// more, so on a frame longer than MaxPhysicsDeltaTime a shot stepped by the whole frame flew
	// dt - h further than its target did: a hull holding station beside its shooter at 200 m/s
	// moves 200 x (0.100 - 0.033) = 13.3 m off the shot's line in one 100 ms frame, and it is
	// eight metres across. Only a target at rest in the world was immune: a straight line through
	// fixed geometry has one hit point whichever clock walks it.
	StarForge::Ballistics::FProjectileStep Step;
	if (!StarForge::Ballistics::StepProjectile(State, PhysicsStepSeconds(DeltaSeconds), LifeSeconds, Step))
	{
		// The maths refused. It refuses only on figures nothing should have produced, so this
		// is loud: a projectile that quietly stops stepping hangs in space forever, and the
		// only evidence would be an actor count nobody is watching.
		UE_LOG(LogStarForge, Error, TEXT("Projectile: [%s] shot %d could not step and is destroyed."),
			*GetNameSafe(FiredBy.Get()), ShotNumber);
		Destroy();
		return;
	}

	const bool bSweepEnabled = CVarProjectileSweep.GetValueOnGameThread() != 0;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SFProjectile), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(this);
	if (AActor* Shooter = FiredBy.Get()) { Params.AddIgnoredActor(Shooter); }

	// By CHANNEL, not by object type. The ship's panels are ECC_WorldDynamic and the
	// station's are ECC_WorldStatic (SFTestShip.cpp:404, SFTestStation.cpp:199); both call
	// SetCollisionResponseToAllChannels(ECR_Block), so both block every trace channel. A
	// query filtered by object type would see that difference and miss half the world; a
	// query by channel cannot see it at all. The ship's hull box is PhysicsOnly and is
	// invisible to queries by design -- the panel is the skin, and the panel is what
	// classifies the shield face.
	const bool bHit = bSweepEnabled
		? GetWorld()->SweepSingleByChannel(Hit, Step.SegmentStart, Step.SegmentEnd,
			FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(RadiusCm), Params)
		// The negative control, behind sf.ProjectileSweep 0: the same query with no length
		// at all. It must MISS where the swept one hits, and it exists so the swept one can
		// be shown to be doing something.
		: GetWorld()->SweepSingleByChannel(Hit, Step.SegmentEnd, Step.SegmentEnd,
			FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(RadiusCm), Params);

	if (bHit) { ReportAndConsume(Hit); return; }

	State = Step.NextState;
	SetActorLocation(State.Position);

	if (Step.bExpired)
	{
		// The range figure, and it is checked against arithmetic rather than eyeballed:
		// 1000 m/s x 5.0 s = 5000 m, and it must read the same at both frame rates.
		UE_LOG(LogStarForge, Display,
			TEXT("Projectile: [%s] shot %d expired after %.2f s, %.1f m travelled."),
			*GetNameSafe(FiredBy.Get()), ShotNumber, State.AgeSeconds,
			StarForge::Units::CmToMetres((State.Position - LaunchPosition).Size()));
		Destroy();
	}
}

void ASFProjectile::ReportAndConsume(const FHitResult& Hit)
{
	// Reported in the TARGET's own axes, through its frame facade -- the one place in this
	// project that is allowed to know about world coordinates (spec 6). A component doing its
	// own InverseTransformPosition would be a second answer to a question already answered.
	// The wording "hull-local" is the one both carriers already use, and the runbooks quote
	// it literally.
	// A hit on something with no damageable component is still logged and still consumes the
	// projectile: a hit that did nothing and said nothing is an instrument that is silent.
	AActor* Target = Hit.GetActor();
	const UPrimitiveComponent* Component = Hit.GetComponent();
	const USFShipFrameComponent* TargetFrame =
		Target ? Target->FindComponentByClass<USFShipFrameComponent>() : nullptr;

	FString Place;
	if (TargetFrame)
	{
		const FVector Local = TargetFrame->WorldToLocal(FTransform(Hit.ImpactPoint)).GetLocation();
		Place = FString::Printf(TEXT("at hull-local (%.3f %.3f %.3f) cm"), Local.X, Local.Y, Local.Z);
	}
	else
	{
		// Said rather than dressed up: with no frame there are no hull-local axes, and a world
		// point printed under that word would be quoted as one.
		Place = FString::Printf(TEXT("at world (%.3f %.3f %.3f) cm -- no frame, so no hull-local axes"),
			Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z);
	}

	// The component's object type is in the line because the claim that a query by channel
	// sees both kinds of panel is only proved by a hit on each kind, read from the log.
	UE_LOG(LogStarForge, Display,
		TEXT("Projectile: [%s] shot %d hit %s, component %s (%s), %s, %.2f m from the muzzle; ")
		TEXT("fired with the turret %s."),
		*GetNameSafe(FiredBy.Get()), ShotNumber, *GetNameSafe(Target), *GetNameSafe(Component),
		Component ? *UEnum::GetValueAsString(Component->GetCollisionObjectType()) : TEXT("no component"),
		*Place,
		StarForge::Units::CmToMetres((Hit.ImpactPoint - LaunchPosition).Size()),
		*TurretStateAtShot);

	// The world point, not the hull-local one above: turning it into ship axes is the damageable
	// component's job, through the same facade, and handing it a point already converted here
	// would make this line and that one two answers that merely happen to agree.
	if (USFDamageableComponent* Damageable =
		Target ? Target->FindComponentByClass<USFDamageableComponent>() : nullptr)
	{
		Damageable->ReceiveHit(Hit.ImpactPoint, Damage, FiredBy.Get());
	}
	else
	{
		UE_LOG(LogStarForge, Display,
			TEXT("Projectile: [%s] shot %d: %s has no damageable component, so nothing takes its %.1f damage."),
			*GetNameSafe(FiredBy.Get()), ShotNumber, *GetNameSafe(Target), Damage);
	}

	Destroy();
}
