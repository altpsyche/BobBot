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
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"
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
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2, 8, 8)
		[
			SNew(STextBlock).Text(this, &SBobBotConnectTab::GetPluginPrereqText)
			.ColorAndOpacity(this, &SBobBotConnectTab::GetPluginPrereqColor)
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
		[ BobBot::UI::KeyValueRow(LOCTEXT("AuthKey", "Authentication"),
			TAttribute<FText>::CreateSP(this, &SBobBotConnectTab::GetAuthStatusText),
			TAttribute<FSlateColor>::CreateSP(this, &SBobBotConnectTab::GetAuthStatusColor)) ]

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

FText SBobBotConnectTab::GetAuthStatusText() const
{
	const FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	if (!Status.bClaudeCodeAvailable) return LOCTEXT("AuthNA", "\x2014");
	return Status.bClaudeAuthenticated
		? LOCTEXT("AuthSignedIn", "Signed in")
		: LOCTEXT("AuthNotSignedIn", "Not signed in");
}

FSlateColor SBobBotConnectTab::GetAuthStatusColor() const
{
	const FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	if (!Status.bClaudeCodeAvailable) return FSlateColor(BobBot::Colors::DimGray);
	return FSlateColor(Status.bClaudeAuthenticated ? BobBot::Colors::Green : BobBot::Colors::Red);
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

#undef LOCTEXT_NAMESPACE
