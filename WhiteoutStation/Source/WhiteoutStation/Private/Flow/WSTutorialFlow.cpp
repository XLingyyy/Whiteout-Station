#include "Flow/WSTutorialFlow.h"

bool FWSTutorialFlow::NeedsAutoShow(const FWSTutorialPreference& P, bool bLoadedSnapshot)
{
	return !bLoadedSnapshot && P.Version == Version
		&& (P.Status == EWSTutorialStatus::Never || P.Status == EWSTutorialStatus::InProgress);
}

int32 FWSTutorialFlow::PageIndex(FName Id)
{
	for (int32 I = 0; I < PageCount; ++I) if (PageId(I) == Id) return I;
	return 0;
}

FName FWSTutorialFlow::PageId(int32 Index)
{
	return FName(*FString::Printf(TEXT("T%02d"), FMath::Clamp(Index, 0, PageCount - 1) + 1));
}

void FWSTutorialFlow::Open(const FWSTutorialPreference& P, bool bReplay)
{
	bActive = true; bReplaying = bReplay;
	Page = bReplay ? 0 : PageIndex(P.LastPageId);
	TransitionUntil = 0;
	AwaitRelease();
}

void FWSTutorialFlow::AwaitRelease()
{
	bArmed = false; bPressed = false; ReleasedFrames = 0;
}

void FWSTutorialFlow::TickInput(bool bAnyInputDown, double Now)
{
	if (!bActive || bArmed) return;
	if (bAnyInputDown || Now < TransitionUntil) { ReleasedFrames = 0; return; }
	if (++ReleasedFrames >= 2) bArmed = true;
}

bool FWSTutorialFlow::Press()
{
	bPressed = bActive && bArmed;
	return bPressed;
}

bool FWSTutorialFlow::Release(double Now)
{
	const bool bAccept = bPressed && bArmed && bActive && Now >= TransitionUntil;
	bPressed = false;
	return bAccept;
}

bool FWSTutorialFlow::Navigate(int32 Direction, double Now)
{
	if (!bActive || !bArmed || Now < TransitionUntil) return false;
	const int32 Next = FMath::Clamp(Page + Direction, 0, PageCount - 1);
	if (Next == Page) return false;
	Page = Next;
	TransitionUntil = Now + 0.14;
	AwaitRelease();
	return true;
}

void FWSTutorialFlow::Close()
{
	bActive = false;
	AwaitRelease();
}
