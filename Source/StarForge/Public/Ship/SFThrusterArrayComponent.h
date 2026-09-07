#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FlightController.h"
#include "ThrustAllocator.h"
#include "ThrusterDef.h"
#include "SFThrusterArrayComponent.generated.h"

class UPrimitiveComponent;
class USFShipDataAsset;

/**
 * The ship thruster array: turns a demanded wrench into forces on the body.
 *
 * It owns the allocator and nothing else. It does not know who is flying, what
 * they pressed, or why the demand is what it is -- that separation is what lets an
 * autopilot, an NPC and a player share one execution path.
 *
 * Mass properties are read from Chaos rather than from the data asset. The asset
 * declares the mass; Chaos derives inertia and centre of mass from it and from the
 * body shape, and it is Chaos that actually moves the ship. Computing them
 * separately would let the two disagree, and the ship would plan one turn while
 * performing another.
 */
UCLASS(ClassGroup = (StarForge), meta = (BlueprintSpawnableComponent))
class STARFORGE_API USFThrusterArrayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USFThrusterArrayComponent();

	UPROPERTY(EditAnywhere, Category = "StarForge|Ship")
	TObjectPtr<USFShipDataAsset> ShipData;

	virtual void BeginPlay() override;

	/**
	 * Rebuilds the allocator from the data asset and the body current mass
	 * properties. Call again when a thruster is lost -- the handling degrades on its
	 * own, because the geometry really did change.
	 *
	 * Returns false when the layout cannot span all six axes. The ship then does not
	 * fly, loudly, instead of refusing to turn quietly.
	 */
	bool Rebuild();

	bool IsReady() const { return bReady; }

	/**
	 * Solves the demand across the thrusters, spools them toward that solution, and
	 * applies what they actually managed to the body.
	 *
	 * DeltaSeconds is needed because a thruster does not reach its commanded throttle
	 * instantly; see ResponseTime on FThrusterDef and spec 5.
	 */
	void ApplyDemand(const StarForge::FWrench& DemandShipFrame, double DeltaSeconds);

	/** The most this array can produce on each axis. What assist-off is a fraction of. */
	StarForge::FWrench GetArrayCapability() const { return Capability; }

	StarForge::FShipMassProperties GetMassProperties() const { return MassProperties; }

	/**
	 * Both throttle arrays are exposed, and which one to read depends on the question.
	 *
	 * The target is where the solver's clamp landed this frame, so saturation is a
	 * property of it and of nothing else: Solve clamps into [0,1], and a demand the
	 * array cannot meet shows up here as a throttle pinned at one.
	 *
	 * The spooled array is where the thrusters actually are, having lagged toward that
	 * target over several response times. It is the honest answer to "how much thrust
	 * is on the ship right now", and the wrong answer to "is the array maxed out" --
	 * it only reaches one long after the saturation began, if the demand holds at all.
	 */
	const TArray<double>& GetTargetThrottles() const { return TargetThrottles; }
	const TArray<double>& GetLastThrottles() const { return LastThrottles; }
	StarForge::FWrench GetLastDemand() const { return LastDemand; }
	StarForge::FWrench GetLastAchieved() const { return LastAchieved; }

	/** Ship-frame linear velocity in m/s and angular velocity in rad/s. */
	StarForge::FRigidBodyState ReadState() const;

private:
	/** The simulated body this array pushes. Resolved once at BeginPlay. */
	UPrimitiveComponent* ResolveBody() const;

	StarForge::FThrustAllocator Allocator;
	StarForge::FShipMassProperties MassProperties;
	StarForge::FWrench Capability;

	/** Where the solver wants each throttle. */
	TArray<double> TargetThrottles;

	/** Where each throttle actually is, having spooled toward the target. */
	TArray<double> LastThrottles;

	/** Seconds to spool from zero to full, per thruster, copied from the asset. */
	TArray<double> ResponseTimes;

	StarForge::FWrench LastDemand;
	StarForge::FWrench LastAchieved;

	bool bReady = false;
};
