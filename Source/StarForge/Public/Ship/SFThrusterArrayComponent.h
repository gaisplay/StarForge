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
	 * properties, over the thrusters that have not been destroyed. The handling
	 * degrades on its own, because the geometry really did change.
	 *
	 * The survivors keep their throttles, spooled and commanded, carried across by
	 * asset index: a rebuild in flight must not snap the ship's thrust to zero.
	 *
	 * Returns false when the layout cannot span all six axes. The ship then does not
	 * fly, loudly, instead of refusing to turn quietly, and every readout below is
	 * left empty rather than describing the array that was there before.
	 */
	bool Rebuild();

	bool IsReady() const { return bReady; }

	/**
	 * Takes one thruster out of the array and rebuilds over what is left.
	 *
	 * Returns what Rebuild returned, and the caller must look: an array that can no longer
	 * span six axes is a ship that does not fly, and spec 5 wants that said out loud rather
	 * than discovered as a refusal to turn.
	 *
	 * There is no repair. Nothing in the slice puts a thruster back, and a mechanism with no
	 * caller is a mechanism nobody has tested.
	 *
	 * AssetIndex is the thruster's index in the ship data asset, not a column of the solver.
	 * The destroyed set is kept by those indices and outlives every Rebuild, including the one
	 * undocking makes: each rebuild starts from the whole asset and takes the set away again.
	 * The loss takes that thruster's thrust and nobody else's: the survivors go on from where
	 * their throttles stood (see Rebuild).
	 * An index that is not a surviving thruster changes nothing, rebuilds nothing, and returns
	 * IsReady() -- the array is exactly as able to fly as it was. This array does not know why a
	 * thruster is gone and does not say; the caller writes the line, and DescribeLossSince gives
	 * it the figures.
	 */
	bool DestroyThruster(int32 AssetIndex);

	/** How many thrusters the ship was built with: the asset's count, destroyed ones included. */
	int32 GetThrusterCount() const;

	/** How many are not destroyed, whether or not they still build. */
	int32 GetLiveThrusterCount() const { return LiveAssetIndices.Num(); }

	/**
	 * The asset indices of the surviving thrusters, ascending. While the array is ready, entry i
	 * is the thruster behind the solver's column i -- and so behind entry i of both throttle arrays
	 * below, which are indexed by column. A log line and a hit speak asset indices; without this
	 * map one silently stands in for the other the moment the first thruster is lost.
	 */
	const TArray<int32>& GetLiveAssetIndices() const { return LiveAssetIndices; }

	/**
	 * The consequence of a loss, for the caller's line: "23 of 24 left; fore-and-aft thrust
	 * 800.0 -> 600.0 kN, 16.0 -> 12.0 m/s^2." ForwardNewtonsBefore is GetArrayCapability().Force.X
	 * read before DestroyThruster. On an array that no longer builds it says that instead.
	 */
	FString DescribeLossSince(double ForwardNewtonsBefore) const;

	/**
	 * Whether this instance answers the unnamed sf.* family -- here sf.KillThruster. See
	 * ESFShipCrewing.
	 */
	void SetConsoleControlled(bool bIn) { bConsoleControlled = bIn; }

	bool IsConsoleControlled() const { return bConsoleControlled; }

	/**
	 * Solves the demand across the thrusters, spools them toward that solution, and
	 * applies what they actually managed to the body.
	 *
	 * DeltaSeconds is needed because a thruster does not reach its commanded throttle
	 * instantly; see ResponseTime on FThrusterDef and spec 5.
	 */
	void ApplyDemand(const StarForge::FWrench& DemandShipFrame, double DeltaSeconds);

	/**
	 * Forgets the last demand, so the readout stops reporting one.
	 *
	 * Called by a station that is not flying. Without it the last wrench anybody asked
	 * for stays in these accessors for as long as nobody asks for another, and the
	 * readout prints a live-looking twenty meganewtons beside the word "unoccupied".
	 * A stale reading and a real one look identical, which is the failure this project
	 * keeps catching.
	 */
	void ClearDemand();

	/**
	 * Locks the array: ApplyDemand becomes a no-op that also clears the readout.
	 *
	 * A docked hull does not simulate, so a wrench applied to it goes nowhere. Without
	 * the lock the readout would print thrust the ship never felt, which is the stale
	 * reading this class already has one function to remove.
	 *
	 * The array refuses, not the post and not the operator: the post asks whether its
	 * engines are locked, and neither it nor whoever holds it has to know that docking
	 * exists. Same shape as the scripted-profile lock one level up.
	 */
	void SetLocked(bool bInLocked);

	bool IsLocked() const { return bLocked; }

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

	/** Seconds to spool from zero to full, per surviving thruster by column, copied from the asset. */
	TArray<double> ResponseTimes;

	/** Asset indices of the thrusters destroyed so far. Never cleared: see DestroyThruster. */
	TSet<int32> DestroyedAssetIndices;

	/** See GetLiveAssetIndices. Rewritten by every Rebuild that finds the asset. */
	TArray<int32> LiveAssetIndices;

	/** See SetConsoleControlled. */
	bool bConsoleControlled = true;

	StarForge::FWrench LastDemand;
	StarForge::FWrench LastAchieved;

	bool bReady = false;

	/** True while something has locked the array. See SetLocked. */
	bool bLocked = false;
};
