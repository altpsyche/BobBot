// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "BobBotConfig.h"

class FBobBotChatController;
class SEditableTextBox;

/**
 * Connect tab — FTUE setup checklist, model selector, chat session info, collapsible Advanced section.
 */
class SBobBotConnectTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBobBotConnectTab) {}
		SLATE_ARGUMENT(FBobBotChatController*, Controller)
		SLATE_EVENT(FSimpleDelegate, OnGoToChat)
		SLATE_EVENT(FSimpleDelegate, OnFactoryReset)
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
	FText GetReadyStatusText() const;
	FSlateColor GetReadyStatusColor() const;
	bool IsReadyToChat() const;

	// -- Backend status row --
	FText GetBackendStatusText() const;
	FSlateColor GetBackendStatusColor() const;

	// -- Authentication section --
	FReply HandleAuthModeChanged(EBobBotAuthMode Mode);
	ECheckBoxState GetAuthModeCheckState(EBobBotAuthMode Mode) const;
	FText GetAuthStatusText() const;
	FSlateColor GetAuthStatusColor() const;
	FReply HandleSaveApiKey();
	void OnApiKeyTextCommitted(const FText& Text, ETextCommit::Type CommitType);
	void OnApiProviderChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	EVisibility GetApiKeyFieldsVisibility() const;
	EVisibility GetApiRegionVisibility() const;
	EVisibility GetApiProjectIdVisibility() const;
	FText GetApiKeyStatusText() const;
	FSlateColor GetApiKeyStatusColor() const;
	TSharedPtr<SEditableTextBox> ApiKeyInput;
	TArray<TSharedPtr<FString>> ApiProviderOptions;

	// -- HTTP Bridge section --
	FText GetBridgeStatusText() const;
	FSlateColor GetBridgeStatusColor() const;
	FText GetBridgeHealthText() const;
	FSlateColor GetBridgeHealthColor() const;
	FReply HandleRestartBridge();

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

	// SDK toggle in Advanced
	void OnSDKToggleChanged(ECheckBoxState State);
	ECheckBoxState GetSDKToggleState() const;
	FReply HandleShowSDKInfoDialog();

	// Permission mode
	FReply HandlePermissionModeChanged(EBobBotPermissionMode Mode);
	ECheckBoxState GetPermissionCheckState(EBobBotPermissionMode Mode) const;

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
	FText GetPluginPrereqText() const;
	FText GetAgentSDKPrereqText() const;
	FSlateColor GetPluginPrereqColor() const;
	FSlateColor GetAgentSDKPrereqColor() const;

	// Troubleshooting
	FReply HandleDiagHealthCheck();
	FReply HandleDiagViewBridgeLog();
	FReply HandleDiagRebuildVenv();
	FReply HandleDiagReinstallPackages();
	FReply HandleDiagRegenMcpConfig();
	FReply HandleDiagClearTempFiles();
	FReply HandleDiagKillPortConflicts();
	FReply HandleFactoryReset();
	FSimpleDelegate OnFactoryReset;

	// Advanced section widget (shown/hidden)
	TSharedPtr<class SBox> AdvancedSection;
};
