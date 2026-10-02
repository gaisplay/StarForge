#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFWorldBoundsSubsystem.generated.h"

class IConsoleVariable;

/**
 * Says, in every game world, whether the engine will destroy what flies past its borders.
 *
 * The project turns the world-bounds checks off in Config/DefaultEngine.ini (spec 4): with them
 * on, a ship below KillZ = -10 485.75 m vanishes without a single log line. Turning them off costs
 * the border the one line the engine had for it, and a setting nobody can see is a setting nobody
 * can tell has been lost. So this class writes the line instead (spec 13).
 *
 * It READS everything back from the world -- AreWorldBoundsChecksEnabled(), which combines both
 * switches, the map's own flag and its KillZ -- and asserts nothing. The line must be able to
 * fail: one that quoted the ini would stay calm with the checks back on, which is exactly the
 * indicator this project refuses to build.
 *
 * A world subsystem rather than code in a game mode, because there are four independent rigs,
 * each deriving straight from AGameModeBase, and every current and future map must get the line
 * without anyone remembering to add it.
 *
 * It reports again whenever WorldBounds.DisableChecks changes, not only at the start. The check
 * reads the state at the moment of each CheckStillInWorld, so a variable flipped mid-run changes
 * what happens from that frame on; and the hand-performed negative control types
 * "WorldBounds.DisableChecks 0" into a running game and must see the Error at once.
 */
UCLASS()
class STARFORGE_API USFWorldBoundsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	/** Game and PIE only: an editor or preview world never simulates, so its line would be noise. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void OnDisableChecksChanged(IConsoleVariable* Variable);

	void Report();

	FDelegateHandle DisableChecksChangedHandle;

	/**
	 * What the last line said. The change delegate also fires on a Set that leaves the value where
	 * it was, and the contract is one line per real change, not one per console command.
	 */
	bool bHasReported = false;
	bool bReportedChecksOn = false;
	bool bReportedMapFlag = false;
	FString ReportedVariableText;
};
