#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Ship/SFStationOperator.h"
#include "SFAutomationOperatorComponent.generated.h"

class ASFTargetDrone;

/**
 * An operator that is not a person: the ship's own automation, seated at the gunner
 * post of the player's own ship.
 *
 * This is where spec 4's claim gets its test. The post it holds was written for a
 * player and has been held by a console; it is now held by this, and neither the post
 * nor the ship changed a line to allow it. Spec 4's amendment of 2026-09-08 puts the
 * automation here rather than in a pirate: the claim under test is about who holds a
 * post, and a second ship would postpone the check for decoration.
 *
 * It is told what to track by the rig. Sensors are a later stage; an automation that
 * went looking for targets on its own would be a sensor post in disguise. When the
 * target is withdrawn it says it has no target, which is a different fact from not
 * being at the post, and the intent carries the difference.
 *
 * It takes the gunner station at BeginPlay and holds it while sf.AutomationOperator
 * stays on, yielding it the moment that variable goes to zero.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFAutomationOperatorComponent : public UActorComponent, public ISFStationOperator
{
	GENERATED_BODY()

public:
	USFAutomationOperatorComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	virtual bool ProvideTurretIntent(StarForge::FTurretIntent& Out) override;
	virtual FString GetOperatorName() const override { return TEXT("automation"); }

	/** What to track. Null, or a withdrawn drone, is reported as no target. */
	void SetTargetDrone(ASFTargetDrone* InDrone);

private:
	TWeakObjectPtr<ASFTargetDrone> TargetDrone;

	/**
	 * Edge detection on the switch. A held post is asked for once per turn of the
	 * switch, so the refusal is heard once; a free post is asked for every frame.
	 */
	bool bWantedSeatLastTick = false;
};
