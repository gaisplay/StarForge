#include "Ship/SFDamageableComponent.h"

#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Ship/SFShipDataAsset.h"
#include "Ship/SFShipFrameComponent.h"
#include "Ship/SFThrusterArrayComponent.h"
#include "Ship/SFTurretComponent.h"
#include "StarForgeLog.h"
#include "Units.h"

namespace
{
	/**
	 * The shields' negative control, and a NAMED EXCEPTION to ESFShipCrewing's rule that an
	 * Automated hull answers no sf.* variable: every hull reads this one, automated or not. The
	 * reason is checkpoint 7's control C1, which turns off the PIRATE's shields and reads the
	 * pirate's hull; a switch the automated hull ignored could not be run at all. It sits beside
	 * the pursuit operator's sf.PirateAI, the stage's other exception, and like sf.ProjectileSweep
	 * it is a question about the mechanism rather than a command addressed to the player's ship.
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
	Turret = Owner ? Owner->FindComponentByClass<USFTurretComponent>() : nullptr;

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

	// Not part of being ready: a ship whose radius is unusable can still be shot, and only the
	// thrusters are out of reach. Said here, once, because the search refuses such a radius on
	// every hit without saying anything.
	const double Radius = ShipData->ThrusterKillRadiusMetres;
	if (!(Radius > 0.0) || !FMath::IsFinite(Radius))
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Damage: [%s] ship data has thruster kill radius %.2f m -- not finite and positive; no hit will destroy a thruster."),
			*GetNameSafe(Owner), Radius);
		return;
	}
	KillRadiusMetres = Radius;
}

void USFDamageableComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Not while powered down: PowerDown has put every face at zero, and there it stays. Powered
	// down means nothing that draws power works, and the shields draw power. A Combat line
	// reading "powered down" beside faces climbing back up would be false, and it is exactly the
	// line recorded for a drifting pirate.
	if (!bReady || bPoweredDown)
	{
		return;
	}

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

		// Gated as the shielded path below is gated on what passed to the hull: only damage the hull
		// actually takes reaches the hull path, so a hit carrying nothing destroys no thruster,
		// whatever it landed next to.
		if (Damage > 0.0 && ApplyToHull(Damage, ShipLocal))
		{
			PowerDown();
		}
		return;
	}

	FHitOutcome Outcome;
	if (!ApplyHit(Shields, Face, Damage, Outcome))
	{
		UE_LOG(LogStarForge, Error,
			TEXT("Hit on [%s] %s face: the shield maths refused %.1f damage -- not a finite non-negative number, ")
			TEXT("or not one of the six faces -- so the hit is dropped."),
			*ShipName, FaceName(Face), Damage);
		return;
	}

	// Only what got past the face reaches the hull path. A hit the face absorbed whole did
	// nothing to the hull, and the hull path is where a hit's place destroys a thruster.
	bool bHullExhaustedNow = false;
	if (Outcome.PassedToHull > 0.0)
	{
		bHullExhaustedNow = ApplyToHull(Outcome.PassedToHull, ShipLocal);
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

bool USFDamageableComponent::ApplyToHull(double Damage, const FVector& ShipLocalCm)
{
	const bool bWasUp = HullStrength > 0.0;
	HullStrength = FMath::Max(0.0, HullStrength - Damage);

	DestroyThrusterNear(ShipLocalCm);

	return bWasUp && HullStrength <= 0.0;
}

void USFDamageableComponent::DestroyThrusterNear(const FVector& ShipLocalCm)
{
	// Nothing to lose. An array without an asset has already said so in red at startup, and a hull
	// without an array has no thrusters.
	if (!Thrusters || !Thrusters->ShipData)
	{
		return;
	}

	// The layout from the array's own asset, so an index found here is an index the array knows.
	// Metres, because FThrusterDef::Location is; crossed through StarForge::Units like every other
	// crossing of that boundary.
	const TArray<StarForge::FThrusterDef> Layout = Thrusters->ShipData->BuildThrusters();
	const FVector HitMetres(
		StarForge::Units::CmToMetres(ShipLocalCm.X),
		StarForge::Units::CmToMetres(ShipLocalCm.Y),
		StarForge::Units::CmToMetres(ShipLocalCm.Z));

	int32 AssetIndex = INDEX_NONE;
	if (!StarForge::Damage::FindThrusterHitBy(
			Layout, Thrusters->GetLiveAssetIndices(), HitMetres, KillRadiusMetres, AssetIndex))
	{
		return;
	}

	const FString ShipName = GetNameSafe(GetOwner());
	const FVector At = Layout[AssetIndex].Location;
	const double Distance = FVector::Dist(At, HitMetres);
	const double ForwardBefore = Thrusters->GetArrayCapability().Force.X;

	// The array rebuilds over the survivors, and if they no longer span six axes it refuses in red
	// on its own. That refusal is the ship not flying, loudly, and it is what spec 5 is paid in.
	Thrusters->DestroyThruster(AssetIndex);

	UE_LOG(LogStarForge, Display,
		TEXT("Thruster %d on [%s] destroyed at (%.1f %.1f %.1f) m by a hit %.2f m away. %s"),
		AssetIndex, *ShipName, At.X, At.Y, At.Z, Distance, *Thrusters->DescribeLossSince(ForwardBefore));
}

void USFDamageableComponent::PowerDown()
{
	bPoweredDown = true;

	const FString ShipName = GetNameSafe(GetOwner());

	// Before the array, so a hull with no array to lock still loses its shields and its gun.
	for (int32 i = 0; i < StarForge::Damage::FaceCount; ++i)
	{
		Shields.Charge[i] = 0.0;
	}

	// Locked and cleared in the same frame, as the array is below, so the Turret line stops
	// printing the last bearing now rather than at the gunner's next tick.
	if (Turret)
	{
		Turret->SetLocked(true);
		Turret->ClearCommand();
	}
	const TCHAR* TurretText = Turret ? TEXT("turret locked") : TEXT("no turret to lock");

	if (!Thrusters)
	{
		UE_LOG(LogStarForge, Error,
			TEXT("[%s] is powered down: hull strength exhausted, all six shield faces at zero, %s, but it has no thruster array ")
			TEXT("to lock -- nothing stops it flying."),
			*ShipName, TurretText);
		return;
	}

	// The same two calls docking makes, in the same order, and only the second still does
	// anything. The lock is already on: bPoweredDown, set on this function's first line, is what
	// the array's IsLocked asks, so every ApplyDemand from that instant refuses. SetLocked(true)
	// is redundant and harmless -- the docking bool is read by IsLocked alone, and an undock that
	// clears it leaves the power-down standing (see PowerDown's declaration). ClearDemand is what
	// stops the readout printing the last demand, and zeroes the spooled throttles, in this frame
	// rather than the next.
	Thrusters->SetLocked(true);
	Thrusters->ClearDemand();

	UE_LOG(LogStarForge, Display,
		TEXT("[%s] is powered down: hull strength exhausted. Thrusters locked, %s, all six shield faces at zero and not ")
		TEXT("regenerating; posts will stand aside without being told why; the hull coasts and anyone aboard stays aboard."),
		*ShipName, TurretText);
}
