#pragma once

#include "CoreMinimal.h"
#include "Ballistics.h"
#include "GameFramework/Actor.h"
#include "SFProjectile.generated.h"

class USceneComponent;
struct FHitResult;

/**
 * A shot in flight. It owns its own motion, and that is the point.
 *
 * UProjectileMovementComponent is deliberately not used. Spec 6 is an entire section about
 * what happens when the engine decides how something moves and the project finds out later:
 * the stock carry sweeps an occupant six metres through his own hull at 400 m/s. The answer
 * there was to take the motion back, and this takes it back before it is given away.
 *
 * Each frame it integrates one step and sweeps the segment from where it was to where it now
 * is. Not a point test at the new position: at 1000 m/s and 60 fps the step is 16.67 m, twice
 * the length of an eight-metre hull, and the panels are 20 cm thick. A sphere of RadiusCm
 * catches a panel over 20 + 2 x 5 = 30 cm of the line, so a point test would land inside one
 * about 3.6% of the time (2 x 0.30 / 16.67) and pass clean through the ship the rest. At
 * 400 fps the step is 2.5 m and the same test would hit about 24% of the time (2 x 0.30 / 2.5).
 * A hit rate that depends on the frame rate is the same defect the frame carry has, seen from
 * the other side.
 *
 * It ticks in TG_PostPhysics. The panels it sweeps against hang off a hull Chaos moves, and
 * their world transforms are only current after physics has run; sweeping before it would test
 * last frame's pose, which at 200 m/s is 3.3 m of aiming error at 60 fps and 0.5 m at 400.
 *
 * It ignores the actor that fired it, permanently. The turret hangs INSIDE the cabin, under
 * the ceiling, because the cabin has no windows and a barrel on the roof could only ever be
 * checked from the log; so every shot begins inside its own hull. That is a consequence of a
 * decision made in stage 2 and it is written down rather than compensated.
 */
UCLASS()
class STARFORGE_API ASFProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASFProjectile();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Sets the shot going. Everything is in world centimetres and seconds; the shot owns
	 * nothing of the carrier it left except the velocity already folded into VelocityWorldCms.
	 */
	void Launch(AActor* InFiredBy, const FVector& MuzzleWorld, const FVector& VelocityWorldCms,
		double InDamage, double InLifeSeconds);

	/**
	 * What the log needs to tie a hit back to its shot: the weapon's count and the turret's
	 * state when the trigger let this one go. Read by nothing but the log lines.
	 */
	void SetShotRecord(int32 InShotNumber, const FString& InTurretStateAtShot);

	/** The sphere swept along each step, in centimetres. */
	static constexpr double RadiusCm = 5.0;

private:
	void ReportAndConsume(const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, Category = "StarForge")
	TObjectPtr<USceneComponent> Root;

	TWeakObjectPtr<AActor> FiredBy;

	StarForge::Ballistics::FProjectileState State;
	FVector LaunchPosition = FVector::ZeroVector;

	/** Zero until Launch, which StepProjectile refuses: a shot nobody launched is destroyed loudly. */
	double LifeSeconds = 0.0;

	/** What a hit hands to the target's damageable component, if it has one. */
	double Damage = 0.0;

	int32 ShotNumber = 0;
	FString TurretStateAtShot;
};
