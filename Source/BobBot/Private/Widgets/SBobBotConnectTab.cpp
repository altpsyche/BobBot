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

		// TOOL PERMISSIONS
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("ToolPerms", "TOOL PERMISSIONS")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "RadioButton")
			.IsChecked(this, &SBobBotConnectTab::GetPermissionCheckState, EBobBotPermissionMode::AllowAlways)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState) { HandlePermissionModeChanged(EBobBotPermissionMode::AllowAlways); })
			[
				SNew(STextBlock).Text(LOCTEXT("AllowAlways", "Allow Always \x2014 Claude can run code without asking"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "RadioButton")
			.IsChecked(this, &SBobBotConnectTab::GetPermissionCheckState, EBobBotPermissionMode::AskMe)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState) { HandlePermissionModeChanged(EBobBotPermissionMode::AskMe); })
			[
				SNew(STextBlock).Text(LOCTEXT("AskMe", "Ask Me \x2014 Approve each tool call before it runs"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "RadioButton")
			.IsChecked(this, &SBobBotConnectTab::GetPermissionCheckState, EBobBotPermissionMode::ChatOnly)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState) { HandlePermissionModeChanged(EBobBotPermissionMode::ChatOnly); })
			[
				SNew(STextBlock).Text(LOCTEXT("ChatOnly", "Chat Only \x2014 Claude answers only, no tool access"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
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
				SNew(SCheckBox)
				.IsChecked(this, &SBobBotConnectTab::GetSDKToggleState)
				.OnCheckStateChanged(this, &SBobBotConnectTab::OnSDKToggleChanged)
				[
					SNew(STextBlock).Text(LOCTEXT("UseSDK", "Use Agent SDK (default)"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("SDKInfo", "?"))
				.ToolTipText(LOCTEXT("SDKInfoTip", "Compare Agent SDK vs Subprocess mode"))
				.OnClicked(this, &SBobBotConnectTab::HandleShowSDKInfoDialog)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(32, 2, 16, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SDKDesc", "Persistent process, cost budgets, subagents, forking. Takes effect on next message."))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
			.AutoWrapText(true)
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
			SNew(STextBlock).Text(this, &SBobBotConnectTab::GetPythonPrereqText)
			.ColorAndOpacity(this, &SBobBotConnectTab::GetPythonPrereqColor)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(STextBlock).Text(this, &SBobBotConnectTab::GetUvPrereqText)
			.ColorAndOpacity(this, &SBobBotConnectTab::GetUvPrereqColor)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
		]
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
		];

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
							.Text(FText::FromString(Config.ApiRegion))
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
							.Text(FText::FromString(Config.ApiProjectId))
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
	return FBobBotConfig::Get().bUseAgentSDK
		? LOCTEXT("BackendSDK", "Agent SDK (persistent)")
		: LOCTEXT("BackendSub", "Subprocess");
}

FSlateColor SBobBotConnectTab::GetBackendStatusColor() const
{
	return FSlateColor(FBobBotConfig::Get().bUseAgentSDK ? BobBot::Colors::BotGreen : BobBot::Colors::DimGray);
}

// -- Authentication section --
FReply SBobBotConnectTab::HandleAuthModeChanged(EBobBotAuthMode Mode)
{
	FBobBotConfig& Cfg = FBobBotConfig::Get();
	Cfg.AuthMode = Mode;
	Cfg.Save();
	Cfg.ApplyEnvironmentVars();
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

// -- SDK toggle --
void SBobBotConnectTab::OnSDKToggleChanged(ECheckBoxState State)
{
	FBobBotConfig& Cfg = FBobBotConfig::Get();
	Cfg.bUseAgentSDK = (State == ECheckBoxState::Checked);
	Cfg.Save();
	Cfg.ApplyEnvironmentVars();
	if (Controller)
	{
		Controller->AddExternalMessage(FBobBotChatMessage::ESender::System,
			FString::Printf(TEXT("Backend switched to %s. Takes effect on next message."),
				Cfg.bUseAgentSDK ? TEXT("Agent SDK") : TEXT("Subprocess")));
	}
}

ECheckBoxState SBobBotConnectTab::GetSDKToggleState() const
{
	return FBobBotConfig::Get().bUseAgentSDK ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FReply SBobBotConnectTab::HandleShowSDKInfoDialog()
{
	TSharedRef<SWindow> InfoWindow = SNew(SWindow)
		.Title(LOCTEXT("SDKInfoTitle", "Agent SDK vs Subprocess Mode"))
		.ClientSize(FVector2D(460, 320))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(16, 12))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SDKInfoHeading", "Agent SDK (recommended):"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					.ColorAndOpacity(FSlateColor(BobBot::Colors::BotGreen))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(8, 4, 0, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SDKInfoBody",
						"- Persistent Claude process across messages (~0s wait)\n"
						"- Cost budgets per message\n"
						"- Specialized subagents for different tasks\n"
						"- Session forking to explore alternatives\n"
						"- Requires: pip install claude-agent-sdk"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SubInfoHeading", "Subprocess mode (fallback):"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(8, 4, 0, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SubInfoBody",
						"- New Claude process per message (~1-2s overhead)\n"
						"- Basic tool execution only\n"
						"- No extra dependencies needed\n"
						"- Use this if Agent SDK has issues"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().FillHeight(1.f) [ SNullWidget::NullWidget ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(SButton).Text(LOCTEXT("CloseDialog", "Close"))
					.OnClicked_Lambda([InfoWindow]() mutable {
						InfoWindow->RequestDestroyWindow();
						return FReply::Handled();
					})
				]
			]
		];

	FSlateApplication::Get().AddWindow(InfoWindow);
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
FText SBobBotConnectTab::GetPythonPrereqText() const
{
	const FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	return Status.bPythonAvailable
		? FText::Format(LOCTEXT("PythonOK", "Python {0}"), FText::FromString(Status.PythonVersion))
		: LOCTEXT("PythonMissing", "Python not found");
}
FText SBobBotConnectTab::GetUvPrereqText() const
{
	const FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	return Status.bUvAvailable
		? FText::Format(LOCTEXT("UvOK", "uv {0}"), FText::FromString(Status.UvVersion))
		: LOCTEXT("UvMissing", "uv not found");
}
FText SBobBotConnectTab::GetPluginPrereqText() const
{
	return FBobBotRuntimeStatus::Get().bPythonPluginAvailable
		? LOCTEXT("PluginOK", "PythonScriptPlugin") : LOCTEXT("PluginMissing", "PythonScriptPlugin missing");
}
FSlateColor SBobBotConnectTab::GetPythonPrereqColor() const { return FSlateColor(FBobBotRuntimeStatus::Get().bPythonAvailable ? BobBot::Colors::Green : BobBot::Colors::Red); }
FSlateColor SBobBotConnectTab::GetUvPrereqColor() const { return FSlateColor(FBobBotRuntimeStatus::Get().bUvAvailable ? BobBot::Colors::Green : BobBot::Colors::Red); }
FSlateColor SBobBotConnectTab::GetPluginPrereqColor() const { return FSlateColor(FBobBotRuntimeStatus::Get().bPythonPluginAvailable ? BobBot::Colors::Green : BobBot::Colors::Red); }
FText SBobBotConnectTab::GetAgentSDKPrereqText() const
{
	const FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	return Status.bAgentSDKAvailable
		? FText::Format(LOCTEXT("SDKOK", "claude-agent-sdk {0}"), FText::FromString(Status.AgentSDKVersion))
		: LOCTEXT("SDKMissing", "claude-agent-sdk not found (pip install claude-agent-sdk)");
}
FSlateColor SBobBotConnectTab::GetAgentSDKPrereqColor() const { return FSlateColor(FBobBotRuntimeStatus::Get().bAgentSDKAvailable ? BobBot::Colors::Green : BobBot::Colors::Yellow); }

#undef LOCTEXT_NAMESPACE
