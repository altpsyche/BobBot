// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "BobBotConfig.h"

class FBobBotChatController;

/**
 * Connect tab — FTUE setup checklist, model selector, chat session info, collapsible Advanced section.
 */
class SBobBotConnectTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBobBotConnectTab) {}
		SLATE_ARGUMENT(FBobBotChatController*, Controller)
		SLATE_EVENT(FSimpleDelegate, OnGoToChat)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FBobBotChatController* Controller = nullptr;
	FSimpleDelegate OnGoToChat;

	// -- Claude CLI detection --
	void DetectClaudeCli();

	// -- Setup section --
	FReply HandleRefreshStatus();
	FReply HandleGoToChat();
	FText GetClaudeInstallStatusText() const;
	FSlateColor GetClaudeInstallStatusColor() const;
	FText GetAuthStatusText() const;
	FSlateColor GetAuthStatusColor() const;
	FText GetReadyStatusText() const;
	FSlateColor GetReadyStatusColor() const;
	bool IsReadyToChat() const;

	// -- Model section --
	FReply HandleModelSelected(FString ModelName);
	FSlateColor GetModelButtonColor(FString ModelName) const;

	// -- Chat Session info (reads from Controller) --
	FText GetSessionModelText() const;
	FText GetSessionCostText() const;
	FText GetSessionMessageCountText() const;

	// -- Advanced (collapsible) --
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
	void LoadSystemPromptIntoEditor();

	// CLAUDE.md editor
	TSharedPtr<class SMultiLineEditableTextBox> ClaudeMdEditor;
	FReply HandleSaveClaudeMd();
	FReply HandleLoadClaudeMd();

	// MCP config
	FReply HandleSetupClient(FString ClientName);
	FText GetClientStatusText(FString ClientName) const;
	FSlateColor GetClientStatusColor(FString ClientName) const;
	bool DoesClientConfigExist(const FString& ClientName) const;

	// Paths
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
};
