#pragma once

#include "CoreMinimal.h"
#include "Ship/SFShipStationComponent.h"
#include "TurretMath.h"
#include "SFGunnerStationComponent.generated.h"

class USFTurretComponent;

/**
 * The post that lays the turret.
 *
 * The second post on the ship, and the first that asks for a different kind of
 * intent. It runs the same shape of tick the pilot station does -- ask the operator,
 * turn the answer into a demand, hand it to the executor -- against a turret instead
 * of a thruster array. Spec 4's claim that the ship cannot tell who holds a post is
 * only testable once there are two posts of two types; this is the second.
 *
 * It does not know who the operator is, and it does not know how a bearing is found:
 * the maths is in StarForgeCore and the slewing is in the turret.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFGunnerStationComponent : public USFShipStationComponent
{
	GENERATED_BODY()

public:
	USFGunnerStationComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** What was last asked of the turret, for the readout. Zeroed when nobody is gunning. */
	StarForge::FTurretIntent GetLastIntent() const { return LastIntent; }

	/**
	 * Whether the post actually commanded the turret on the last tick. Occupancy alone
	 * cannot say: an operator that declines leaves the same zeroed intent a gunner
	 * with no target leaves, and only one of those means "the post is working".
	 */
	bool IsOperatorGunning() const { return bOperatorGunning; }

	USFTurretComponent* GetTurret() const { return Turret; }

private:
	UPROPERTY()
	TObjectPtr<USFTurretComponent> Turret;

	StarForge::FTurretIntent LastIntent;

	/** Set only where an intent was actually provided; cleared on every other path. */
	bool bOperatorGunning = false;
};
