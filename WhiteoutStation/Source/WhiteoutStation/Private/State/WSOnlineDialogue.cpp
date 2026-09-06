#include "State/WindStationStateSubsystem.h"
#include "Agents/WSAgentGateway.h"

void UWindStationStateSubsystem::ResolveOnlineIntent(FName ActionId, const FString& Text,
	FGuid SessionId, TFunction<void(bool, const FWSCanonicalIntent&, const FString&)> Completion)
{
	FWSActionRequest Request;
	Request.ActionId = ActionId; Request.DialogueSessionId = SessionId;
	EWSReasonCode Reason;
	if (GetDialogueMode() != EWSDialogueMode::Online || HasPendingDialogue()
		|| bCommitDispatchActive || bLifecycleTransitionActive || Text.IsEmpty() || Text.Len() > 480
		|| !NormalizeDialogueSessionRequest(Request, Reason))
	{ Completion(false, {}, TEXT("当前无法发送，请检查会话与 AI 设置。")); return; }
	FWSDialogueSessionRuntimeState& Session = DialogueSessions.FindOrAdd(SessionId);
	Session.ActionId = ActionId; Session.DayPhase = RulesEngine.GetState().DayPhase;
	TArray<FString> History = Session.SafeConversation;
	if (Session.PendingCommitment.IsSet())
	{
		History.Add(TEXT("待玩家确认的承诺：") + Session.PendingCommitment->PromiseCondition.ToString());
	}
	bHasPendingOnlineIntent = true;
	const int64 Generation = ++DialogueGeneration;
	const int64 Revision = StateRevision;
	const double Deadline = FPlatformTime::Seconds() + 10.0;
	const FName Speaker = ActionId == TEXT("talk_ye_cheng") ? FName(TEXT("ye_cheng")) : FName(TEXT("gu_heng"));
	TWeakObjectPtr<UWindStationStateSubsystem> WeakThis(this);
	AgentGateway->RequestCanonicalIntent(Text, Speaker, History, Session.CommittedTurns,
		[WeakThis, Generation, Revision, SessionId, ActionId, Deadline, Completion](bool Valid,
			const FWSCanonicalIntent& Parsed, const FString& Error)
		{
			if (!WeakThis.IsValid() || WeakThis->DialogueGeneration != Generation) return;
			UWindStationStateSubsystem* Self = WeakThis.Get();
			Self->bHasPendingOnlineIntent = false;
			FWSDialogueSessionRuntimeState* Current = Self->DialogueSessions.Find(SessionId);
			if (!Current || Current->ActionId != ActionId || Self->StateRevision != Revision)
			{ Completion(false, {}, TEXT("情况已变化，请按当前状态重新交谈。")); return; }
			if (!Valid) { Completion(false, {}, TEXT("连接或意图解析失败，本次未扣费。请重新输入或在设置中切换离线。")); return; }
			if (!Parsed.CanPlan()) { Completion(false, {}, TEXT("请说明对象和想做的事。本次未扣费。")); return; }
			FWSCanonicalIntent Intent = Parsed;
			if (Intent.Commitment == EWSCommitmentIntent::RejectPending)
			{
				Current->PendingCommitment.Reset();
				Completion(false, {}, TEXT("已取消待确认事项，本次未扣费。")); return;
			}
			if (Intent.Commitment == EWSCommitmentIntent::ConfirmPending)
			{
				if (!Current->PendingCommitment.IsSet() || Current->PendingCommitmentRevision != Revision)
				{ Completion(false, {}, TEXT("当前没有有效的待确认事项，请说明具体安排。")); return; }
				Intent = Current->PendingCommitment.GetValue();
				Intent.Commitment = EWSCommitmentIntent::ConfirmPending;
				Current->PendingCommitment.Reset();
			}
			else if (Intent.Commitment == EWSCommitmentIntent::Proposed)
			{
				FWSActionRequest Preview; Preview.ActionId = ActionId; Preview.DialogueSessionId = SessionId;
				Intent.ApplyTo(Preview);
				if (!Self->PreviewAction(Preview).bCanExecute)
				{ Completion(false, {}, TEXT("这项承诺当前无法履行，请调整安排。")); return; }
				Current->PendingCommitment = Intent; Current->PendingCommitmentRevision = Revision;
				const FString Terms = Intent.PromiseCondition == TEXT("heat_repair_room") ? TEXT("下一阶段给维修间供暖")
					: Intent.PromiseCondition == TEXT("keep_records") ? TEXT("保留记录") : TEXT("预留药品");
				Completion(false, {}, TEXT("请确认：你承诺") + Terms + TEXT("。请用下一条消息明确同意或拒绝；本次未扣费。")); return;
			}
			Intent.DeadlineSeconds = Deadline;
			Completion(true, Intent, TEXT("正在回应…"));
		});
}
