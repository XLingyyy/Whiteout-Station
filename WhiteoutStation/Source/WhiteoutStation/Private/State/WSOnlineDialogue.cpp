#include "State/WindStationStateSubsystem.h"
#include "Agents/WSAgentGateway.h"

namespace
{
	EWSCommitmentIntent PureControl(const FString& Text)
	{
		FString Value = Text.TrimStartAndEnd();
		while (Value.EndsWith(TEXT("。")) || Value.EndsWith(TEXT("！")) || Value.EndsWith(TEXT("."))) Value.LeftChopInline(1);
		if (Value == TEXT("确认") || Value == TEXT("确认承诺") || Value == TEXT("确认这项承诺") || Value == TEXT("同意") || Value == TEXT("好")) return EWSCommitmentIntent::ConfirmPending;
		if (Value == TEXT("取消") || Value == TEXT("取消承诺") || Value == TEXT("取消这项承诺") || Value == TEXT("不确认")) return EWSCommitmentIntent::RejectPending;
		return EWSCommitmentIntent::None;
	}
	FString DescribeTerms(const TArray<FWSPromiseTerms>& Terms)
	{
		TArray<FString> Lines;
		for (const auto& Term : Terms)
		{
			if (Term.Kind == TEXT("heat_zone"))
			{
				const TCHAR* Zone = Term.Zone == EWSHeatingZone::RepairRoom ? TEXT("维修间")
					: Term.Zone == EWSHeatingZone::MedicalRoom ? TEXT("医务室")
					: Term.Zone == EWSHeatingZone::Kitchen ? TEXT("厨房") : TEXT("控制室");
				Lines.Add(FString::Printf(TEXT("%s给%s供暖"), Term.DuePhase == 1 ? TEXT("下午") : TEXT("黄昏"), Zone));
			}
			else Lines.Add(Term.Kind == TEXT("keep_records") ? TEXT("保留记录至本日结束") : TEXT("预留药品至本日结束"));
		}
		return FString::Join(Lines, TEXT("；"));
	}
	bool SameTerms(const TArray<FWSPromiseTerms>& A, const TArray<FWSPromiseTerms>& B)
	{
		if (A.Num() != B.Num()) return false;
		for (int32 I = 0; I < A.Num(); ++I) if (!A[I].SameTerms(B[I])) return false;
		return true;
	}
}

bool UWindStationStateSubsystem::ResolveParsedOnlineMessage(FName ActionId, const FString& Text,
	FGuid SessionId, const FWSCanonicalIntent& Parsed, FWSCanonicalIntent& Out, FString& Status, bool bMessageCounted)
{
	Out = {}; Status.Reset();
	if (HasPendingDialogue() || bCommitDispatchActive || bLifecycleTransitionActive || !SessionId.IsValid())
	{ Status = TEXT("当前事务尚未完成。"); return false; }
	FWSDialogueSessionRuntimeState& Session = DialogueSessions.FindOrAdd(SessionId);
	if (!Session.ActionId.IsNone() && (Session.ActionId != ActionId || Session.DayPhase != RulesEngine.GetState().DayPhase))
	{ Status = TEXT("会话状态已变化。"); return false; }
	Session.ActionId = ActionId; Session.DayPhase = RulesEngine.GetState().DayPhase;
	Session.LatestMessageId = FGuid::NewGuid(); if (!bMessageCounted) ++Session.MessageCount; Session.ResolvedMessage.Reset();
	Session.LastParsedMessage = Parsed;
	TArray<FWSCanonicalIntent> Parts = Parsed.Parts.IsEmpty() ? TArray<FWSCanonicalIntent>{Parsed} : Parsed.Parts;
	int32 ProposalPart = INDEX_NONE;
	for (int32 I = 0; I < Parts.Num(); ++I)
	{
		if (Parts[I].Commitment != EWSCommitmentIntent::Proposed) continue;
		if (ProposalPart == INDEX_NONE) { ProposalPart = I; continue; }
		Parts[ProposalPart].Terms.Append(Parts[I].Terms);
		Parts[ProposalPart].bNeedsClarification |= Parts[I].bNeedsClarification;
		Parts[ProposalPart].Clarification += Parts[I].Clarification;
		Parts.RemoveAt(I--);
	}
	const bool Closing = GetDialogueTurnsUsed(ActionId) >= GetDialogueTurnLimit();
	if (Closing && (Parts.Num() != 1 || !Session.PendingCommitment.IsSet()
		|| PureControl(Text) == EWSCommitmentIntent::None
		|| (Parts[0].Commitment != EWSCommitmentIntent::ConfirmPending && Parts[0].Commitment != EWSCommitmentIntent::RejectPending)))
	{ Status = TEXT("本局交谈额度已用完，仅可输入“确认”或“取消”收尾当前提议。"); return false; }
	TArray<FWSCanonicalIntent> Ready;
	TArray<FString> Notices;
	for (FWSCanonicalIntent Part : Parts)
	{
		Part.Parts.Reset();
		if (Part.Frame.Confidence < 0.75f)
		{ Notices.Add(TEXT("这部分我还没听清，请说明对象和具体意思。")); continue; }
		if (Part.Commitment == EWSCommitmentIntent::RejectPending)
		{
			if (Session.PendingCommitment.IsSet() && Part.ProposalId == Session.PendingCommitment->ProposalId
				&& Part.ProposalVersion == Session.PendingCommitment->ProposalVersion)
			{ Session.PendingCommitment.Reset(); Notices.Add(TEXT("已取消待确认事项。")); }
			else Notices.Add(TEXT("没有与本次引用相符的待确认事项，未取消其他提议。"));
			continue;
		}
		if (Part.Commitment == EWSCommitmentIntent::Proposed || Part.Commitment == EWSCommitmentIntent::ConfirmPending)
		{
			for (auto& Term : Part.Terms)
				if (Term.DuePhase == INDEX_NONE && Term.PhaseOffset > 0)
					Term.DuePhase = static_cast<int32>(Session.DayPhase) + Term.PhaseOffset;
			if (Part.Commitment == EWSCommitmentIntent::ConfirmPending)
			{
				if (!Session.PendingCommitment.IsSet() || Part.ProposalId != Session.PendingCommitment->ProposalId
					|| Part.ProposalVersion != Session.PendingCommitment->ProposalVersion)
				{ Notices.Add(TEXT("确认引用已失效，请按当前提议及版本重新确认。")); continue; }
				if (!Part.Terms.IsEmpty() && !SameTerms(Part.Terms, Session.PendingCommitment->Terms))
				{
					if (Closing) { Notices.Add(TEXT("三轮已结束，修改条款需要另行讨论；原提议未确认。")); continue; }
					Part.Commitment = EWSCommitmentIntent::Proposed;
				}
				else
				{
					Part.Terms = Session.PendingCommitment->Terms;
					Part.PromiseCondition = Session.PendingCommitment->PromiseCondition;
					Part.Frame.SpeechAct = EWSDialogueAct::Promise;
					Part.bNeedsClarification = false;
					Part.bConfirmationClosure = Parts.Num() == 1 && PureControl(Text) == EWSCommitmentIntent::ConfirmPending;
					Notices.Add(TEXT("已登记：") + DescribeTerms(Part.Terms) + TEXT("。这不代表已经履行。"));
					Ready.Add(Part); continue;
				}
			}
			bool Supported = !Part.Terms.IsEmpty(); bool MissingTime = false;
			for (int32 I = 0; I < Part.Terms.Num(); ++I)
				for (int32 J = I + 1; J < Part.Terms.Num(); ++J)
				{
					if (Part.Terms[I].SameTerms(Part.Terms[J])) { Part.Terms.RemoveAt(J--); continue; }
					if (Part.Terms[I].Kind == TEXT("heat_zone") && Part.Terms[J].Kind == TEXT("heat_zone")
						&& Part.Terms[I].DuePhase != INDEX_NONE && Part.Terms[I].DuePhase == Part.Terms[J].DuePhase)
						Supported = false;
				}
			for (auto& Term : Part.Terms)
			{
				Supported &= Term.Prerequisite.IsEmpty();
				if (Term.Kind == TEXT("heat_zone"))
				{
					MissingTime |= Term.DuePhase == INDEX_NONE;
					Supported &= Term.Zone != EWSHeatingZone::None && (Term.DuePhase == INDEX_NONE
						|| (Term.DuePhase > static_cast<int32>(Session.DayPhase) && Term.DuePhase < 3));
				}
				else if (Term.Kind == TEXT("keep_records") || Term.Kind == TEXT("reserve_medicine")) Term.DuePhase = 3;
				else Supported = false;
			}
			if (!Supported)
			{
				if (Session.PendingCommitment.IsSet() && Part.ProposalId == Session.PendingCommitment->ProposalId)
					Session.PendingCommitment.Reset();
				Notices.Add(TEXT("我理解这项保证，但当前规则不支持这些条款，不能登记；也不会替换成其他承诺。"));
				if (!Part.Clarification.IsEmpty()) Notices.Add(Part.Clarification);
				continue;
			}
			if (Part.bNeedsClarification || MissingTime)
			{
				if (Session.PendingCommitment.IsSet() && Part.ProposalId == Session.PendingCommitment->ProposalId)
					Session.PendingCommitment.Reset();
				Notices.Add(Part.Clarification.IsEmpty() ? TEXT("请明确承诺的地点、期限与前提；供暖没有默认期限。") : Part.Clarification); continue;
			}
			const bool Duplicate = RulesEngine.GetState().Promises.ContainsByPredicate([&](const FWSPromiseRecord& P)
				{ return P.Recipient == (ActionId == TEXT("talk_ye_cheng") ? EWSCharacterId::YeCheng : EWSCharacterId::GuHeng)
					&& Part.Terms.ContainsByPredicate([&](const FWSPromiseTerms& T) { return P.Terms.SameTerms(T); }); });
			if (Duplicate) { Notices.Add(TEXT("这项条款已经登记，不重复登记或奖励。")); continue; }
			Part.ProposalId = Session.PendingCommitment.IsSet() ? Session.PendingCommitment->ProposalId : FGuid::NewGuid();
			Part.ProposalVersion = Session.PendingCommitment.IsSet() ? Session.PendingCommitment->ProposalVersion + 1 : 1;
			Part.PromiseCondition = Part.Terms[0].Kind;
			Part.Frame.SpeechAct = EWSDialogueAct::Promise;
			Session.PendingCommitment = Part; Session.PendingCommitmentRevision = StateRevision;
			Notices.Add(FString::Printf(TEXT("请确认：%s。当前为第 %d 版安排，尚未登记。"),
				*DescribeTerms(Part.Terms), Part.ProposalVersion));
			continue;
		}
		if (!Part.CanPlan())
		{ Notices.Add(Part.Clarification.IsEmpty() ? TEXT("请说明这部分所指的对象或任务；引用、假设不会作为承诺登记。") : Part.Clarification); continue; }
		if (Part.bNeedsClarification)
			Notices.Add(Part.Clarification.IsEmpty() ? TEXT("你具体希望我做哪项工作？") : Part.Clarification);
		Ready.Add(MoveTemp(Part));
	}
	Status = FString::Join(Notices, TEXT("\n"));
	if (Ready.IsEmpty())
	{
		FWSConversationEntry Entry;
		Entry.EntryId = Session.LatestMessageId; Entry.SessionId = SessionId;
		Entry.SpeakerId = ActionId == TEXT("talk_ye_cheng") ? TEXT("ye_cheng") : TEXT("gu_heng");
		Entry.DayPhase = Session.DayPhase; Entry.PlayerLine = Text; Entry.NpcLine = Status;
		Entry.bCountedTurn = PureControl(Text) == EWSCommitmentIntent::None;
		Entry.bCommitted = true; Entry.ReplySource = TEXT("local_control_v16"); Entry.StateRevision = StateRevision;
		Entry.ControlStatus = Session.PendingCommitment.IsSet() ? TEXT("pending") : TEXT("resolved");
		for (const auto& Part : Parts) if (!Part.TopicId.IsNone()) Entry.Topics.AddUnique(Part.TopicId);
		RulesEngine.RecordConversationEntry(Entry);
		Session.CommittedTurns = GetDialogueTurnsUsed(ActionId);
		++StateRevision;
		SaveSnapshot();
		return false;
	}
	int32 Primary = Ready.IndexOfByPredicate([](const auto& P) { return P.Frame.SpeechAct == EWSDialogueAct::Command; });
	if (Primary == INDEX_NONE) Primary = 0;
	Out = Ready[Primary]; Out.Parts = Ready; Out.MessageId = Session.LatestMessageId;
	Out.Notice = Status; Out.DeadlineSeconds = FPlatformTime::Seconds() + 7.0;
	for (auto& Part : Out.Parts) Part.DeadlineSeconds = Out.DeadlineSeconds;
	Session.ResolvedMessage = Out;
	Status = TEXT("正在回应…"); return true;
}

void UWindStationStateSubsystem::ResolveOnlineIntent(FName ActionId, const FString& Text,
	FGuid SessionId, TFunction<void(bool, const FWSCanonicalIntent&, const FString&)> Completion)
{
	if (GetDialogueMode() != EWSDialogueMode::Online || HasPendingDialogue() || bCommitDispatchActive
		|| bLifecycleTransitionActive || Text.IsEmpty() || Text.Len() > 480 || !SessionId.IsValid())
	{ Completion(false, {}, TEXT("当前无法发送，请检查会话与 AI 设置。")); return; }
	const auto Control = PureControl(Text);
	const auto* Existing = DialogueSessions.Find(SessionId);
	if (Control != EWSCommitmentIntent::None)
	{
		if (!Existing || !Existing->PendingCommitment.IsSet())
		{ Completion(false, {}, TEXT("当前没有可确认或取消的提议。")); return; }
		FWSCanonicalIntent Intent = Existing->PendingCommitment.GetValue();
		Intent.Commitment = Control; Intent.Terms.Reset(); Intent.Parts.Reset();
		FWSCanonicalIntent Resolved; FString Status;
		const bool Ready = ResolveParsedOnlineMessage(ActionId, Text, SessionId, Intent, Resolved, Status);
		Completion(Ready, Resolved, Status); return;
	}
	if (GetDialogueTurnsUsed(ActionId) >= GetDialogueTurnLimit())
	{ Completion(false, {}, TEXT("本局交谈额度已用完。已有提议仅可输入“确认”或“取消”，不能夹带新问题或修改。")); return; }
	FWSActionRequest Check; Check.ActionId = ActionId; Check.DialogueSessionId = SessionId;
	if (!PreviewAction(Check).bCanExecute)
	{ Completion(false, {}, TEXT("当前阶段无法交谈。")); return; }
	if (!RulesEngine.TryRecordModelCall())
	{ Completion(false, {}, TEXT("本局 AI 请求额度已用完，请切换离线对话。")); return; }
	FWSDialogueSessionRuntimeState& Session = DialogueSessions.FindOrAdd(SessionId);
	if (!Session.ActionId.IsNone() && (Session.ActionId != ActionId || Session.DayPhase != RulesEngine.GetState().DayPhase))
	{ Completion(false, {}, TEXT("会话已失效，请重新交谈。")); return; }
	++Session.MessageCount; Session.ResolvedMessage.Reset(); Session.LastParsedMessage.Reset(); Session.LatestMessageId.Invalidate();
	Session.ActionId = ActionId; Session.DayPhase = RulesEngine.GetState().DayPhase;
	TArray<FString> History = BuildOnlineConversationHistory(ActionId, SessionId);
	History.Add(FString::Printf(TEXT("current_day_phase=%d (morning=0, afternoon=1, dusk=2)"), static_cast<int32>(Session.DayPhase)));
	if (Session.PendingCommitment.IsSet())
	{
		History.Add(FString::Printf(TEXT("pending_proposal id=%s version=%d terms=%s"),
			*Session.PendingCommitment->ProposalId.ToString(EGuidFormats::DigitsWithHyphens),
			Session.PendingCommitment->ProposalVersion, *DescribeTerms(Session.PendingCommitment->Terms)));
		for (const auto& Term : Session.PendingCommitment->Terms)
			History.Add(FString::Printf(TEXT("pending_term kind=%s due_phase=%d phase_offset=%d description=%s"),
				*Term.Kind.ToString(), Term.DuePhase, Term.DuePhase - static_cast<int32>(Session.DayPhase), *DescribeTerms({Term})));
	}
	bHasPendingOnlineIntent = true;
	const int64 Generation = ++DialogueGeneration, Revision = StateRevision;
	TWeakObjectPtr<UWindStationStateSubsystem> WeakThis(this);
	AgentGateway->RequestCanonicalIntent(Text, ActionId == TEXT("talk_ye_cheng") ? FName(TEXT("ye_cheng")) : FName(TEXT("gu_heng")),
		History, Session.CommittedTurns,
		[WeakThis, Generation, Revision, SessionId, ActionId, Text, Completion](bool Valid, const FWSCanonicalIntent& Parsed, const FString& Error)
		{
			if (!WeakThis.IsValid() || WeakThis->DialogueGeneration != Generation) return;
			WeakThis->bHasPendingOnlineIntent = false;
			if (!WeakThis->DialogueSessions.Contains(SessionId) || WeakThis->StateRevision != Revision)
			{ Completion(false, {}, TEXT("情况已变化，请重新交谈。")); return; }
			if (!Valid) { Completion(false, {}, TEXT("连接或意图解析失败，本次未扣费。")); return; }
			FWSCanonicalIntent Resolved; FString Status;
			const bool Ready = WeakThis->ResolveParsedOnlineMessage(ActionId, Text, SessionId, Parsed, Resolved, Status, true);
			Completion(Ready, Resolved, Status);
		});
}
