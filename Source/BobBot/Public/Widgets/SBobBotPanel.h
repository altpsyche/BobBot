// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * Main BobBot editor panel with Connect and Chat tabs.
 * - Connect: FTUE, Claude Code status, model selector, MCP client configs, prerequisites
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

	// -- Connect tab --
	FReply HandleCopyConfig(FString ClientName);
	FReply HandleWriteConfig(FString ClientName);
	FReply HandleModelSelected(FString ModelName);
	FText GetServerStatusText() const;
	FSlateColor GetServerStatusColor() const;
	FText GetClaudeStatusText() const;
	FSlateColor GetClaudeStatusColor() const;
	FText GetConnectedClientsText() const;
	FText GetPythonPrereqText() const;
	FText GetUvPrereqText() const;
	FText GetPluginPrereqText() const;
	FSlateColor GetPythonPrereqColor() const;
	FSlateColor GetUvPrereqColor() const;
	FSlateColor GetPluginPrereqColor() const;

	void DetectClaudeCli();

	// -- Chat tab --
	struct FChatMessage
	{
		enum class ESender : uint8 { User, Bot, System };
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

	void AddChatMessage(FChatMessage::ESender Sender, const FString& Content, float Cost = 0.f, int32 DurationMs = 0, int32 NumTurns = 0);
	void RebuildChatMessages();
	FReply OnSendClicked();
	FReply OnClearChatClicked();

	// -- Polling --
	float StatusPollTimer = 0.f;
	float ChatPollTimer = 0.f;
	bool bServerRunning = false;
	int32 ConnectedClientCount = 0;
	bool bAiThinking = false;

	void PollServerStatus();
	void PollChatUpdates();

	// -- Tab content container --
	TSharedPtr<class SWidgetSwitcher> TabSwitcher;
};
