#pragma once
#include "CoreMinimal.h"
#include "HUD/WSTutorialTypes.h"

// Presentation-only state: no world, rules or save-game reference.
class WHITEOUTSTATION_API FWSTutorialFlow
{
public:
	static constexpr int32 Version = 1;
	static constexpr int32 PageCount = 5;
	static bool NeedsAutoShow(const FWSTutorialPreference& Preference, bool bLoadedSnapshot);
	static int32 PageIndex(FName PageId);
	static FName PageId(int32 Index);
	void Open(const FWSTutorialPreference& Preference, bool bReplay);
	void AwaitRelease();
	void TickInput(bool bAnyInputDown, double Now);
	bool Press();
	bool Release(double Now);
	bool Navigate(int32 Direction, double Now);
	void Close();
	bool IsActive() const { return bActive; }
	bool IsReplay() const { return bReplaying; }
	bool IsArmed() const { return bArmed; }
	int32 GetPage() const { return Page; }
private:
	bool bActive = false;
	bool bReplaying = false;
	bool bArmed = false;
	bool bPressed = false;
	int32 ReleasedFrames = 0;
	int32 Page = 0;
	double TransitionUntil = 0;
};
