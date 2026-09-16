#include "Ship/SFThrusterArrayComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Ship/SFShipDataAsset.h"
#include "StarForgeLog.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "Units.h"

namespace
{
	void KillThrusterNow(UWorld* World, int32 AssetIndex)
	{
		int32 Answered = 0;
		for (TObjectIterator<USFThrusterArrayComponent> It; It; ++It)
		{
			USFThrusterArrayComponent* Array = *It;
			if (!World || Array->GetWorld() != World || !Array->HasBegunPlay() || !Array->IsConsoleControlled())
			{
				continue;
			}
			++Answered;

			const FString ShipName = GetNameSafe(Array->GetOwner());

			// Both refusals are said, because a debug command that did nothing looks exactly like one
			// that did something nobody checked -- and twelve identical commands must read as one
			// destruction and eleven refusals, not as twelve quiet lines of nothing.
			if (!Array->ShipData)
			{
				UE_LOG(LogStarForge, Warning,
					TEXT("sf.KillThruster %d: the thruster array on [%s] has no ship data asset, so it has no thrusters to destroy."),
					AssetIndex, *ShipName);
				continue;
			}

			if (!Array->ShipData->Thrusters.IsValidIndex(AssetIndex))
			{
				UE_LOG(LogStarForge, Warning,
					TEXT("sf.KillThruster %d: [%s] has no thruster with that index (it has %d); nothing destroyed."),
					AssetIndex, *ShipName, Array->GetThrusterCount());
				continue;
			}

			if (!Array->GetLiveAssetIndices().Contains(AssetIndex))
			{
				UE_LOG(LogStarForge, Warning,
					TEXT("sf.KillThruster %d: thruster %d on [%s] is already destroyed; nothing changed."),
					AssetIndex, AssetIndex, *ShipName);
				continue;
			}

			const double ForwardBefore = Array->GetArrayCapability().Force.X;
			const FVector At = Array->ShipData->Thrusters[AssetIndex].LocationMetres;
			Array->DestroyThruster(AssetIndex);

			UE_LOG(LogStarForge, Display,
				TEXT("Thruster %d on [%s] destroyed at (%.1f %.1f %.1f) m by sf.KillThruster. %s"),
				AssetIndex, *ShipName, At.X, At.Y, At.Z, *Array->DescribeLossSince(ForwardBefore));
		}

		if (Answered == 0)
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("sf.KillThruster %d: no thruster array in this world answers the console; nothing destroyed."), AssetIndex);
		}
	}

	void KillThrusterFromConsole(const TArray<FString>& Args, UWorld* World)
	{
		const auto IsWholeNumber = [](const FString& Arg) { return Arg.IsNumeric() && !Arg.Contains(TEXT(".")); };

		const bool bParsed = (Args.Num() == 1 || Args.Num() == 2) && IsWholeNumber(Args[0])
			&& (Args.Num() == 1 || Args[1].IsNumeric());
		if (!bParsed)
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("sf.KillThruster needs a thruster index, a whole number, and optionally a delay in seconds; nothing destroyed."));
			return;
		}
		const int32 AssetIndex = FCString::Atoi(*Args[0]);
		const double AfterSeconds = (Args.Num() == 2) ? FCString::Atod(*Args[1]) : 0.0;

		if (!(AfterSeconds >= 0.0) || !FMath::IsFinite(AfterSeconds))
		{
			UE_LOG(LogStarForge, Warning,
				TEXT("sf.KillThruster %d: a delay of %s is not a finite non-negative number of seconds; nothing destroyed."),
				AssetIndex, *Args[1]);
			return;
		}

		if (AfterSeconds == 0.0 || !World)
		{
			KillThrusterNow(World, AssetIndex);
			return;
		}

		// A delay in seconds of game time, not in frames, and it exists for one measurement: -ExecCmds
		// runs its whole list on the first frame, before any thruster has spooled, so a loss that has
		// to land on a ship already under thrust cannot be asked for from the command line otherwise.
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle,
			FTimerDelegate::CreateLambda([WeakWorld = TWeakObjectPtr<UWorld>(World), AssetIndex]()
			{
				if (UWorld* LiveWorld = WeakWorld.Get())
				{
					KillThrusterNow(LiveWorld, AssetIndex);
				}
			}),
			static_cast<float>(AfterSeconds), false);

		UE_LOG(LogStarForge, Display,
			TEXT("sf.KillThruster %d: scheduled %.2f s of game time from now."), AssetIndex, AfterSeconds);
	}

	/**
	 * Destroys a thruster by its index in the ship data asset, on the hulls that answer the
	 * console -- a person's ship, never an automated one. It exists so spec 5's consequence can be
	 * measured on the spike rig without a battle: aiming, firing and shields are three mechanisms
	 * that could each be wrong and put the blame on a solver that is innocent.
	 *
	 * A COMMAND, not a variable, and that is what makes it usable headless. -ExecCmds applies its
	 * whole list at startup, and a variable polled afterwards hears only the last value in it --
	 * the reason sf.Dock 2 exists -- so twelve kills in one list would have been one. A command
	 * runs once per entry.
	 *
	 * The optional second argument delays the kill by that many seconds of game time, because
	 * every entry of that list runs on the first frame, before the ship has thrust to lose.
	 */
	FAutoConsoleCommandWithWorldAndArgs CmdKillThruster(
		TEXT("sf.KillThruster"),
		TEXT("sf.KillThruster N [Seconds]: destroy thruster N, by its index in the ship data asset, on the player's ship, and\n")
		TEXT("rebuild the array over the survivors -- now, or after that many seconds of game time. Acts once per command.\n")
		TEXT("An index that is not a surviving thruster is refused out loud when the kill is due."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KillThrusterFromConsole));
}

USFThrusterArrayComponent::USFThrusterArrayComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USFThrusterArrayComponent::BeginPlay()
{
	Super::BeginPlay();
	Rebuild();
}

UPrimitiveComponent* USFThrusterArrayComponent::ResolveBody() const
{
	AActor* Owner = GetOwner();
	return Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
}

bool USFThrusterArrayComponent::Rebuild()
{
	bReady = false;

	// Where every surviving thruster's throttle stands, kept by ASSET index before the columns are
	// replaced, and written back into the new columns by asset index below. A thruster lost in flight
	// takes its own thrust with it and nobody else's: spec 5 says a thruster spools rather than
	// snaps, and zeroing the survivors here snapped every one of them to nothing and made the whole
	// ship's thrust dip for about three response times on every loss.
	//
	// Nothing resets the throttles in this function any more, and no path needs it to. On the first
	// build there is nothing to carry. On undock there is nothing spooled to carry: docking locks the
	// array and clears its demand, and a locked array clears it again on every ApplyDemand, so the
	// arrays are already zero and stay zero. Carried only while they are parallel to the live set,
	// which is to say only from an array that was built; a failed build below leaves them empty.
	TMap<int32, double> SpooledByAsset;
	TMap<int32, double> TargetByAsset;
	if (LastThrottles.Num() == LiveAssetIndices.Num() && TargetThrottles.Num() == LiveAssetIndices.Num())
	{
		for (int32 Column = 0; Column < LiveAssetIndices.Num(); ++Column)
		{
			SpooledByAsset.Add(LiveAssetIndices[Column], LastThrottles[Column]);
			TargetByAsset.Add(LiveAssetIndices[Column], TargetThrottles[Column]);
		}
	}

	// Every failure below leaves the readouts empty. Every reader asks IsReady first today, but the
	// throttles, the capability and the last wrench would otherwise go on describing the columns of
	// an array that no longer exists, and a stale reading looks exactly like a real one.
	const auto Refuse = [this]()
	{
		TargetThrottles.Reset();
		LastThrottles.Reset();
		ResponseTimes.Reset();
		Capability = StarForge::FWrench();
		LastDemand = StarForge::FWrench();
		LastAchieved = StarForge::FWrench();
		return false;
	};

	if (!ShipData)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Thruster array on [%s] has no ship data asset; the ship will not fly."), *GetNameSafe(GetOwner()));
		return Refuse();
	}

	// The whole asset with the destroyed set taken away, every time, rather than the last build
	// with one more column removed. Undocking rebuilds too, for its own reason, and a rebuild that
	// started from the asset without the set would put every destroyed thruster back on the ship.
	const TArray<StarForge::FThrusterDef> AssetThrusters = ShipData->BuildThrusters();

	TArray<StarForge::FThrusterDef> Thrusters;
	LiveAssetIndices.Reset();
	for (int32 AssetIndex = 0; AssetIndex < AssetThrusters.Num(); ++AssetIndex)
	{
		if (!DestroyedAssetIndices.Contains(AssetIndex))
		{
			LiveAssetIndices.Add(AssetIndex);
			Thrusters.Add(AssetThrusters[AssetIndex]);
		}
	}

	UPrimitiveComponent* Body = ResolveBody();
	if (!Body || !Body->GetBodyInstance())
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Thruster array on [%s] found no physics body on the root component."), *GetNameSafe(GetOwner()));
		return Refuse();
	}

	FBodyInstance* Instance = Body->GetBodyInstance();

	// Spec 9: the flight model is balanced by editing the asset, not by rebuilding
	// C++, so the asset's mass has to reach the body. Set before the read below and
	// not after: Chaos rescales the inertia tensor when mass changes, so reading
	// first would capture the values belonging to the old mass. Set through the
	// component rather than the body instance: only this path recomputes the mass
	// properties, and it is those recomputed values that are read back below.
	Body->SetMassOverrideInKg(NAME_None, static_cast<float>(ShipData->MassKg), true);

	// Chaos is authoritative here. The asset says what the ship weighs; Chaos works
	// out how that mass is distributed, and it is Chaos that integrates the motion.
	const FTransform MassSpace = Instance->GetMassSpaceLocal();

	MassProperties.Mass = Instance->GetBodyMass();
	MassProperties.CentreOfMass = MassSpace.GetLocation() / StarForge::Units::CmPerMetre;
	MassProperties.InertiaDiagonal = StarForge::Units::InertiaUuToSi(Instance->GetBodyInertiaTensor());
	MassProperties.InertiaRotation = MassSpace.GetRotation();

	if (!Allocator.Build(Thrusters, MassProperties.CentreOfMass))
	{
		// Six axes needs opposed pairs: with throttles confined to [0, 1] an array
		// pushing only along the positive axes cannot torque without also shoving.
		//
		// Once thrusters have been destroyed the asset is not what is wrong, and the hint
		// would send a reader to it. Two full corners of the standard layout carry every
		// opposed pair and still cannot span.
		const FString Why = DestroyedAssetIndices.Num() == 0
			? FString(TEXT("Check that every axis has an opposed pair."))
			: FString::Printf(TEXT("%d of the asset's %d are destroyed, and the survivors no longer span."),
				DestroyedAssetIndices.Num(), AssetThrusters.Num());

		UE_LOG(LogStarForge, Error,
			TEXT("Thruster layout of %d thrusters on [%s] cannot span all six axes; the ship will not fly. %s"),
			Thrusters.Num(), *GetNameSafe(GetOwner()), *Why);
		return Refuse();
	}

	// Achievable, not the capability sum. The stick is scaled by this figure, and
	// the sum counts opposed thrusters that can never push the way being asked --
	// about twice the truth on a balanced array, which saturates the stick at half
	// travel and leaves its whole upper half doing nothing.
	for (int32 Axis = 0; Axis < 6; ++Axis)
	{
		const double Value = Allocator.GetAxisAchievable(Axis);
		if (Axis < 3)
		{
			Capability.Force[Axis] = Value;
		}
		else
		{
			Capability.Torque[Axis - 3] = Value;
		}
	}

	LastThrottles.Reset();
	LastThrottles.SetNumZeroed(Thrusters.Num());
	TargetThrottles.Reset();
	TargetThrottles.SetNumZeroed(Thrusters.Num());
	for (int32 Column = 0; Column < LiveAssetIndices.Num(); ++Column)
	{
		if (const double* Spooled = SpooledByAsset.Find(LiveAssetIndices[Column]))
		{
			LastThrottles[Column] = *Spooled;
		}
		if (const double* Target = TargetByAsset.Find(LiveAssetIndices[Column]))
		{
			TargetThrottles[Column] = *Target;
		}
	}

	// What the carried throttles push with now, over the survivors. The last value was computed over
	// the old columns and still counts the thruster that has just gone.
	LastAchieved = Allocator.ComputeAchieved(LastThrottles);

	ResponseTimes.Reset();
	ResponseTimes.Reserve(Thrusters.Num());
	for (const StarForge::FThrusterDef& Thruster : Thrusters)
	{
		ResponseTimes.Add(Thruster.ResponseTime);
	}

	bReady = true;

	UE_LOG(LogStarForge, Display,
		TEXT("Thruster array on [%s] ready: %d thrusters, %.0f kg, inertia (%.0f, %.0f, %.0f) kg m^2."),
		*GetNameSafe(GetOwner()), Thrusters.Num(), MassProperties.Mass,
		MassProperties.InertiaDiagonal.X, MassProperties.InertiaDiagonal.Y,
		MassProperties.InertiaDiagonal.Z);

	return true;
}

bool USFThrusterArrayComponent::DestroyThruster(int32 AssetIndex)
{
	if (!LiveAssetIndices.Contains(AssetIndex))
	{
		return bReady;
	}

	DestroyedAssetIndices.Add(AssetIndex);
	return Rebuild();
}

int32 USFThrusterArrayComponent::GetThrusterCount() const
{
	return ShipData ? ShipData->Thrusters.Num() : 0;
}

FString USFThrusterArrayComponent::DescribeLossSince(double ForwardNewtonsBefore) const
{
	if (!bReady)
	{
		return FString::Printf(TEXT("%d of %d left, and the array is NOT built over them -- the ship will not fly."),
			GetLiveThrusterCount(), GetThrusterCount());
	}

	// Fore-and-aft rather than forward: this is the X ceiling the stick's full travel is scaled by,
	// the weaker of forward and astern (FThrustAllocator::GetAxisAchievable). On the standard layout
	// a hit takes a corner's +X thruster before its -X, because +X holds the lower index, so under
	// fire the weaker way is always forward and the two words would agree. sf.KillThruster 1 is not
	// bound by that, and a line saying "forward" would then be printing astern.
	const double ForwardNewtonsAfter = Capability.Force.X;
	return FString::Printf(TEXT("%d of %d left; fore-and-aft thrust %.1f -> %.1f kN, %.1f -> %.1f m/s^2."),
		GetLiveThrusterCount(), GetThrusterCount(),
		ForwardNewtonsBefore / 1000.0, ForwardNewtonsAfter / 1000.0,
		ForwardNewtonsBefore / MassProperties.Mass, ForwardNewtonsAfter / MassProperties.Mass);
}

StarForge::FRigidBodyState USFThrusterArrayComponent::ReadState() const
{
	StarForge::FRigidBodyState State;

	const UPrimitiveComponent* Body = ResolveBody();
	if (!Body)
	{
		return State;
	}

	const FTransform ShipToWorld = Body->GetComponentTransform();

	// Velocities come back in world space and centimetres. The control loop thinks
	// in the ship own axes and in metres, because that is where the thrusters are.
	State.LinearVelocity =
		ShipToWorld.InverseTransformVectorNoScale(Body->GetPhysicsLinearVelocity())
		/ StarForge::Units::CmPerMetre;

	// Radians are unitless, so this one only changes frame.
	State.AngularVelocity =
		ShipToWorld.InverseTransformVectorNoScale(Body->GetPhysicsAngularVelocityInRadians());

	return State;
}

void USFThrusterArrayComponent::ClearDemand()
{
	LastDemand = StarForge::FWrench();
	LastAchieved = StarForge::FWrench();

	// Both arrays, not just the commanded one. An earlier version left the spooled
	// throttles alone on the theory that they wind down through the lag by themselves
	// -- they do not: that lag lives in ApplyDemand, which is exactly what stops being
	// called when nobody is commanding. They would freeze at their last value and
	// print as thrusters still open on a ship nobody is flying, which is the same
	// stale reading this function exists to remove.
	for (double& Throttle : TargetThrottles)
	{
		Throttle = 0.0;
	}

	for (double& Throttle : LastThrottles)
	{
		Throttle = 0.0;
	}
}

void USFThrusterArrayComponent::SetLocked(bool bInLocked)
{
	bLocked = bInLocked;
}

void USFThrusterArrayComponent::ApplyDemand(const StarForge::FWrench& DemandShipFrame, double DeltaSeconds)
{
	// Before the readiness check, so a locked array reads as quiet whether or not it
	// ever built. Clearing rather than merely returning: the last demand anybody made
	// would otherwise sit in these accessors for as long as the ship stays docked, and
	// the readout would print a live-looking wrench beside the word "docked".
	if (bLocked)
	{
		ClearDemand();
		return;
	}

	if (!bReady)
	{
		return;
	}

	UPrimitiveComponent* Body = ResolveBody();
	if (!Body)
	{
		return;
	}

	LastDemand = DemandShipFrame;
	Allocator.Solve(DemandShipFrame, TargetThrottles);

	// Spec 5: a thruster spools, it does not snap. Without this the ship changes
	// direction on the exact frame the stick moves, which reads as weightless no
	// matter how much mass the maths was given.
	for (int32 Index = 0; Index < LastThrottles.Num(); ++Index)
	{
		const double Tau = ResponseTimes.IsValidIndex(Index) ? ResponseTimes[Index] : 0.0;

		if (Tau <= UE_DOUBLE_SMALL_NUMBER || DeltaSeconds <= 0.0)
		{
			LastThrottles[Index] = TargetThrottles[Index];
			continue;
		}

		// First-order lag, solved over the step rather than stepped through it. The
		// forward-Euler factor that stood here -- DeltaSeconds / Tau, clamped so a step
		// longer than the time constant could not overshoot and oscillate -- gives a
		// response whose time constant belongs to the frame rate: -dt / ln(1 - dt/Tau) is
		// 0.0411 s at 60 fps and 0.0487 s at 400 against the 0.05 s the data asset
		// authors, 19 per cent apart, and at 60 fps dt/Tau is a third, nowhere near the
		// small-step regime where Euler and the lag agree. Settled thrust is unaffected,
		// which is why every stage-2 rate check passed at both rates and this stood until
		// stage 4 read the TRANSIENT: the frames after a thruster is lost, and the margin
		// the assist spends catching up to a speed it is always slightly behind.
		//
		// The exponential is the exact solution of the lag over the step, so a step of T
		// leaves the throttle where N steps of T/N do, and it lies in [0, 1) by
		// construction -- there is nothing left to clamp.
		const double Alpha = 1.0 - FMath::Exp(-DeltaSeconds / Tau);
		LastThrottles[Index] += (TargetThrottles[Index] - LastThrottles[Index]) * Alpha;
	}

	LastAchieved = Allocator.ComputeAchieved(LastThrottles);

	// What the thrusters actually manage, not what was asked for. Applying the
	// demand instead would give the ship thrust it does not have.
	const StarForge::FWrench Uu = StarForge::Units::WrenchToUu(LastAchieved);

	const FTransform ShipToWorld = Body->GetComponentTransform();

	// Nothing here wakes the body. Waking it was tried on 2026-09-10 and measured to
	// do nothing: a sleeping hull is re-slept within the same step, because what
	// decides the matter is the velocity gained in one step and not whether the body
	// was awake when the torque arrived. The fix is on the hull, which is set never to
	// sleep -- see ASFTestShip's constructor for the measurements.

	// AddForce with no bone applies at the centre of mass, so it adds no torque of
	// its own; the torque below is the whole of it.
	Body->AddForce(ShipToWorld.TransformVectorNoScale(Uu.Force));
	Body->AddTorqueInRadians(ShipToWorld.TransformVectorNoScale(Uu.Torque));
}
