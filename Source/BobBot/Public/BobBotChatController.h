// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"

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
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnChatMessageAdded, const FBobBotChatMessage&);
DECLARE_MULTICAST_DELEGATE(FOnChatHistoryCleared);
DECLARE_MULTICAST_DELEGATE(FOnApprovalStateChanged);
DECLARE_MULTICAST_DELEGATE(FOnThinkingStateChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMessageUpdated, int32 /*MessageIndex*/);
DECLARE_MULTICAST_DELEGATE(FOnChatListChanged);

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

	// Tool classification (mirrors Python's _classify_tool for UI use)
	static FString ClassifyTool(const FString& ToolName);
	static bool IsToolAutoApproved(const FString& ToolName);
	bool IsServerRunning() const { return bServerRunning; }
	int32 GetConnectedClientCount() const { return ConnectedClientCount; }
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
	TMap<FString, TFunction<void(const FString&)>> SlashCommands;

	// -- Message management --
	void AddMessage(FBobBotChatMessage::ESender Sender, const FString& Content, float Cost = 0.f, int32 DurationMs = 0, int32 NumTurns = 0);

	// -- Polling --
	void PollServerStatus();
	void PollChatUpdates();
	void PollApprovalRequests();

	// -- Persistence --
	void SaveChatHistory();
	void LoadChatHistory();
	FString GetChatHistoryPath() const;

	// -- Multi-chat persistence --
	static FString GetChatsDir();
	static FString GetChatIndexPath();
	void SaveChatIndex();
	void LoadChatIndex();
	void MigrateLegacyHistory();
	void UpdateIndexEntry();

	// -- Multi-chat state --
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

	// Server state
	bool bServerRunning = false;
	int32 ConnectedClientCount = 0;

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
};
