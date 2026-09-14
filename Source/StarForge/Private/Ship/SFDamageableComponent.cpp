#include "Ship/SFDamageableComponent.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFShipDataAsset.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFThrusterArrayComponent.h"
#include "StarForgeLog.h"

namespace
{
	/**
	 * The shields' negative control. It acts on every hull in the world, automated or not: the
	 * checkpoint control turns off the PIRATE's shields and reads the pirate's hull, and a switch
	 * an automated hull ignored could not be run at all. Like sf.ProjectileSweep it is a question
	 * about the mechanism, not a command addressed to one ship.
	 */
	TAutoConsoleVariable<int32> CVarShields(TEXT("sf.Shields"), 1,
		TEXT("1: a hit is taken by the shield face it lands on. 0: every hit goes straight to the hull, and\n")
		TEXT("each one says so in the log -- the negative control. Acts on every hull, automated or not."),
		ECVF_Default);
}

USFDamageableComponent::USFDamageableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USFDamageableComponent::BeginPlay()
{
	Super::BeginPlay();

	const AActor* Owner = GetOwner();
	Frame = Owner ? Owner->FindComponentByClass<USFShipFrameComponent>() : nullptr;
	Thrusters = Owner ? Owner->FindComponentByClass<USFThrusterArrayComponent>() : nullptr;

	if (!ShipData)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Damage: [%s] has no ship data asset, so no shields and no hull strength; nothing on it can be hurt."),
			*GetNameSafe(Owner));
		return;
	}

	Limits = ShipData->BuildShieldLimits();
	HullCapacity = ShipData->HullStrength;

	// Refused rather than clamped. TickShields writes nothing on a limit that is not finite, so
	// such a figure would be shields that never regenerate and never say why; a negative capacity
	// would make every hit pass more than it carried; and a hull that starts at zero could never
	// be taken from above zero to zero, so it would never power down.
	const auto IsUsable = [](double Value) { return Value >= 0.0 && FMath::IsFinite(Value); };
	if (!IsUsable(Limits.FaceCapacity) || !IsUsable(Limits.RegenPerSecond) || !IsUsable(Limits.RegenDelaySeconds)
		|| !IsUsable(HullCapacity) || !(HullCapacity > 0.0))
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Damage: [%s] ship data has shield face %.1f, regen %.1f/s after %.1f s, hull %.1f -- not all finite, ")
			TEXT("non-negative and a hull above zero; nothing on it can be hurt."),
			*GetNameSafe(Owner), Limits.FaceCapacity, Limits.RegenPerSecond, Limits.RegenDelaySeconds, HullCapacity);
		return;
	}

	// FShieldState starts every face empty and hit just now. A ship that has never been shot has
	// every face full, and none of them waiting out a delay after a hit that never happened.
	for (int32 i = 0; i < StarForge::Damage::FaceCount; ++i)
	{
		Shields.Charge[i] = Limits.FaceCapacity;
		Shields.SecondsSinceHit[i] = Limits.RegenDelaySeconds;
	}
	HullStrength = HullCapacity;
	bReady = true;
}

void USFDamageableComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bReady)
	{
		return;
	}

	// Also while powered down. The plan ties a power-down to the thruster lock and to nothing
	// else, and stopping the shields here would be a second consequence nobody decided.
	StarForge::Damage::TickShields(Shields, Limits, DeltaTime);
}

double USFDamageableComponent::GetFaceCharge(StarForge::Damage::EShipFace Face) const
{
	const int32 Index = static_cast<int32>(Face);
	return Index < StarForge::Damage::FaceCount ? Shields.Charge[Index] : 0.0;
}

void USFDamageableComponent::ReceiveHit(const FVector& HitWorldCm, double Damage, AActor* Instigator)
{
	using namespace StarForge::Damage;

	const FString ShipName = GetNameSafe(GetOwner());

	if (!bReady)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Hit on [%s]: no usable shield or hull figures (see the Damage line at startup), so the hit is dropped."),
			*ShipName);
		return;
	}

	FVector ShipLocal;
	if (Frame)
	{
		ShipLocal = Frame->WorldToLocal(FTransform(HitWorldCm)).GetLocation();
	}
	else
	{
		// Loud, and the hit is dropped rather than placed at a made-up coordinate. Without a
		// frame there is no ship-local anything, and classifying the WORLD point would put
		// every hit on whichever face of the origin the shooter happened to be on.
		UE_LOG(LogStarForge, Error,
			TEXT("Hit on [%s]: no frame component, so the hit has no place and is dropped."), *ShipName);
		return;
	}

	EShipFace Face = EShipFace::XPos;
	if (!ClassifyFace(ShipLocal, HalfExtentsCm, Face))
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Hit on [%s] at hull-local (%.0f %.0f %.0f) cm with half-extents (%.0f %.0f %.0f) cm: ")
			TEXT("the point or the half-extents are not usable, so no face is under it and the hit is dropped."),
			*ShipName, ShipLocal.X, ShipLocal.Y, ShipLocal.Z, HalfExtentsCm.X, HalfExtentsCm.Y, HalfExtentsCm.Z);
		return;
	}

	// The negative control, and it is not a no-op: it says out loud that it did something,
	// so a control typed in the wrong place cannot look exactly like a control that found
	// nothing. That is the lesson of sf.BindFrame, which silently rejected a rebinding to
	// the frame a man was already in and left a runbook step that could be "performed"
	// without leaving the room.
	if (CVarShields.GetValueOnGameThread() == 0)
	{
		// ApplyHit's guard does not stand in front of this path, so it is repeated: a negative
		// hit would repair the hull and a NaN would make it NaN, and neither would say so.
		if (!(Damage >= 0.0) || !FMath::IsFinite(Damage))
		{
			UE_LOG(LogStarForge, Error,
				TEXT("Hit on [%s] %s face: %.1f damage is not a finite non-negative number, so the hit is dropped."),
				*ShipName, FaceName(Face), Damage);
			return;
		}

		UE_LOG(LogStarForge, Display,
			TEXT("Hit on [%s] %s face: shields OFF (sf.Shields 0), %.1f straight to hull."),
			*ShipName, FaceName(Face), Damage);

		if (ApplyToHull(Damage, ShipLocal, Instigator))
		{
			PowerDown();
		}
		return;
	}

	FHitOutcome Outcome;
	if (!ApplyHit(Shields, Limits, Face, Damage, Outcome))
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Hit on [%s] %s face: the shield maths refused %.1f damage -- not a finite non-negative number, ")
			TEXT("or not one of the six faces -- so the hit is dropped."),
			*ShipName, FaceName(Face), Damage);
		return;
	}

	// Only what got past the face reaches the hull path. A hit the face absorbed whole did
	// nothing to the hull, and the hull path is where a hit's place is going to matter.
	bool bHullExhaustedNow = false;
	if (Outcome.PassedToHull > 0.0)
	{
		bHullExhaustedNow = ApplyToHull(Outcome.PassedToHull, ShipLocal, Instigator);
	}

	// One line, written after the hull has taken its share so both figures in it are the ones
	// the hit left behind. The face-down and power-down events follow it, in the order they
	// happened.
	UE_LOG(LogStarForge, Display,
		TEXT("Hit on [%s] %s face at hull-local (%.0f %.0f %.0f) cm: %.1f damage, %.1f absorbed, %.1f to hull; ")
		TEXT("face %.1f/%.1f, hull %.1f/%.1f."),
		*ShipName, FaceName(Face), ShipLocal.X, ShipLocal.Y, ShipLocal.Z,
		Damage, Outcome.AbsorbedByShield, Outcome.PassedToHull,
		GetFaceCharge(Face), Limits.FaceCapacity, HullStrength, HullCapacity);

	if (Outcome.bFaceBrokenNow)
	{
		UE_LOG(LogStarForge, Display, TEXT("Shield face %s is down on [%s]."), FaceName(Face), *ShipName);
	}

	if (bHullExhaustedNow)
	{
		PowerDown();
	}
}

bool USFDamageableComponent::ApplyToHull(double Damage, const FVector& ShipLocalCm, AActor* Instigator)
{
	const bool bWasUp = HullStrength > 0.0;
	HullStrength = FMath::Max(0.0, HullStrength - Damage);
	return bWasUp && HullStrength <= 0.0;
}

void USFDamageableComponent::PowerDown()
{
	bPoweredDown = true;

	const FString ShipName = GetNameSafe(GetOwner());

	if (!Thrusters)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("[%s] is powered down: hull strength exhausted, but it has no thruster array to lock -- nothing stops it flying."),
			*ShipName);
		return;
	}

	// The same two calls docking makes, in the same order: locked so nothing can command the
	// hull, cleared so the readout stops printing the last demand in this frame rather than the
	// next. Not a second mechanism -- see PowerDown's declaration for what this lock does not hold.
	Thrusters->SetLocked(true);
	Thrusters->ClearDemand();

	UE_LOG(LogStarForge, Display,
		TEXT("[%s] is powered down: hull strength exhausted. Thrusters locked, posts will stand aside without being told why; ")
		TEXT("the hull coasts and anyone aboard stays aboard."),
		*ShipName);
}
