// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "Widgets/SBobBotConnectTab.h"
#include "BobBot.h"
#include "BobBotChatController.h"
#include "BobBotConfig.h"
#include "BobBotConstants.h"
#include "BobBotRuntimeStatus.h"
#include "BobBotPythonBridge.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"

#define LOCTEXT_NAMESPACE "SBobBotConnectTab"

/** Typedef to avoid angle-bracket issues inside Slate macros. */
using SSpinBox_Int32 = SSpinBox<int32>;

// =========================================================================== //
// Construction
// =========================================================================== //

void SBobBotConnectTab::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;
	OnGoToChat = InArgs._OnGoToChat;
	OnFactoryReset = InArgs._OnFactoryReset;

	DetectClaudeCli();

	const FBobBotConfig& Config = FBobBotConfig::Get();

	// -- Build a model button with subtitle --
	auto MakeModelButton = [this](const FText& Name, const FText& Subtitle, const FString& ModelKey) -> TSharedRef<SWidget>
	{
		FString Key = ModelKey;
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SButton).Text(Name)
				.OnClicked(FOnClicked::CreateSP(this, &SBobBotConnectTab::HandleModelSelected, Key))
				.ButtonColorAndOpacity(this, &SBobBotConnectTab::GetModelButtonColor, Key)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(Subtitle)
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
				.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
			];
	};

	// -- Build a client row for "Use in Other Editors" --
	auto MakeEditorRow = [this](const FText& DisplayName, const FString& ClientKey) -> TSharedRef<SWidget>
	{
		FString Key = ClientKey;
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center)
			[ SNew(STextBlock).Text(DisplayName).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)) ]
			+ SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SBobBotConnectTab::GetClientStatusText, Key)
				.ColorAndOpacity(this, &SBobBotConnectTab::GetClientStatusColor, Key)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(LOCTEXT("SetupBtn", "Setup"))
			.ToolTipText(LOCTEXT("SetupTip", "Write MCP server config so this editor can connect to BobBot"))
				.OnClicked(FOnClicked::CreateSP(this, &SBobBotConnectTab::HandleSetupClient, Key))
			];
	};

	// -- API provider dropdown options --
	ApiProviderOptions.Add(MakeShared<FString>(TEXT("anthropic")));
	ApiProviderOptions.Add(MakeShared<FString>(TEXT("bedrock")));
	ApiProviderOptions.Add(MakeShared<FString>(TEXT("vertex")));

	// -- Advanced section content --
	TSharedRef<SVerticalBox> AdvancedContent = SNew(SVerticalBox)

		// TOOL PERMISSIONS (Claude Code style)
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("ToolPerms", "TOOL PERMISSIONS")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "RadioButton")
			.IsChecked(this, &SBobBotConnectTab::GetPermissionCheckState, EBobBotPermissionMode::EditAutomatically)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState) { HandlePermissionModeChanged(EBobBotPermissionMode::EditAutomatically); })
			[
				SNew(STextBlock).Text(LOCTEXT("EditAuto", "Edit Automatically \x2014 Claude does everything without asking"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "RadioButton")
			.IsChecked(this, &SBobBotConnectTab::GetPermissionCheckState, EBobBotPermissionMode::AskBeforeEdits)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState) { HandlePermissionModeChanged(EBobBotPermissionMode::AskBeforeEdits); })
			[
				SNew(STextBlock).Text(LOCTEXT("AskEdits", "Ask Before Edits \x2014 Allows reads, asks before writes and creates"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "RadioButton")
			.IsChecked(this, &SBobBotConnectTab::GetPermissionCheckState, EBobBotPermissionMode::Plan)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState) { HandlePermissionModeChanged(EBobBotPermissionMode::Plan); })
			[
				SNew(STextBlock).Text(LOCTEXT("PlanMode", "Plan \x2014 Read-only, Claude suggests but doesn't execute"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]
		]

		// AUTO-APPROVE CATEGORIES (visible only in Ask Before Edits mode)
		+ SVerticalBox::Slot().AutoHeight().Padding(32, 8, 8, 2)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return FBobBotConfig::Get().PermissionMode == EBobBotPermissionMode::AskBeforeEdits ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
				[
					SNew(STextBlock).Text(LOCTEXT("AutoApproveLabel", "Auto-approve tool categories:"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
					.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([]() { return FBobBotConfig::Get().bAutoApproveReadOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([](ECheckBoxState S) { FBobBotConfig::Get().bAutoApproveReadOnly = (S == ECheckBoxState::Checked); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
					[ SNew(STextBlock).Text(LOCTEXT("ApproveReadOnly", "Read-only (get_*, search_*, is_*, list_*)")).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9)) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([]() { return FBobBotConfig::Get().bAutoApproveViewport ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([](ECheckBoxState S) { FBobBotConfig::Get().bAutoApproveViewport = (S == ECheckBoxState::Checked); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
					[ SNew(STextBlock).Text(LOCTEXT("ApproveViewport", "Viewport (capture, camera)")).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9)) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([]() { return FBobBotConfig::Get().bAutoApproveCreate ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([](ECheckBoxState S) { FBobBotConfig::Get().bAutoApproveCreate = (S == ECheckBoxState::Checked); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
					[ SNew(STextBlock).Text(LOCTEXT("ApproveCreate", "Create (spawn_*, create_*, add_*)")).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9)) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([]() { return FBobBotConfig::Get().bAutoApproveModify ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([](ECheckBoxState S) { FBobBotConfig::Get().bAutoApproveModify = (S == ECheckBoxState::Checked); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
					[ SNew(STextBlock).Text(LOCTEXT("ApproveModify", "Modify (set_*, delete_*, remove_*)")).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9)) ]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(8, 2)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([]() { return FBobBotConfig::Get().bAutoApproveCodeExec ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([](ECheckBoxState S) { FBobBotConfig::Get().bAutoApproveCodeExec = (S == ECheckBoxState::Checked); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
					[ SNew(STextBlock).Text(LOCTEXT("ApproveCodeExec", "Code execution (execute_unreal_python)")).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9)) ]
				]
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8) [ SNew(SSeparator) ]

		// BACKEND MODE
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("BackendMode", "BACKEND MODE")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BackendSDK", "Backend: Agent SDK (persistent process)"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.ColorAndOpacity(FSlateColor(BobBot::Colors::BotGreen))
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8) [ SNew(SSeparator) ]

		// COST BUDGET
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("CostBudget", "COST BUDGET")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("BudgetLabel", "Per-message limit:")) ]
			+ SHorizontalBox::Slot().FillWidth(0.4f)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.f).MaxValue(100.f)
				.Delta(0.50f)
				.Value(Config.MaxBudgetUsd)
				.OnValueCommitted_Lambda([](float Val, ETextCommit::Type) { FBobBotConfig::Get().MaxBudgetUsd = Val; FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
			[ SNew(STextBlock).Text(LOCTEXT("USD", "USD")).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(32, 2, 16, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BudgetHint", "Set to 0 for unlimited."))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8) [ SNew(SSeparator) ]

		// EXTENDED THINKING
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("ThinkingSection", "EXTENDED THINKING")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("ThinkingLabel", "Mode:")).ToolTipText(LOCTEXT("ThinkingTip", "Extended thinking lets Claude reason through complex problems step by step before responding")) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked_Lambda([]() { return FBobBotConfig::Get().ThinkingMode == TEXT("disabled") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([](ECheckBoxState) { FBobBotConfig::Get().ThinkingMode = TEXT("disabled"); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
				[ SNew(STextBlock).Text(LOCTEXT("ThinkingOff", "Off")).Margin(FMargin(4, 2)) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked_Lambda([]() { return FBobBotConfig::Get().ThinkingMode == TEXT("enabled") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([](ECheckBoxState) { FBobBotConfig::Get().ThinkingMode = TEXT("enabled"); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
				[ SNew(STextBlock).Text(LOCTEXT("ThinkingOn", "Enabled")).Margin(FMargin(4, 2)) ]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked_Lambda([]() { return FBobBotConfig::Get().ThinkingMode == TEXT("adaptive") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([](ECheckBoxState) { FBobBotConfig::Get().ThinkingMode = TEXT("adaptive"); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
				[ SNew(STextBlock).Text(LOCTEXT("ThinkingAdaptive", "Adaptive")).Margin(FMargin(4, 2)) ]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([]() { return FBobBotConfig::Get().ThinkingMode == TEXT("enabled") ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("ThinkingBudgetLabel", "Token budget:")).ToolTipText(LOCTEXT("ThinkingBudgetTip", "Maximum tokens for thinking (1000-100000)")) ]
			+ SHorizontalBox::Slot().FillWidth(0.6f)
			[
				SNew(SSpinBox_Int32)
				.MinValue(1000).MaxValue(100000)
				.Delta(1000)
				.Value_Lambda([]() { return FBobBotConfig::Get().ThinkingBudget; })
				.OnValueCommitted_Lambda([](int32 Val, ETextCommit::Type) { FBobBotConfig::Get().ThinkingBudget = Val; FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4) [ SNew(SSeparator) ]

		// EFFORT LEVEL
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("EffortSection", "EFFORT LEVEL")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("EffortLabel", "Level:")).ToolTipText(LOCTEXT("EffortTip", "Controls how thoroughly Claude analyzes your request. Lower = faster but less detailed")) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked_Lambda([]() { return FBobBotConfig::Get().EffortLevel == TEXT("low") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([](ECheckBoxState) { FBobBotConfig::Get().EffortLevel = TEXT("low"); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
				[ SNew(STextBlock).Text(LOCTEXT("EffortLow", "Low")).Margin(FMargin(4, 2)) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked_Lambda([]() { return FBobBotConfig::Get().EffortLevel == TEXT("medium") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([](ECheckBoxState) { FBobBotConfig::Get().EffortLevel = TEXT("medium"); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
				[ SNew(STextBlock).Text(LOCTEXT("EffortMed", "Medium")).Margin(FMargin(4, 2)) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked_Lambda([]() { return FBobBotConfig::Get().EffortLevel == TEXT("high") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([](ECheckBoxState) { FBobBotConfig::Get().EffortLevel = TEXT("high"); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
				[ SNew(STextBlock).Text(LOCTEXT("EffortHigh", "High")).Margin(FMargin(4, 2)) ]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked_Lambda([]() { return FBobBotConfig::Get().EffortLevel == TEXT("max") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([](ECheckBoxState) { FBobBotConfig::Get().EffortLevel = TEXT("max"); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
				[ SNew(STextBlock).Text(LOCTEXT("EffortMax", "Max")).Margin(FMargin(4, 2)) ]
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8) [ SNew(SSeparator) ]

		// BRIDGE (Advanced settings)
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("BridgeAdvSection", "BRIDGE")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("BridgePortLabel", "Port:")).ToolTipText(LOCTEXT("BridgePortTip", "TCP port for the HTTP MCP bridge (default 13580)")) ]
			+ SHorizontalBox::Slot().FillWidth(0.6f)
			[
				SNew(SSpinBox_Int32)
				.MinValue(1).MaxValue(65535)
				.Value(Config.BridgePort)
				.OnValueCommitted_Lambda([](int32 Val, ETextCommit::Type) { FBobBotConfig::Get().BridgePort = Val; FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("BridgeAutoStart", "Auto-start:")).ToolTipText(LOCTEXT("BridgeAutoStartTip", "Launch the HTTP bridge when the editor starts")) ]
			+ SHorizontalBox::Slot().FillWidth(0.6f)
			[
				SNew(SCheckBox)
				.IsChecked(Config.bAutoStartBridge ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged_Lambda([](ECheckBoxState State) { FBobBotConfig::Get().bAutoStartBridge = (State == ECheckBoxState::Checked); FBobBotConfig::Get().Save(); })
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8) [ SNew(SSeparator) ]

		// SERVER
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("ServerSection", "SERVER")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("PortLabel", "Port:")).ToolTipText(LOCTEXT("PortTip", "TCP port for the MCP server (default 13579)")) ]
			+ SHorizontalBox::Slot().FillWidth(0.6f)
			[
				SNew(SSpinBox_Int32)
				.MinValue(1).MaxValue(65535)
				.Value(Config.Port)
				.OnValueCommitted_Lambda([](int32 Val, ETextCommit::Type) { FBobBotConfig::Get().Port = Val; FBobBotConfig::Get().Save(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("AutoStart", "Auto-start:")).ToolTipText(LOCTEXT("AutoStartTip", "Start the MCP server when the editor launches")) ]
			+ SHorizontalBox::Slot().FillWidth(0.6f)
			[
				SNew(SCheckBox)
				.IsChecked(Config.bAutoStartServer ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged_Lambda([](ECheckBoxState State) { FBobBotConfig::Get().bAutoStartServer = (State == ECheckBoxState::Checked); FBobBotConfig::Get().Save(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("AutoGen", "Auto-generate .mcp.json:")).ToolTipText(LOCTEXT("AutoGenTip", "Write .mcp.json at project root on startup so Claude Code can find the MCP server")) ]
			+ SHorizontalBox::Slot().FillWidth(0.6f)
			[
				SNew(SCheckBox)
				.IsChecked(Config.bAutoGenerateMcpJson ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged_Lambda([](ECheckBoxState State) { FBobBotConfig::Get().bAutoGenerateMcpJson = (State == ECheckBoxState::Checked); FBobBotConfig::Get().Save(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("MaxClients", "Max Clients:")).ToolTipText(LOCTEXT("MaxClientsTip", "Maximum concurrent MCP connections (editors, scripts)")) ]
			+ SHorizontalBox::Slot().FillWidth(0.6f)
			[
				SNew(SSpinBox_Int32)
				.MinValue(1).MaxValue(16)
				.Value(Config.MaxClients)
				.OnValueCommitted_Lambda([](int32 Val, ETextCommit::Type) { FBobBotConfig::Get().MaxClients = Val; FBobBotConfig::Get().Save(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("RateLimit", "Rate Limit:")).ToolTipText(LOCTEXT("RateLimitTip", "Maximum messages per second per client (prevents runaway tool loops)")) ]
			+ SHorizontalBox::Slot().FillWidth(0.6f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SSpinBox_Int32)
					.MinValue(1).MaxValue(1000)
					.Value(Config.RateLimitPerSecond)
					.OnValueCommitted_Lambda([](int32 Val, ETextCommit::Type) { FBobBotConfig::Get().RateLimitPerSecond = Val; FBobBotConfig::Get().Save(); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[ SNew(STextBlock).Text(LOCTEXT("MsgPerSec", "msg/s")).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)) ]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("ChatTimeout", "Chat Timeout:")).ToolTipText(LOCTEXT("ChatTimeoutTip", "Kill the Claude subprocess if no response after this many seconds")) ]
			+ SHorizontalBox::Slot().FillWidth(0.6f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SSpinBox_Int32)
					.MinValue(10).MaxValue(3600)
					.Value(Config.ChatTimeoutSeconds)
					.OnValueCommitted_Lambda([](int32 Val, ETextCommit::Type) { FBobBotConfig::Get().ChatTimeoutSeconds = Val; FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[ SNew(STextBlock).Text(LOCTEXT("Seconds", "seconds")).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)) ]
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8) [ SNew(SSeparator) ]

		// USE IN OTHER EDITORS
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("OtherEditors", "USE IN OTHER EDITORS")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2) [ MakeEditorRow(LOCTEXT("ClaudeCodeLabel", "Claude Code"), TEXT("claude")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2) [ MakeEditorRow(LOCTEXT("CursorLabel", "Cursor"), TEXT("cursor")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2) [ MakeEditorRow(LOCTEXT("VSCodeLabel", "VS Code"), TEXT("vscode")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2) [ MakeEditorRow(LOCTEXT("WindsurfLabel", "Windsurf"), TEXT("windsurf")) ]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8) [ SNew(SSeparator) ]

		// PATHS (read-only, for troubleshooting)
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("PathsSection", "PATHS")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[ BobBot::UI::KeyValueRow(LOCTEXT("ProjRootKey", "Project Root:"), TAttribute<FText>::CreateSP(this, &SBobBotConnectTab::GetProjectRootPath)) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[ BobBot::UI::KeyValueRow(LOCTEXT("McpJsonKey", ".mcp.json:"), TAttribute<FText>::CreateSP(this, &SBobBotConnectTab::GetMcpJsonPath)) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[ BobBot::UI::KeyValueRow(LOCTEXT("BridgeKey", "Bridge:"), TAttribute<FText>::CreateSP(this, &SBobBotConnectTab::GetBridgePath)) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[ BobBot::UI::KeyValueRow(LOCTEXT("PromptKey", "Prompt File:"), TAttribute<FText>::CreateSP(this, &SBobBotConnectTab::GetPromptFilePath)) ]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8) [ SNew(SSeparator) ]

		// PREREQUISITES
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("PrereqSection", "PREREQUISITES")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(STextBlock).Text(this, &SBobBotConnectTab::GetPluginPrereqText)
			.ColorAndOpacity(this, &SBobBotConnectTab::GetPluginPrereqColor)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2, 8, 8)
		[
			SNew(STextBlock).Text(this, &SBobBotConnectTab::GetAgentSDKPrereqText)
			.ColorAndOpacity(this, &SBobBotConnectTab::GetAgentSDKPrereqColor)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8) [ SNew(SSeparator) ]

		// TROUBLESHOOTING
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("TroubleSection", "TROUBLESHOOTING")) ]

		// Diagnostics
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 4, 8, 0)
		[ SNew(STextBlock).Text(LOCTEXT("DiagGroup", "Diagnostics")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 9)).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[ SNew(SButton).Text(LOCTEXT("RunHealth", "Run Health Check")).ToolTipText(LOCTEXT("RunHealthTip", "Check venv, bridge, Claude CLI, auth, SDK, MCP config")).OnClicked(this, &SBobBotConnectTab::HandleDiagHealthCheck) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ SNew(SButton).Text(LOCTEXT("ViewLog", "View Bridge Log")).ToolTipText(LOCTEXT("ViewLogTip", "Show the HTTP bridge log file")).OnClicked(this, &SBobBotConnectTab::HandleDiagViewBridgeLog) ]
		]

		// Repair
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 8, 8, 0)
		[ SNew(STextBlock).Text(LOCTEXT("RepairGroup", "Repair")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 9)).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[ SNew(SButton).Text(LOCTEXT("RebuildVenv", "Rebuild Venv")).ToolTipText(LOCTEXT("RebuildTip", "Delete and recreate the Python environment, reinstall all packages")).OnClicked(this, &SBobBotConnectTab::HandleDiagRebuildVenv) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[ SNew(SButton).Text(LOCTEXT("ReinstallPkgs", "Reinstall Packages")).ToolTipText(LOCTEXT("ReinstallTip", "Reinstall mcp and SDK without rebuilding venv")).OnClicked(this, &SBobBotConnectTab::HandleDiagReinstallPackages) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ SNew(SButton).Text(LOCTEXT("RegenMcp", "Regen MCP Config")).ToolTipText(LOCTEXT("RegenTip", "Regenerate .mcp.json and internal MCP config")).OnClicked(this, &SBobBotConnectTab::HandleDiagRegenMcpConfig) ]
		]

		// Cleanup
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 8, 8, 0)
		[ SNew(STextBlock).Text(LOCTEXT("CleanupGroup", "Cleanup")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 9)).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[ SNew(SButton).Text(LOCTEXT("ClearTemp", "Clear Temp Files")).ToolTipText(LOCTEXT("ClearTempTip", "Remove stale temp files from Saved/BobBot/")).OnClicked(this, &SBobBotConnectTab::HandleDiagClearTempFiles) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ SNew(SButton).Text(LOCTEXT("KillPorts", "Kill Port Conflicts")).ToolTipText(LOCTEXT("KillPortsTip", "Kill processes on bridge/MCP ports, restart bridge")).OnClicked(this, &SBobBotConnectTab::HandleDiagKillPortConflicts) ]
		]

		// Nuclear Option
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 12, 8, 0)
		[ SNew(STextBlock).Text(LOCTEXT("NuclearGroup", "Nuclear Option")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 9)).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2, 8, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ResetDesc", "Removes the Python environment, config, and temp files. Re-runs first-time setup."))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 4, 8, 8)
		[ SNew(SButton).Text(LOCTEXT("FactoryReset", "Factory Reset")).OnClicked(this, &SBobBotConnectTab::HandleFactoryReset) ];

	// -- Main layout --
	ChildSlot
	[
		SNew(SScrollBox)

		// ---- SETUP ----
		+ SScrollBox::Slot().Padding(8, 8, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("SetupSection", "SETUP")) ]

		+ SScrollBox::Slot().Padding(16, 4)
		[ BobBot::UI::KeyValueRow(LOCTEXT("ClaudeCodeKey", "Claude Code"),
			TAttribute<FText>::CreateSP(this, &SBobBotConnectTab::GetClaudeInstallStatusText),
			TAttribute<FSlateColor>::CreateSP(this, &SBobBotConnectTab::GetClaudeInstallStatusColor)) ]

		+ SScrollBox::Slot().Padding(16, 4)
		[ BobBot::UI::KeyValueRow(LOCTEXT("BackendKey", "Backend"),
			TAttribute<FText>::CreateSP(this, &SBobBotConnectTab::GetBackendStatusText),
			TAttribute<FSlateColor>::CreateSP(this, &SBobBotConnectTab::GetBackendStatusColor)) ]

		+ SScrollBox::Slot().Padding(16, 6)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("StatusKey", "Status"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
					.MinDesiredWidth(110.f)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock)
					.Text(this, &SBobBotConnectTab::GetReadyStatusText)
					.ColorAndOpacity(this, &SBobBotConnectTab::GetReadyStatusColor)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
			[
				SNew(SButton).Text(LOCTEXT("Refresh", "Refresh"))
				.OnClicked(this, &SBobBotConnectTab::HandleRefreshStatus)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
			[
				SNew(SButton).Text(LOCTEXT("GoToChat", "Go to Chat"))
				.OnClicked(this, &SBobBotConnectTab::HandleGoToChat)
				.IsEnabled_Lambda([this]() { return IsReadyToChat(); })
			]
		]

		+ SScrollBox::Slot().Padding(0, 4) [ SNew(SSeparator) ]

		// ---- AUTHENTICATION ----
		+ SScrollBox::Slot().Padding(8, 8, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("AuthSection", "AUTHENTICATION")) ]

		+ SScrollBox::Slot().Padding(16, 4)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "RadioButton")
			.IsChecked(this, &SBobBotConnectTab::GetAuthModeCheckState, EBobBotAuthMode::Subscription)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState) { HandleAuthModeChanged(EBobBotAuthMode::Subscription); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("AuthSub", "Claude Code subscription"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(12, 0, 0, 0)
				[
					SNew(STextBlock)
					.Text(this, &SBobBotConnectTab::GetAuthStatusText)
					.ColorAndOpacity(this, &SBobBotConnectTab::GetAuthStatusColor)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				]
			]
		]

		+ SScrollBox::Slot().Padding(16, 4)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "RadioButton")
			.IsChecked(this, &SBobBotConnectTab::GetAuthModeCheckState, EBobBotAuthMode::ApiKey)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState) { HandleAuthModeChanged(EBobBotAuthMode::ApiKey); })
			[
				SNew(STextBlock).Text(LOCTEXT("AuthApiKey", "API key"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]
		]

		// API key fields (visible only when ApiKey mode selected)
		+ SScrollBox::Slot().Padding(32, 4, 16, 2)
		[
			SNew(SBox).Visibility(this, &SBobBotConnectTab::GetApiKeyFieldsVisibility)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[ SNew(STextBlock).Text(LOCTEXT("ApiKeyLabel", "Key:")).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)).MinDesiredWidth(60.f) ]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SAssignNew(ApiKeyInput, SEditableTextBox)
						.IsPassword(true)
						.HintText(LOCTEXT("ApiKeyHint", "Enter your API key..."))
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 10))
						.OnTextCommitted(this, &SBobBotConnectTab::OnApiKeyTextCommitted)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
					[
						SNew(SButton).Text(LOCTEXT("SaveKey", "Save"))
						.OnClicked(this, &SBobBotConnectTab::HandleSaveApiKey)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[ SNew(STextBlock).Text(LOCTEXT("ProviderLabel", "Provider:")).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)).MinDesiredWidth(60.f) ]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&ApiProviderOptions)
						.OnSelectionChanged(this, &SBobBotConnectTab::OnApiProviderChanged)
						.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
							return SNew(STextBlock).Text(FText::FromString(*Item));
						})
						[
							SNew(STextBlock).Text_Lambda([this]() {
								return FText::FromString(FBobBotConfig::Get().ApiProvider);
							})
						]
					]
				]
				// Region field (Bedrock/Vertex)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
				[
					SNew(SBox).Visibility(this, &SBobBotConnectTab::GetApiRegionVisibility)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[ SNew(STextBlock).Text(LOCTEXT("RegionLabel", "Region:")).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)).MinDesiredWidth(60.f) ]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SEditableTextBox)
							.Text_Lambda([]() { return FText::FromString(FBobBotConfig::Get().ApiRegion); })
							.HintText(LOCTEXT("RegionHint", "us-east-1"))
							.OnTextCommitted_Lambda([](const FText& Text, ETextCommit::Type) { FBobBotConfig::Get().ApiRegion = Text.ToString(); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
						]
					]
				]
				// Project ID field (Vertex only)
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
				[
					SNew(SBox).Visibility(this, &SBobBotConnectTab::GetApiProjectIdVisibility)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
						[ SNew(STextBlock).Text(LOCTEXT("ProjectIdLabel", "Project:")).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)).MinDesiredWidth(60.f) ]
						+ SHorizontalBox::Slot().FillWidth(1.f)
						[
							SNew(SEditableTextBox)
							.Text_Lambda([]() { return FText::FromString(FBobBotConfig::Get().ApiProjectId); })
							.HintText(LOCTEXT("ProjectIdHint", "my-gcp-project"))
							.OnTextCommitted_Lambda([](const FText& Text, ETextCommit::Type) { FBobBotConfig::Get().ApiProjectId = Text.ToString(); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
						]
					]
				]
				// Status
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
				[
					BobBot::UI::KeyValueRow(LOCTEXT("ApiKeyStatusKey", "Status:"),
						TAttribute<FText>::CreateSP(this, &SBobBotConnectTab::GetApiKeyStatusText),
						TAttribute<FSlateColor>::CreateSP(this, &SBobBotConnectTab::GetApiKeyStatusColor))
				]
			]
		]

		+ SScrollBox::Slot().Padding(0, 4) [ SNew(SSeparator) ]

		// ---- HTTP BRIDGE ----
		+ SScrollBox::Slot().Padding(8, 8, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("BridgeSection", "HTTP BRIDGE")) ]

		+ SScrollBox::Slot().Padding(16, 4)
		[ BobBot::UI::KeyValueRow(LOCTEXT("BridgeStatusKey", "Status"),
			TAttribute<FText>::CreateSP(this, &SBobBotConnectTab::GetBridgeStatusText),
			TAttribute<FSlateColor>::CreateSP(this, &SBobBotConnectTab::GetBridgeStatusColor)) ]

		+ SScrollBox::Slot().Padding(16, 4)
		[ BobBot::UI::KeyValueRow(LOCTEXT("BridgeHealthKey", "Health"),
			TAttribute<FText>::CreateSP(this, &SBobBotConnectTab::GetBridgeHealthText),
			TAttribute<FSlateColor>::CreateSP(this, &SBobBotConnectTab::GetBridgeHealthColor)) ]

		+ SScrollBox::Slot().Padding(16, 4, 8, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[ SNullWidget::NullWidget ]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("RestartBridge", "Restart Bridge"))
				.OnClicked(this, &SBobBotConnectTab::HandleRestartBridge)
			]
		]

		+ SScrollBox::Slot().Padding(0, 4) [ SNew(SSeparator) ]

		// ---- MODEL ----
		+ SScrollBox::Slot().Padding(8, 8, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("ModelSection", "MODEL")) ]

		+ SScrollBox::Slot().Padding(16, 2, 8, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0) [ MakeModelButton(LOCTEXT("Sonnet", "Sonnet"), LOCTEXT("SonnetSub", "Fast"), TEXT("sonnet")) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0) [ MakeModelButton(LOCTEXT("Opus", "Opus"), LOCTEXT("OpusSub", "Best"), TEXT("opus")) ]
			+ SHorizontalBox::Slot().AutoWidth() [ MakeModelButton(LOCTEXT("Haiku", "Haiku"), LOCTEXT("HaikuSub", "Cheapest"), TEXT("haiku")) ]
		]

		+ SScrollBox::Slot().Padding(0, 4) [ SNew(SSeparator) ]

		// ---- CHAT SESSION ----
		+ SScrollBox::Slot().Padding(8, 8, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("SessionSection", "CHAT SESSION")) ]

		+ SScrollBox::Slot().Padding(16, 2) [ BobBot::UI::KeyValueRow(LOCTEXT("SessModelKey", "Model:"), TAttribute<FText>::CreateSP(this, &SBobBotConnectTab::GetSessionModelText)) ]
		+ SScrollBox::Slot().Padding(16, 2) [ BobBot::UI::KeyValueRow(LOCTEXT("SessCostKey", "Session cost:"), TAttribute<FText>::CreateSP(this, &SBobBotConnectTab::GetSessionCostText)) ]
		+ SScrollBox::Slot().Padding(16, 2, 8, 8) [ BobBot::UI::KeyValueRow(LOCTEXT("SessMsgKey", "Messages:"), TAttribute<FText>::CreateSP(this, &SBobBotConnectTab::GetSessionMessageCountText)) ]

		+ SScrollBox::Slot().Padding(0, 4) [ SNew(SSeparator) ]

		// ---- ADVANCED (collapsed by default) ----
		+ SScrollBox::Slot().Padding(8, 4)
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.OnClicked(this, &SBobBotConnectTab::HandleToggleAdvanced)
			.ContentPadding(FMargin(0))
			[
				SNew(STextBlock)
				.Text(this, &SBobBotConnectTab::GetAdvancedToggleText)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(FSlateColor(BobBot::Colors::LightGray))
			]
		]

		+ SScrollBox::Slot()
		[
			SAssignNew(AdvancedSection, SBox)
			.Visibility(EVisibility::Collapsed)  // hidden by default
			[
				AdvancedContent
			]
		]
	];
}

// =========================================================================== //
// Claude CLI detection
// =========================================================================== //

void SBobBotConnectTab::DetectClaudeCli()
{
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (!Bridge.IsAvailable()) return;

	Bridge.ExecCallWithString(TEXT("bob_chat"), TEXT("set_model"), FBobBotConfig::Get().ChatModel);

	FString Script =
		TEXT("import bob_chat, json\n")
		TEXT("found, ver = bob_chat.detect_claude()\n")
		TEXT("auth, status = bob_chat.check_auth() if found else (False, 'CLI not found')\n")
		TEXT("open(output_path, 'w').write(json.dumps({'found': found, 'version': ver, 'auth': auth, 'status': status}))\n");

	TSharedPtr<FJsonObject> Obj = Bridge.ExecPythonWithJsonResult(Script, BobBot::TempFiles::ClaudeDetect);
	if (Obj.IsValid())
	{
		FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
		Status.bClaudeCodeAvailable = Obj->GetBoolField(TEXT("found"));
		Status.ClaudeCodeVersion = Obj->GetStringField(TEXT("version"));
		Status.bClaudeAuthenticated = Obj->GetBoolField(TEXT("auth"));
		Status.ClaudeAuthStatus = Obj->GetStringField(TEXT("status"));
	}
}

// =========================================================================== //
// Setup section helpers
// =========================================================================== //

FText SBobBotConnectTab::GetClaudeInstallStatusText() const
{
	const FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	if (Status.bClaudeCodeAvailable)
		return FText::Format(LOCTEXT("ClaudeInstalled", "Installed ({0})"), FText::FromString(Status.ClaudeCodeVersion));
	return LOCTEXT("ClaudeNotInstalled", "Not installed");
}

FSlateColor SBobBotConnectTab::GetClaudeInstallStatusColor() const
{
	return FSlateColor(FBobBotRuntimeStatus::Get().bClaudeCodeAvailable ? BobBot::Colors::Green : BobBot::Colors::Red);
}

// -- Backend status row --
FText SBobBotConnectTab::GetBackendStatusText() const
{
	return LOCTEXT("BackendSDK", "Agent SDK (persistent)");
}

FSlateColor SBobBotConnectTab::GetBackendStatusColor() const
{
	return FSlateColor(BobBot::Colors::BotGreen);
}

// -- Authentication section --
FReply SBobBotConnectTab::HandleAuthModeChanged(EBobBotAuthMode Mode)
{
	FBobBotConfig& Cfg = FBobBotConfig::Get();
	Cfg.AuthMode = Mode;
	Cfg.Save();
	Cfg.ApplyEnvironmentVars();

	if (Mode == EBobBotAuthMode::ApiKey && Cfg.ApiKey.IsEmpty() && Controller)
	{
		Controller->AddExternalMessage(FBobBotChatMessage::ESender::System,
			TEXT("API key mode selected but no key is set. Enter your key below and click Save."));
	}
	return FReply::Handled();
}

ECheckBoxState SBobBotConnectTab::GetAuthModeCheckState(EBobBotAuthMode Mode) const
{
	return FBobBotConfig::Get().AuthMode == Mode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FText SBobBotConnectTab::GetAuthStatusText() const
{
	const FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	if (FBobBotConfig::Get().AuthMode == EBobBotAuthMode::ApiKey)
	{
		return FText::FromString(Status.ApiKeyStatus.IsEmpty() ? TEXT("No key set") : Status.ApiKeyStatus);
	}
	if (!Status.bClaudeCodeAvailable) return LOCTEXT("AuthNA", "\x2014");
	return Status.bClaudeAuthenticated
		? LOCTEXT("AuthSignedIn", "Signed in")
		: LOCTEXT("AuthNotSignedIn", "Not signed in");
}

FSlateColor SBobBotConnectTab::GetAuthStatusColor() const
{
	const FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	if (FBobBotConfig::Get().AuthMode == EBobBotAuthMode::ApiKey)
	{
		return FSlateColor(Status.bApiKeyValid ? BobBot::Colors::Green : BobBot::Colors::Red);
	}
	if (!Status.bClaudeCodeAvailable) return FSlateColor(BobBot::Colors::DimGray);
	return FSlateColor(Status.bClaudeAuthenticated ? BobBot::Colors::Green : BobBot::Colors::Red);
}

FReply SBobBotConnectTab::HandleSaveApiKey()
{
	if (ApiKeyInput.IsValid())
	{
		FBobBotConfig& Cfg = FBobBotConfig::Get();
		Cfg.ApiKey = ApiKeyInput->GetText().ToString();
		Cfg.Save();
		Cfg.ApplyEnvironmentVars();

		// Update status based on whether key is set
		FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
		if (Cfg.ApiKey.IsEmpty())
		{
			Status.bApiKeyValid = false;
			Status.ApiKeyStatus = TEXT("No key set");
		}
		else
		{
			// Optimistic: mark as set pending validation
			Status.ApiKeyStatus = TEXT("Key saved (not yet validated)");
		}
	}
	return FReply::Handled();
}

void SBobBotConnectTab::OnApiKeyTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		HandleSaveApiKey();
	}
}

void SBobBotConnectTab::OnApiProviderChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		FBobBotConfig& Cfg = FBobBotConfig::Get();
		Cfg.ApiProvider = *NewValue;
		Cfg.Save();
		Cfg.ApplyEnvironmentVars();
	}
}

EVisibility SBobBotConnectTab::GetApiKeyFieldsVisibility() const
{
	return FBobBotConfig::Get().AuthMode == EBobBotAuthMode::ApiKey ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SBobBotConnectTab::GetApiRegionVisibility() const
{
	const FString& Provider = FBobBotConfig::Get().ApiProvider;
	return (Provider == TEXT("bedrock") || Provider == TEXT("vertex")) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SBobBotConnectTab::GetApiProjectIdVisibility() const
{
	return FBobBotConfig::Get().ApiProvider == TEXT("vertex") ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SBobBotConnectTab::GetApiKeyStatusText() const
{
	const FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	return FText::FromString(Status.ApiKeyStatus.IsEmpty() ? TEXT("No key set") : Status.ApiKeyStatus);
}

FSlateColor SBobBotConnectTab::GetApiKeyStatusColor() const
{
	const FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	return FSlateColor(Status.bApiKeyValid ? BobBot::Colors::Green : BobBot::Colors::Red);
}

// -- HTTP Bridge section --
FText SBobBotConnectTab::GetBridgeStatusText() const
{
	const FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	if (Status.bBridgeRunning)
		return FText::Format(LOCTEXT("BridgeRunning", "Running on port {0}"), FText::AsNumber(Status.BridgePort));
	return LOCTEXT("BridgeStopped", "Stopped");
}

FSlateColor SBobBotConnectTab::GetBridgeStatusColor() const
{
	return FSlateColor(FBobBotRuntimeStatus::Get().bBridgeRunning ? BobBot::Colors::Green : BobBot::Colors::Red);
}

FText SBobBotConnectTab::GetBridgeHealthText() const
{
	const FString& Health = FBobBotRuntimeStatus::Get().BridgeHealth;
	return FText::FromString(Health.IsEmpty() ? TEXT("Unknown") : Health);
}

FSlateColor SBobBotConnectTab::GetBridgeHealthColor() const
{
	const FString& Health = FBobBotRuntimeStatus::Get().BridgeHealth;
	if (Health == TEXT("Connected")) return FSlateColor(BobBot::Colors::Green);
	if (Health == TEXT("Starting")) return FSlateColor(BobBot::Colors::Yellow);
	return FSlateColor(BobBot::Colors::Red);
}

FReply SBobBotConnectTab::HandleRestartBridge()
{
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (Bridge.IsAvailable())
	{
		// Run stop+start in a single background thread so port wait doesn't freeze the editor.
		Bridge.ExecPythonCommand(TEXT("import bob_bridge_launcher, threading\n")
			TEXT("def _restart():\n")
			TEXT("    bob_bridge_launcher.stop()\n")
			TEXT("    bob_bridge_launcher.start()\n")
			TEXT("threading.Thread(target=_restart, daemon=True).start()\n"));
		if (Controller)
			Controller->AddExternalMessage(FBobBotChatMessage::ESender::System, TEXT("Restarting HTTP bridge..."));
	}
	return FReply::Handled();
}




bool SBobBotConnectTab::IsReadyToChat() const
{
	return FBobBotRuntimeStatus::Get().IsReadyToChat();
}

FText SBobBotConnectTab::GetReadyStatusText() const
{
	if (IsReadyToChat()) return LOCTEXT("Ready", "Ready to chat!");
	if (!FBobBotRuntimeStatus::Get().bClaudeCodeAvailable) return LOCTEXT("NeedInstall", "Install Claude Code first");
	return LOCTEXT("NeedAuth", "Run 'claude login' in terminal");
}

FSlateColor SBobBotConnectTab::GetReadyStatusColor() const
{
	return FSlateColor(IsReadyToChat() ? BobBot::Colors::Green : BobBot::Colors::Yellow);
}

FReply SBobBotConnectTab::HandleRefreshStatus()
{
	DetectClaudeCli();
	return FReply::Handled();
}

FReply SBobBotConnectTab::HandleGoToChat()
{
	OnGoToChat.ExecuteIfBound();
	return FReply::Handled();
}

// =========================================================================== //
// Model helpers
// =========================================================================== //

FSlateColor SBobBotConnectTab::GetModelButtonColor(FString ModelName) const
{
	return FSlateColor(FBobBotConfig::Get().ChatModel == ModelName ? BobBot::Colors::ActiveBlue : BobBot::Colors::InactiveDark);
}

FReply SBobBotConnectTab::HandleModelSelected(FString ModelName)
{
	FBobBotConfig& Config = FBobBotConfig::Get();
	Config.ChatModel = ModelName;
	Config.Save();

	FBobBotPythonBridge::Get().ExecCallWithString(TEXT("bob_chat"), TEXT("set_model"), ModelName);

	if (Controller)
		Controller->AddExternalMessage(FBobBotChatMessage::ESender::System, FString::Printf(TEXT("Model switched to %s."), *ModelName));

	return FReply::Handled();
}

// =========================================================================== //
// Chat Session helpers (reads from Controller)
// =========================================================================== //

FText SBobBotConnectTab::GetSessionModelText() const
{
	return FText::FromString(FBobBotConfig::Get().ChatModel);
}

FText SBobBotConnectTab::GetSessionCostText() const
{
	return FText::FromString(FString::Printf(TEXT("$%.2f"), Controller ? Controller->GetSessionCost() : 0.f));
}

FText SBobBotConnectTab::GetSessionMessageCountText() const
{
	return FText::AsNumber(Controller ? Controller->GetMessageCount() : 0);
}

// =========================================================================== //
// Advanced section helpers
// =========================================================================== //

FReply SBobBotConnectTab::HandleToggleAdvanced()
{
	bAdvancedExpanded = !bAdvancedExpanded;
	if (AdvancedSection.IsValid())
		AdvancedSection->SetVisibility(bAdvancedExpanded ? EVisibility::Visible : EVisibility::Collapsed);
	return FReply::Handled();
}

FText SBobBotConnectTab::GetAdvancedToggleText() const
{
	return bAdvancedExpanded ? LOCTEXT("CollapseAdv", "\x25BC  Advanced") : LOCTEXT("ExpandAdv", "\x25B6  Advanced");
}

// Permission mode
FReply SBobBotConnectTab::HandlePermissionModeChanged(EBobBotPermissionMode Mode)
{
	FBobBotConfig& Config = FBobBotConfig::Get();
	Config.PermissionMode = Mode;
	Config.Save();
	Config.ApplyEnvironmentVars();

	const TCHAR* ModeStr = FBobBotConfig::PermissionModeToString(Mode);

	FBobBotPythonBridge::Get().ExecPythonCommand(*FString::Printf(
		TEXT("import os; os.environ['BOB_PERMISSION_MODE'] = '%s'"), ModeStr));

	return FReply::Handled();
}

ECheckBoxState SBobBotConnectTab::GetPermissionCheckState(EBobBotPermissionMode Mode) const
{
	return FBobBotConfig::Get().PermissionMode == Mode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

// MCP client config
bool SBobBotConnectTab::DoesClientConfigExist(const FString& ClientName) const
{
	FString TargetPath;
	if (ClientName == TEXT("claude")) TargetPath = FPaths::ProjectDir() / TEXT(".mcp.json");
	else if (ClientName == TEXT("cursor")) TargetPath = FPaths::ProjectDir() / TEXT(".cursor") / TEXT("mcp.json");
	else if (ClientName == TEXT("vscode")) TargetPath = FPaths::ProjectDir() / TEXT(".vscode") / TEXT("mcp.json");
	else if (ClientName == TEXT("windsurf")) TargetPath = FString(FPlatformProcess::UserHomeDir()) / TEXT(".codeium") / TEXT("windsurf") / TEXT("mcp_config.json");
	if (TargetPath.IsEmpty()) return false;
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*TargetPath);
}

FText SBobBotConnectTab::GetClientStatusText(FString ClientName) const
{
	return DoesClientConfigExist(ClientName)
		? LOCTEXT("Configured", "Configured")
		: LOCTEXT("NotConfigured", "Not configured");
}

FSlateColor SBobBotConnectTab::GetClientStatusColor(FString ClientName) const
{
	return FSlateColor(DoesClientConfigExist(ClientName) ? BobBot::Colors::Green : BobBot::Colors::DimGray);
}

FReply SBobBotConnectTab::HandleSetupClient(FString ClientName)
{
	FBobBotModule& Module = FModuleManager::GetModuleChecked<FBobBotModule>("BobBot");
	FString TargetPath;
	if (ClientName == TEXT("claude")) TargetPath = FPaths::ProjectDir() / TEXT(".mcp.json");
	else if (ClientName == TEXT("cursor")) TargetPath = FPaths::ProjectDir() / TEXT(".cursor") / TEXT("mcp.json");
	else if (ClientName == TEXT("vscode")) TargetPath = FPaths::ProjectDir() / TEXT(".vscode") / TEXT("mcp.json");
	else if (ClientName == TEXT("windsurf")) TargetPath = FString(FPlatformProcess::UserHomeDir()) / TEXT(".codeium") / TEXT("windsurf") / TEXT("mcp_config.json");
	if (TargetPath.IsEmpty()) return FReply::Handled();

	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	PF.CreateDirectoryTree(*FPaths::GetPath(TargetPath));

	if (Module.WriteClientConfig(ClientName, TargetPath))
	{
		if (Controller)
			Controller->AddExternalMessage(FBobBotChatMessage::ESender::System,
				FString::Printf(TEXT("%s configured: %s"), *ClientName, *TargetPath));
	}
	else
	{
		if (Controller)
			Controller->AddExternalMessage(FBobBotChatMessage::ESender::Error,
				FString::Printf(TEXT("Failed to configure %s"), *ClientName));
	}
	return FReply::Handled();
}

// Paths
FText SBobBotConnectTab::GetProjectRootPath() const
{
	FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	bool bExists = FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*Root);
	return FText::FromString(FString::Printf(TEXT("%s   %s"), *Root, bExists ? TEXT("exists") : TEXT("MISSING")));
}

FText SBobBotConnectTab::GetMcpJsonPath() const
{
	FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT(".mcp.json"));
	bool bExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*Path);
	return FText::FromString(FString::Printf(TEXT("%s   %s"), *Path, bExists ? TEXT("exists") : TEXT("MISSING")));
}

FText SBobBotConnectTab::GetBridgePath() const
{
	FString Path = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectPluginsDir() / TEXT("BobBot") / TEXT("Scripts") / TEXT("bob_mcp_bridge.py"));
	bool bExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*Path);
	return FText::FromString(FString::Printf(TEXT("%s   %s"), *Path, bExists ? TEXT("exists") : TEXT("MISSING")));
}

FText SBobBotConnectTab::GetPromptFilePath() const
{
	FString Path = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / BobBot::SavedSubDir / BobBot::TempFiles::SystemPrompt);
	bool bExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*Path);
	return FText::FromString(FString::Printf(TEXT("%s   %s"), *Path, bExists ? TEXT("exists") : TEXT("MISSING")));
}

// Prerequisites
FText SBobBotConnectTab::GetPluginPrereqText() const
{
	return FBobBotRuntimeStatus::Get().bPythonPluginAvailable
		? LOCTEXT("PluginOK", "PythonScriptPlugin") : LOCTEXT("PluginMissing", "PythonScriptPlugin missing");
}
FSlateColor SBobBotConnectTab::GetPluginPrereqColor() const { return FSlateColor(FBobBotRuntimeStatus::Get().bPythonPluginAvailable ? BobBot::Colors::Green : BobBot::Colors::Red); }
FText SBobBotConnectTab::GetAgentSDKPrereqText() const
{
	const FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	return Status.bAgentSDKAvailable
		? FText::Format(LOCTEXT("SDKOK", "claude-agent-sdk {0}"), FText::FromString(Status.AgentSDKVersion))
		: LOCTEXT("SDKMissing", "claude-agent-sdk not available (auto-installs on first launch)");
}
FSlateColor SBobBotConnectTab::GetAgentSDKPrereqColor() const { return FSlateColor(FBobBotRuntimeStatus::Get().bAgentSDKAvailable ? BobBot::Colors::Green : BobBot::Colors::Yellow); }

// =========================================================================== //
// Troubleshooting handlers
// =========================================================================== //

FReply SBobBotConnectTab::HandleDiagHealthCheck()
{
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (!Bridge.IsAvailable() || !Controller) return FReply::Handled();

	FString Script =
		TEXT("import bob_bridge_launcher, bob_chat, json, os, socket\n")
		TEXT("results = []\n")
		TEXT("venv_py = bob_bridge_launcher._get_venv_python()\n")
		TEXT("results.append({'name':'Venv','ok':os.path.isfile(venv_py),'detail':venv_py if os.path.isfile(venv_py) else 'missing'})\n")
		TEXT("port = bob_bridge_launcher._get_port()\n")
		TEXT("bok = bob_bridge_launcher._health_check(port)\n")
		TEXT("results.append({'name':'Bridge','ok':bok,'detail':'port '+str(port)})\n")
		TEXT("found, ver = bob_chat.detect_claude()\n")
		TEXT("results.append({'name':'Claude CLI','ok':found,'detail':ver if found else 'not found'})\n")
		TEXT("if found:\n")
		TEXT("    aok, amsg = bob_chat.check_auth()\n")
		TEXT("    results.append({'name':'Auth','ok':aok,'detail':amsg})\n")
		TEXT("try:\n")
		TEXT("    import bob_chat_sdk\n")
		TEXT("    si = bob_chat_sdk.check_sdk_available()\n")
		TEXT("    results.append({'name':'Agent SDK','ok':si.get('ok',False),'detail':si.get('ver','unknown')})\n")
		TEXT("except Exception as e:\n")
		TEXT("    results.append({'name':'Agent SDK','ok':False,'detail':str(e)})\n")
		TEXT("root = os.environ.get('BOB_PROJECT_ROOT','.')\n")
		TEXT("mcp = os.path.join(root, '.mcp.json')\n")
		TEXT("if os.path.isfile(mcp):\n")
		TEXT("    try: json.load(open(mcp)); results.append({'name':'.mcp.json','ok':True,'detail':'valid'})\n")
		TEXT("    except: results.append({'name':'.mcp.json','ok':False,'detail':'corrupt JSON'})\n")
		TEXT("else: results.append({'name':'.mcp.json','ok':False,'detail':'missing'})\n")
		TEXT("all_ok = all(r['ok'] for r in results)\n")
		TEXT("open(output_path,'w').write(json.dumps({'ok':all_ok,'checks':results}))\n");

	TSharedPtr<FJsonObject> Result = Bridge.ExecPythonWithJsonResult(Script, TEXT("_diag_health.json"));
	if (Result.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Checks;
		if (Result->TryGetArrayField(TEXT("checks"), Checks))
		{
			FString Report = TEXT("=== Health Check ===\n");
			for (const auto& Check : *Checks)
			{
				TSharedPtr<FJsonObject> C = Check->AsObject();
				if (!C.IsValid()) continue;
				bool bOk = C->GetBoolField(TEXT("ok"));
				FString Name = C->GetStringField(TEXT("name"));
				FString Detail = C->GetStringField(TEXT("detail"));
				Report += FString::Printf(TEXT("%s %s: %s\n"),
					bOk ? TEXT("\x2713") : TEXT("\x2717"), *Name, *Detail);
			}
			bool bAllOk = Result->GetBoolField(TEXT("ok"));
			Report += bAllOk ? TEXT("\nAll systems healthy.") : TEXT("\nIssues found above.");
			Controller->AddExternalMessage(
				bAllOk ? FBobBotChatMessage::ESender::System : FBobBotChatMessage::ESender::Error,
				Report);
		}
		else
		{
			Controller->AddExternalMessage(FBobBotChatMessage::ESender::Error,
				TEXT("Health check failed to parse results. Check the Output Log."));
		}
	}
	return FReply::Handled();
}

FReply SBobBotConnectTab::HandleDiagViewBridgeLog()
{
	FString BridgeLogPath = FPaths::ProjectSavedDir() / BobBot::SavedSubDir / TEXT("_bridge.log");
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *BridgeLogPath))
		Content = TEXT("(No bridge log found. Bridge may not have started.)");
	else if (Content.Len() > 3000)
		Content = TEXT("...(truncated)...\n") + Content.Right(3000);

	TSharedRef<SWindow> LogWindow = SNew(SWindow)
		.Title(LOCTEXT("BridgeLogTitle", "Bridge Log"))
		.ClientSize(FVector2D(600, 400))
		.SupportsMinimize(false).SupportsMaximize(false)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(8))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(STextBlock).Text(FText::FromString(Content))
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
						.ColorAndOpacity(FSlateColor(BobBot::Colors::CodeText))
						.AutoWrapText(true)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0, 8, 0, 0)
				[
					SNew(SButton).Text(LOCTEXT("CloseLog", "Close"))
					.OnClicked_Lambda([LogWindow]() mutable {
						LogWindow->RequestDestroyWindow();
						return FReply::Handled();
					})
				]
			]
		];
	FSlateApplication::Get().AddWindow(LogWindow);
	return FReply::Handled();
}

FReply SBobBotConnectTab::HandleDiagRebuildVenv()
{
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (!Bridge.IsAvailable()) return FReply::Handled();

	if (Controller)
		Controller->AddExternalMessage(FBobBotChatMessage::ESender::System,
			TEXT("Rebuilding Python environment... (this may take 30-60s)"));

	Bridge.ExecPythonCommand(
		TEXT("import bob_bridge_launcher, shutil, os, threading\n")
		TEXT("def _rebuild():\n")
		TEXT("    import unreal\n")
		TEXT("    bob_bridge_launcher.stop()\n")
		TEXT("    venv = bob_bridge_launcher._get_venv_dir()\n")
		TEXT("    if os.path.isdir(venv): shutil.rmtree(venv, ignore_errors=True)\n")
		TEXT("    bob_bridge_launcher._venv_ready = False\n")
		TEXT("    r1 = bob_bridge_launcher.setup_create_venv()\n")
		TEXT("    if not r1.get('ok'):\n")
		TEXT("        unreal.log_warning('BobBot: Venv rebuild failed: ' + r1.get('message',''))\n")
		TEXT("        return\n")
		TEXT("    r2 = bob_bridge_launcher.setup_install_mcp()\n")
		TEXT("    unreal.log('BobBot rebuild mcp: ' + r2.get('message',''))\n")
		TEXT("    r3 = bob_bridge_launcher.setup_install_sdk()\n")
		TEXT("    unreal.log('BobBot rebuild sdk: ' + r3.get('message',''))\n")
		TEXT("    bob_bridge_launcher.start()\n")
		TEXT("    unreal.log('BobBot: Venv rebuild complete, bridge restarted')\n")
		TEXT("threading.Thread(target=_rebuild, daemon=True).start()\n"));

	return FReply::Handled();
}

FReply SBobBotConnectTab::HandleDiagReinstallPackages()
{
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (!Bridge.IsAvailable()) return FReply::Handled();

	if (Controller)
		Controller->AddExternalMessage(FBobBotChatMessage::ESender::System,
			TEXT("Reinstalling packages... (this may take 60s)"));

	Bridge.ExecPythonCommand(
		TEXT("import bob_bridge_launcher, threading\n")
		TEXT("def _reinstall():\n")
		TEXT("    import unreal\n")
		TEXT("    bob_bridge_launcher.stop()\n")
		TEXT("    r1 = bob_bridge_launcher.setup_install_mcp()\n")
		TEXT("    unreal.log('BobBot reinstall mcp: ' + r1.get('message',''))\n")
		TEXT("    r2 = bob_bridge_launcher.setup_install_sdk()\n")
		TEXT("    unreal.log('BobBot reinstall sdk: ' + r2.get('message',''))\n")
		TEXT("    bob_bridge_launcher.start()\n")
		TEXT("    msg = 'Packages reinstalled'\n")
		TEXT("    if not r1.get('ok'): msg += ' (mcp FAILED)'\n")
		TEXT("    if not r2.get('ok'): msg += ' (sdk FAILED)'\n")
		TEXT("    unreal.log('BobBot: ' + msg)\n")
		TEXT("threading.Thread(target=_reinstall, daemon=True).start()\n"));

	return FReply::Handled();
}

FReply SBobBotConnectTab::HandleDiagRegenMcpConfig()
{
	FBobBotModule& Module = FModuleManager::GetModuleChecked<FBobBotModule>("BobBot");
	Module.EnsureMcpJson();

	if (Controller)
		Controller->AddExternalMessage(FBobBotChatMessage::ESender::System,
			TEXT("MCP config regenerated."));

	return FReply::Handled();
}

FReply SBobBotConnectTab::HandleDiagClearTempFiles()
{
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (!Bridge.IsAvailable()) return FReply::Handled();

	Bridge.ExecPythonCommand(
		TEXT("import os\n")
		TEXT("d = os.path.join(os.environ.get('BOB_PROJECT_ROOT','.'), 'Saved', 'BobBot')\n")
		TEXT("count = 0\n")
		TEXT("if os.path.isdir(d):\n")
		TEXT("    for f in os.listdir(d):\n")
		TEXT("        if f.startswith('_') and (f.endswith('.txt') or f.endswith('.json')):\n")
		TEXT("            try: os.remove(os.path.join(d, f)); count += 1\n")
		TEXT("            except: pass\n")
		TEXT("import unreal; unreal.log('BobBot: Cleared {} temp files'.format(count))\n"));

	if (Controller)
		Controller->AddExternalMessage(FBobBotChatMessage::ESender::System,
			TEXT("Temp files cleared."));

	return FReply::Handled();
}

FReply SBobBotConnectTab::HandleDiagKillPortConflicts()
{
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (!Bridge.IsAvailable()) return FReply::Handled();

	if (Controller)
		Controller->AddExternalMessage(FBobBotChatMessage::ESender::System,
			TEXT("Checking for port conflicts..."));

	Bridge.ExecPythonCommand(
		TEXT("import os, subprocess, threading\n")
		TEXT("def _kill_ports():\n")
		TEXT("    import unreal\n")
		TEXT("    bridge_port = os.environ.get('BOB_MCP_BRIDGE_PORT', '13580')\n")
		TEXT("    mcp_port = os.environ.get('BOB_MCP_PORT', '13579')\n")
		TEXT("    killed = []\n")
		TEXT("    for name, port in [('bridge', bridge_port), ('MCP', mcp_port)]:\n")
		TEXT("        try:\n")
		TEXT("            r = subprocess.run(['netstat', '-ano'], capture_output=True, text=True, timeout=5,\n")
		TEXT("                creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))\n")
		TEXT("            for line in r.stdout.splitlines():\n")
		TEXT("                if ':' + port + ' ' in line and 'LISTENING' in line:\n")
		TEXT("                    pid = line.split()[-1]\n")
		TEXT("                    if pid and pid != '0':\n")
		TEXT("                        subprocess.run(['taskkill', '/F', '/PID', pid],\n")
		TEXT("                            capture_output=True, timeout=5,\n")
		TEXT("                            creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))\n")
		TEXT("                        killed.append('{} (port {}, PID {})'.format(name, port, pid))\n")
		TEXT("        except Exception as e:\n")
		TEXT("            unreal.log_warning('BobBot port check: {}'.format(e))\n")
		TEXT("    if killed:\n")
		TEXT("        unreal.log('BobBot: Killed port conflicts: ' + ', '.join(killed))\n")
		TEXT("        import bob_bridge_launcher\n")
		TEXT("        bob_bridge_launcher.start()\n")
		TEXT("    else:\n")
		TEXT("        unreal.log('BobBot: No port conflicts found')\n")
		TEXT("threading.Thread(target=_kill_ports, daemon=True).start()\n"));

	return FReply::Handled();
}

// Factory Reset
FReply SBobBotConnectTab::HandleFactoryReset()
{
	// Stop the bridge
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (Bridge.IsAvailable())
	{
		Bridge.ExecPythonCommand(TEXT("import bob_bridge_launcher; bob_bridge_launcher.stop()"));
	}

	// Delete venv, config, and temp files via Python (handles cross-platform paths)
	if (Bridge.IsAvailable())
	{
		Bridge.ExecPythonCommand(
			TEXT("import shutil, os\n")
			TEXT("root = os.environ.get('BOB_PROJECT_ROOT', '.')\n")
			TEXT("venv = os.path.join(root, 'Saved', 'BobBot', '.venv')\n")
			TEXT("if os.path.isdir(venv): shutil.rmtree(venv, ignore_errors=True)\n")
			TEXT("for f in ['_sdk_check.txt','_welcome_step.json','_bridge.log','_bridge_health.txt','_welcome_poll.json']:\n")
			TEXT("    p = os.path.join(root, 'Saved', 'BobBot', f)\n")
			TEXT("    if os.path.isfile(p): os.remove(p)\n")
		);
	}

	// Delete config INI
	FString ConfigPath = FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("BobBot.ini");
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	PF.DeleteFile(*ConfigPath);

	// Reset runtime status
	FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	Status.bBridgeRunning = false;
	Status.BridgeHealth = TEXT("");
	Status.bAgentSDKAvailable = false;
	Status.AgentSDKVersion = TEXT("");

	// Reload config — with INI deleted, Load() is a no-op and fields keep code defaults.
	// Reset the key flag explicitly since the singleton persists in memory.
	FBobBotConfig& Cfg = FBobBotConfig::Get();
	Cfg.bSetupComplete = false;
	Cfg.MaxClients = 4;
	Cfg.MaxBudgetUsd = 5.0f;
	Cfg.AuthMode = EBobBotAuthMode::Subscription;
	Cfg.ApiKey = TEXT("");
	Cfg.ApiProvider = TEXT("anthropic");
	Cfg.ApiRegion = TEXT("");
	Cfg.ApiProjectId = TEXT("");

	// Fire delegate to switch to Welcome tab
	OnFactoryReset.ExecuteIfBound();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
