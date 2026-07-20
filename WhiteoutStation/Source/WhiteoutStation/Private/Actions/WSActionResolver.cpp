#include "Actions/WSActionResolver.h"

#include "State/WindStationStateSubsystem.h"

void UWSActionResolver::Initialize(UWindStationStateSubsystem* InStateSubsystem)
{
	StateSubsystem = InStateSubsystem;
}

FWSActionPreview UWSActionResolver::Preview(const FWSActionRequest& Request) const
{
	return StateSubsystem ? StateSubsystem->PreviewAction(Request) : FWSActionPreview();
}

FWSActionResult UWSActionResolver::Commit(const FWSActionRequest& Request)
{
	return StateSubsystem ? StateSubsystem->CommitAction(Request) : FWSActionResult();
}
