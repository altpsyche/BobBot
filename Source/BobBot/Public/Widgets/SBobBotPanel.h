// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "BobBotConfig.h"

/**
 * Main BobBot editor panel with Connect and Chat tabs.
 * - Connect: FTUE setup checklist, model selector, chat session info, collapsible Advanced
 * - Chat: AI chat powered by Claude Code CLI (uses OAuth subscription)
 */
class SBobBotPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBobBotPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	// -- Tab switching --
	enum class EBobBotTab : uint8 { Connect, Chat };
	EBobBotTab ActiveTab = EBobBotTab::Connect;

	TSharedRef<SWidget> BuildConnectTab();
	TSharedRef<SWidget> BuildChatTab();

	FReply OnTabClicked(EBobBotTab Tab);
	FSlateColor GetTabColor(EBobBotTab Tab) const;

	// -- Connect tab: Setup section --
	FReply HandleRefreshStatus();
	FReply HandleGoToChat();
	FText GetClaudeInstallStatusText() const;
	FSlateColor GetClaudeInstallStatusColor() const;
	FText GetAuthStatusText() const;
	FSlateColor GetAuthStatusColor() const;
	FText GetReadyStatusText() const;
	FSlateColor GetReadyStatusColor() const;
	bool IsReadyToChat() const;

	void DetectClaudeCli();

	// -- Connect tab: Model section --
	FReply HandleModelSelected(FString ModelName);
	FSlateColor GetModelButtonColor(FString ModelName) const;

	// -- Connect tab: Chat Session info --
	FText GetSessionModelText() const;
	FText GetSessionCostText() const;
	FText GetSessionMessageCountText() const;
	int32 SessionMessageCount = 0;

	// -- Connect tab: Advanced (collapsible) --
	bool bAdvancedExpanded = false;
	FReply HandleToggleAdvanced();
	FText GetAdvancedToggleText() const;

	// Permission mode
	FReply HandlePermissionModeChanged(EBobBotPermissionMode Mode);
	ECheckBoxState GetPermissionCheckState(EBobBotPermissionMode Mode) const;

	// System prompt editor
	TSharedPtr<class SMultiLineEditableTextBox> SystemPromptEditor;
	FReply HandleSaveSystemPrompt();
	FReply HandleResetSystemPrompt();

	// CLAUDE.md editor
	TSharedPtr<class SMultiLineEditableTextBox> ClaudeMdEditor;
	FReply HandleSaveClaudeMd();
	FReply HandleLoadClaudeMd();

	// Other editors (MCP config)
	FReply HandleSetupClient(FString ClientName);
	FText GetClientStatusText(FString ClientName) const;
	FSlateColor GetClientStatusColor(FString ClientName) const;
	bool DoesClientConfigExist(const FString& ClientName) const;

	// Paths (read-only, debug)
	FText GetProjectRootPath() const;
	FText GetMcpJsonPath() const;
	FText GetBridgePath() const;
	FText GetPromptFilePath() const;

	// Prerequisites
	FText GetPythonPrereqText() const;
	FText GetUvPrereqText() const;
	FText GetPluginPrereqText() const;
	FSlateColor GetPythonPrereqColor() const;
	FSlateColor GetUvPrereqColor() const;
	FSlateColor GetPluginPrereqColor() const;

	// Advanced section widget (shown/hidden)
	TSharedPtr<class SBox> AdvancedSection;

	// -- Chat tab --
	struct FChatMessage
	{
		enum class ESender : uint8 { User, Bot, System, Error, Approval };
		ESender Sender;
		FString Content;
		FDateTime Timestamp;
		float Cost = 0.f;
		int32 DurationMs = 0;
		int32 NumTurns = 0;
	};

	TArray<FChatMessage> ChatHistory;
	TSharedPtr<class SScrollBox> ChatScrollBox;
	TSharedPtr<class SMultiLineEditableTextBox> CommandInput;
	TSharedPtr<class SVerticalBox> ChatMessagesBox;
	TSharedPtr<class SButton> SendButton;
	TSharedPtr<class SButton> StopButton;

	// Chat header bar
	FText GetChatHeaderText() const;
	float TotalSessionCost = 0.f;

	void AddChatMessage(FChatMessage::ESender Sender, const FString& Content, float Cost = 0.f, int32 DurationMs = 0, int32 NumTurns = 0);
	void RebuildChatMessages();
	static TSharedRef<SWidget> BuildChatMessageWidget(const FChatMessage& Msg);
	FReply OnSendClicked();
	FReply OnClearChatClicked();
	FReply OnStopClicked();

	// Thinking indicator — shown as a real message in chat
	void UpdateThinkingIndicator();
	int32 ThinkingDotCount = 0;
	float ThinkingAnimTimer = 0.f;
	bool bWasThinking = false;
	TSharedPtr<class STextBlock> ThinkingTextBlock;

	// Send button state
	bool IsSendEnabled() const;
	bool IsStopVisible() const;

	// -- Polling --
	float StatusPollTimer = 0.f;
	float ChatPollTimer = 0.f;
	bool bServerRunning = false;
	int32 ConnectedClientCount = 0;
	bool bAiThinking = false;

	void PollServerStatus();
	void PollChatUpdates();

	// -- Tool approval ("Ask Me" mode) --
	bool bHasPendingApproval = false;
	int32 PendingApprovalId = 0;
	FString PendingApprovalTool;
	FString PendingApprovalCode;

	void PollApprovalRequests();
	FReply OnApproveClicked();
	FReply OnDenyClicked();

	// -- Tab content container --
	TSharedPtr<class SWidgetSwitcher> TabSwitcher;
};
