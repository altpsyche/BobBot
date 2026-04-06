// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"
#include "BobBotStatusMonitors.h"

/**
 * Chat message — top-level struct used by chat controller and UI.
 * Matches the persistence format exactly (ESender values 0-4).
 */
struct FBobBotChatMessage
{
	enum class ESender : uint8 { User, Bot, System, Error, Approval, ToolCall, Subagent };
	ESender Sender;
	FString Content;
	FDateTime Timestamp;
	float Cost = 0.f;
	int32 DurationMs = 0;
	int32 NumTurns = 0;

	// Tool call fields (only for ESender::ToolCall)
	FString ToolName;
	FString ToolInput;
	bool bToolComplete = false;

	// Subagent fields (only for ESender::Subagent)
	FString SubagentTaskId;
	FString SubagentDescription;
	FString SubagentStatus;      // "running", "completed", "failed", "stopped"
	FString SubagentSummary;
	int32 SubagentTokens = 0;
	int32 SubagentToolUses = 0;
	int32 SubagentDurationMs = 0;
	TArray<FBobBotChatMessage> SubagentToolCalls;  // Tool calls nested under this subagent
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnChatMessageAdded, const FBobBotChatMessage&);
DECLARE_MULTICAST_DELEGATE(FOnChatHistoryCleared);
DECLARE_MULTICAST_DELEGATE(FOnApprovalStateChanged);
DECLARE_MULTICAST_DELEGATE(FOnThinkingStateChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMessageUpdated, int32 /*MessageIndex*/);
DECLARE_MULTICAST_DELEGATE(FOnChatListChanged);

/**
 * One slash command. Single registration owns the name, the user-visible
 * description (shown in /help and the Info tab), the optional `[args]`
 * usage hint, and the handler. The Info tab and /help both read from this
 * list so the docs can never go stale relative to what's registered.
 */
struct FBobBotSlashCommand
{
	FString Name;          // "/foo"
	FString UsageSuffix;   // " [arg]" — appended to Name in display, empty for no args
	FString Description;   // One-line user-visible help text
	TFunction<void(const FString&)> Handler;
};

/** Entry in the chat index — lightweight metadata for the switcher dropdown. */
struct FBobBotChatEntry
{
	FString Id;       // UUID
	FString Title;    // First 40 chars of first user message
	FString Model;
	float Cost = 0.f;
	FDateTime Updated;
	FString ParentId;     // Empty for root chats, UUID of parent for forks
	FString BranchName;   // Display name for the branch
};

/**
 * Non-Slate chat business logic controller.
 * Owns all chat state: history, polling, slash commands, approval flow, thinking animation.
 * Widgets subscribe to multicast delegates to react to state changes.
 */
class FBobBotChatController
{
public:
	FBobBotChatController();
	~FBobBotChatController();

	// -- Public API --
	void SendMessage(const FString& Message);
	void ClearSession();
	void StopChat();
	void UndoLastMessage();
	void StopSubagentTask(const FString& TaskId);
	void ApproveExecution();
	void DenyExecution();
	void Tick(float DeltaTime);

	/** Add a system or error message from outside the controller (e.g. connect tab notifications). */
	void AddExternalMessage(FBobBotChatMessage::ESender Sender, const FString& Content);

	// -- Multi-chat --
	void NewChat();
	void SwitchChat(const FString& ChatId);
	void DeleteChat(const FString& ChatId);
	void ForkChat();
	bool CanFork() const;
	const TArray<FBobBotChatEntry>& GetChatIndex() const { return ChatIndex; }
	const FString& GetActiveChatId() const { return ActiveChatId; }
	void RefreshChatIndex();
	FString GetActiveChatTitle() const;

	// -- Const getters --
	const TArray<FBobBotChatMessage>& GetHistory() const { return ChatHistory; }
	bool IsThinking() const { return bAiThinking; }
	float GetSessionCost() const { return TotalSessionCost; }
	int32 GetMessageCount() const { return SessionMessageCount; }
	bool HasPendingApproval() const { return bHasPendingApproval; }
	const FString& GetPendingApprovalTool() const { return PendingApprovalTool; }
	const FString& GetPendingApprovalCode() const { return PendingApprovalCode; }
	const FString& GetPendingApprovalCategory() const { return PendingApprovalCategory; }

	// Context usage
	int32 GetContextTokensUsed() const { return ContextTokensUsed; }
	int32 GetContextTokensMax() const { return ContextTokensMax; }
	float GetContextPercent() const { return ContextTokensMax > 0 ? (float)ContextTokensUsed / ContextTokensMax * 100.f : 0.f; }

	/** Read-only view of registered slash commands, in registration order.
	 *  The Info tab and /help both render from this. */
	const TArray<FBobBotSlashCommand>& GetSlashCommands() const { return SlashCommands; }

	// Tool classification (mirrors Python's _classify_tool for UI use)
	static FString ClassifyTool(const FString& ToolName);
	static bool IsToolAutoApproved(const FString& ToolName);
	bool IsServerRunning() const { return ServerMonitor.IsRunning(); }
	int32 GetConnectedClientCount() const { return ServerMonitor.GetClientCount(); }
	int32 GetThinkingDotCount() const { return ThinkingDotCount; }

	// -- Delegates --
	FOnChatMessageAdded OnMessageAdded;
	FOnChatHistoryCleared OnHistoryCleared;
	FOnApprovalStateChanged OnApprovalStateChanged;
	FOnThinkingStateChanged OnThinkingStateChanged;
	FOnMessageUpdated OnMessageUpdated;
	FOnChatListChanged OnChatListChanged;

private:
	// -- Slash command handling --
	void HandleSlashCommand(const FString& Message);
	void RegisterSlashCommand(const FString& Name, const FString& UsageSuffix,
	                          const FString& Description, TFunction<void(const FString&)> Handler);
	const FBobBotSlashCommand* FindSlashCommand(const FString& Name) const;
	bool IsSlashCommandPrefix(const FString& LowerCmd) const;
	TArray<FBobBotSlashCommand> SlashCommands;

	// -- Message management --
	void AddMessage(FBobBotChatMessage::ESender Sender, const FString& Content, float Cost = 0.f, int32 DurationMs = 0, int32 NumTurns = 0);

	// -- Polling --
	void PollServerStatus();
	void PollChatUpdates();

	// -- Stream event handlers (called from PollChatUpdates) --
	void HandleStreamEvent_Text(const TSharedPtr<FJsonObject>& Evt);
	void HandleStreamEvent_ToolUse(const TSharedPtr<FJsonObject>& Evt);
	void HandleStreamEvent_ToolResult(const TSharedPtr<FJsonObject>& Evt);
	void HandleStreamEvent_Complete(const TSharedPtr<FJsonObject>& Evt);
	void HandleStreamEvent_Error(const TSharedPtr<FJsonObject>& Evt);
	void HandleStreamEvent_SubagentStart(const TSharedPtr<FJsonObject>& Evt);
	void HandleStreamEvent_SubagentProgress(const TSharedPtr<FJsonObject>& Evt);
	void HandleStreamEvent_SubagentComplete(const TSharedPtr<FJsonObject>& Evt);
	void HandleStreamEvent_Approval(const TSharedPtr<FJsonObject>& Evt);
	void HandleStreamEvent_Notification(const TSharedPtr<FJsonObject>& Evt);
	void HandleStreamEvent_HookToolError(const TSharedPtr<FJsonObject>& Evt);

	/** Find the ChatHistory index of the most recently started subagent message. Returns INDEX_NONE if no active subagents. */
	int32 FindMostRecentSubagentIndex() const;

	// -- Session state --
	void LoadSessionMessages(const FString& SessionId);
	FString ActiveChatId;
	TArray<FBobBotChatEntry> ChatIndex;

	// -- Cleanup --
	void KillChatProcess();

	// -- State --
	TArray<FBobBotChatMessage> ChatHistory;
	int32 SessionMessageCount = 0;
	float TotalSessionCost = 0.f;

	// Thinking animation
	bool bAiThinking = false;
	int32 ThinkingDotCount = 0;
	float ThinkingAnimTimer = 0.f;
	bool bWasThinking = false;

	// Poll timers
	float StatusPollTimer = 0.f;
	float ChatPollTimer = 0.f;

	// Status monitors
	FBobBotServerMonitor ServerMonitor;
	FBobBotBridgeMonitor BridgeMonitor;
	FBobBotSdkMonitor SdkMonitor;

	// Tool approval
	bool bHasPendingApproval = false;
	int32 PendingApprovalId = 0;
	FString PendingApprovalTool;
	FString PendingApprovalCode;
	FString PendingApprovalCategory;

	// Subagent tracking (task_id -> ChatHistory index)
	TMap<FString, int32> ActiveSubagentMessageIndex;

	// Context usage (updated per query from SDK)
	int32 ContextTokensUsed = 0;
	int32 ContextTokensMax = 0;

	// Streaming text — track the index of the currently streaming bot message
	int32 StreamingBotMessageIndex = INDEX_NONE;
};
