// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "Widgets/SBobBotPanel.h"
#include "BobBot.h"
#include "BobBotConfig.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IPythonScriptPlugin.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "SBobBotPanel"

// --------------------------------------------------------------------------- //
// Construction
// --------------------------------------------------------------------------- //

void SBobBotPanel::Construct(const FArguments& InArgs)
{
	DetectClaudeCli();

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton).Text(LOCTEXT("ConnectTab", "Connect"))
				.OnClicked(this, &SBobBotPanel::OnTabClicked, EBobBotTab::Connect)
				.ButtonColorAndOpacity(this, &SBobBotPanel::GetTabColor, EBobBotTab::Connect)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("ChatTab", "Chat"))
				.OnClicked(this, &SBobBotPanel::OnTabClicked, EBobBotTab::Chat)
				.ButtonColorAndOpacity(this, &SBobBotPanel::GetTabColor, EBobBotTab::Chat)
			]
		]

		+ SVerticalBox::Slot().AutoHeight() [ SNew(SSeparator) ]

		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(TabSwitcher, SWidgetSwitcher)
			.WidgetIndex(0)
			+ SWidgetSwitcher::Slot() [ BuildConnectTab() ]
			+ SWidgetSwitcher::Slot() [ BuildChatTab() ]
		]
	];

	AddChatMessage(FChatMessage::ESender::System, TEXT("BobBot ready. Type a message and press Enter to chat with Claude."));
}

// --------------------------------------------------------------------------- //
// Tab switching
// --------------------------------------------------------------------------- //

FReply SBobBotPanel::OnTabClicked(EBobBotTab Tab)
{
	ActiveTab = Tab;
	if (TabSwitcher.IsValid())
		TabSwitcher->SetActiveWidgetIndex(static_cast<int32>(Tab));
	return FReply::Handled();
}

FSlateColor SBobBotPanel::GetTabColor(EBobBotTab Tab) const
{
	return FSlateColor(ActiveTab == Tab ? FLinearColor(0.2f, 0.4f, 0.8f, 1.f) : FLinearColor(0.3f, 0.3f, 0.3f, 1.f));
}

// --------------------------------------------------------------------------- //
// Claude CLI detection
// --------------------------------------------------------------------------- //

void SBobBotPanel::DetectClaudeCli()
{
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin) return;

	FString TempDir = FPaths::ProjectSavedDir() / TEXT("BobBot");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*TempDir);

	FString OutputFile = TempDir / TEXT("_claude_detect.txt");

	FString Script = FString::Printf(
		TEXT("import bob_chat, json\n")
		TEXT("found, ver = bob_chat.detect_claude()\n")
		TEXT("auth, status = bob_chat.check_auth() if found else (False, 'CLI not found')\n")
		TEXT("bob_chat.set_model('%s')\n")
		TEXT("open(r'%s', 'w').write(json.dumps({'found': found, 'version': ver, 'auth': auth, 'status': status}))\n"),
		*FBobBotConfig::Get().ChatModel,
		*OutputFile.Replace(TEXT("\\"), TEXT("/")));

	PythonPlugin->ExecPythonCommand(*Script);

	FString ResultJson;
	if (FFileHelper::LoadFileToString(ResultJson, *OutputFile))
	{
		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResultJson);
		if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
		{
			FBobBotConfig& Config = FBobBotConfig::Get();
			Config.bClaudeCodeAvailable = Obj->GetBoolField(TEXT("found"));
			Config.ClaudeCodeVersion = Obj->GetStringField(TEXT("version"));
			Config.bClaudeAuthenticated = Obj->GetBoolField(TEXT("auth"));
			Config.ClaudeAuthStatus = Obj->GetStringField(TEXT("status"));
		}
	}

	PlatformFile.DeleteFile(*OutputFile);
}

// --------------------------------------------------------------------------- //
// Connect Tab
// --------------------------------------------------------------------------- //

TSharedRef<SWidget> SBobBotPanel::BuildConnectTab()
{
	auto MakeClientRow = [this](const FText& DisplayName, const FString& ClientKey, const FText& Description) -> TSharedRef<SWidget>
	{
		FString Key = ClientKey;
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 6, 8, 2)
			[ SNew(STextBlock).Text(DisplayName).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10)) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(16, 0, 8, 2)
			[ SNew(STextBlock).Text(Description).ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f))) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(16, 2, 8, 6)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
				[ SNew(SButton).Text(LOCTEXT("CopyConfig", "Copy Config")).OnClicked(FOnClicked::CreateSP(this, &SBobBotPanel::HandleCopyConfig, Key)) ]
				+ SHorizontalBox::Slot().AutoWidth()
				[ SNew(SButton).Text(LOCTEXT("WriteConfig", "Write Config")).OnClicked(FOnClicked::CreateSP(this, &SBobBotPanel::HandleWriteConfig, Key)) ]
			];
	};

	return SNew(SScrollBox)

		// Server Status
		+ SScrollBox::Slot().Padding(8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("\x25CF")))
				.ColorAndOpacity(this, &SBobBotPanel::GetServerStatusColor)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[ SNew(STextBlock).Text(this, &SBobBotPanel::GetServerStatusText).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11)) ]
		]

		+ SScrollBox::Slot() [ SNew(SSeparator) ]

		// Claude Code Status
		+ SScrollBox::Slot().Padding(8, 8, 8, 2)
		[ SNew(STextBlock).Text(LOCTEXT("ClaudeSection", "Claude Code")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11)) ]

		+ SScrollBox::Slot().Padding(16, 4, 8, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[ SNew(STextBlock).Text(FText::FromString(TEXT("\x25CF"))).ColorAndOpacity(this, &SBobBotPanel::GetClaudeStatusColor) ]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[ SNew(STextBlock).Text(this, &SBobBotPanel::GetClaudeStatusText) ]
		]

		// Model selector with highlighted active button
		+ SScrollBox::Slot().Padding(16, 6, 8, 2)
		[ SNew(STextBlock).Text(LOCTEXT("ModelLabel", "Model:")).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)) ]

		+ SScrollBox::Slot().Padding(16, 2, 8, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton).Text(LOCTEXT("Sonnet", "Sonnet"))
				.ToolTipText(LOCTEXT("SonnetTip", "Fast and balanced — best for most tasks"))
				.OnClicked(FOnClicked::CreateSP(this, &SBobBotPanel::HandleModelSelected, FString(TEXT("sonnet"))))
				.ButtonColorAndOpacity(this, &SBobBotPanel::GetModelButtonColor, FString(TEXT("sonnet")))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton).Text(LOCTEXT("Opus", "Opus"))
				.ToolTipText(LOCTEXT("OpusTip", "Most capable — best for complex tasks"))
				.OnClicked(FOnClicked::CreateSP(this, &SBobBotPanel::HandleModelSelected, FString(TEXT("opus"))))
				.ButtonColorAndOpacity(this, &SBobBotPanel::GetModelButtonColor, FString(TEXT("opus")))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("Haiku", "Haiku"))
				.ToolTipText(LOCTEXT("HaikuTip", "Fastest and cheapest — best for simple tasks"))
				.OnClicked(FOnClicked::CreateSP(this, &SBobBotPanel::HandleModelSelected, FString(TEXT("haiku"))))
				.ButtonColorAndOpacity(this, &SBobBotPanel::GetModelButtonColor, FString(TEXT("haiku")))
			]
		]

		+ SScrollBox::Slot() [ SNew(SSeparator) ]

		// AI Clients
		+ SScrollBox::Slot().Padding(8, 8, 8, 2)
		[ SNew(STextBlock).Text(LOCTEXT("AIClients", "AI Clients (MCP)")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11)) ]
		+ SScrollBox::Slot() [ MakeClientRow(LOCTEXT("ClaudeCode", "Claude Code"), TEXT("claude"), LOCTEXT("ClaudeDesc", ".mcp.json at project root (auto-generated)")) ]
		+ SScrollBox::Slot() [ MakeClientRow(LOCTEXT("Cursor", "Cursor"), TEXT("cursor"), LOCTEXT("CursorDesc", ".cursor/mcp.json at project root")) ]
		+ SScrollBox::Slot() [ MakeClientRow(LOCTEXT("VSCode", "VS Code Copilot"), TEXT("vscode"), LOCTEXT("VSCodeDesc", ".vscode/mcp.json at project root")) ]
		+ SScrollBox::Slot() [ MakeClientRow(LOCTEXT("Windsurf", "Windsurf"), TEXT("windsurf"), LOCTEXT("WindsurfDesc", "~/.codeium/windsurf/mcp_config.json")) ]

		+ SScrollBox::Slot() [ SNew(SSeparator) ]

		// Prerequisites
		+ SScrollBox::Slot().Padding(8, 8, 8, 2)
		[ SNew(STextBlock).Text(LOCTEXT("Prerequisites", "Prerequisites")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11)) ]
		+ SScrollBox::Slot().Padding(16, 2) [ SNew(STextBlock).Text(this, &SBobBotPanel::GetPythonPrereqText).ColorAndOpacity(this, &SBobBotPanel::GetPythonPrereqColor) ]
		+ SScrollBox::Slot().Padding(16, 2) [ SNew(STextBlock).Text(this, &SBobBotPanel::GetUvPrereqText).ColorAndOpacity(this, &SBobBotPanel::GetUvPrereqColor) ]
		+ SScrollBox::Slot().Padding(16, 2) [ SNew(STextBlock).Text(this, &SBobBotPanel::GetPluginPrereqText).ColorAndOpacity(this, &SBobBotPanel::GetPluginPrereqColor) ]

		+ SScrollBox::Slot() [ SNew(SSeparator) ]

		// Connected Clients
		+ SScrollBox::Slot().Padding(8, 8, 8, 2)
		[ SNew(STextBlock).Text(LOCTEXT("ConnectedClients", "Connected Clients")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11)) ]
		+ SScrollBox::Slot().Padding(16, 2, 8, 8)
		[ SNew(STextBlock).Text(this, &SBobBotPanel::GetConnectedClientsText) ];
}

// -- Connect tab accessors --

FText SBobBotPanel::GetServerStatusText() const
{
	if (bServerRunning)
		return FText::Format(LOCTEXT("ServerRunning", "Server: Running on :{0}  ({1} client(s))"),
			FText::AsNumber(FBobBotConfig::Get().Port), FText::AsNumber(ConnectedClientCount));
	return LOCTEXT("ServerStopped", "Server: Stopped");
}

FSlateColor SBobBotPanel::GetServerStatusColor() const
{
	if (bServerRunning)
		return ConnectedClientCount > 0 ? FSlateColor(FLinearColor::Green) : FSlateColor(FLinearColor::Yellow);
	return FSlateColor(FLinearColor::Red);
}

FText SBobBotPanel::GetClaudeStatusText() const
{
	const FBobBotConfig& Config = FBobBotConfig::Get();
	if (!Config.bClaudeCodeAvailable)
		return LOCTEXT("ClaudeNotFound", "Claude Code not found. Install: npm i -g @anthropic-ai/claude-code");
	if (!Config.bClaudeAuthenticated)
		return FText::Format(LOCTEXT("ClaudeNotAuth", "Claude Code {0} — not authenticated. Run 'claude login' in terminal."),
			FText::FromString(Config.ClaudeCodeVersion));
	return FText::Format(LOCTEXT("ClaudeReady", "Claude Code {0} — authenticated"),
		FText::FromString(Config.ClaudeCodeVersion));
}

FSlateColor SBobBotPanel::GetClaudeStatusColor() const
{
	const FBobBotConfig& Config = FBobBotConfig::Get();
	if (!Config.bClaudeCodeAvailable) return FSlateColor(FLinearColor::Red);
	if (!Config.bClaudeAuthenticated) return FSlateColor(FLinearColor::Yellow);
	return FSlateColor(FLinearColor::Green);
}

FSlateColor SBobBotPanel::GetModelButtonColor(FString ModelName) const
{
	return FSlateColor(FBobBotConfig::Get().ChatModel == ModelName
		? FLinearColor(0.1f, 0.5f, 0.9f, 1.f)   // bright blue for selected
		: FLinearColor(0.15f, 0.15f, 0.15f, 1.f));  // dark for unselected
}

FText SBobBotPanel::GetPythonPrereqText() const
{
	const FBobBotConfig& Config = FBobBotConfig::Get();
	return Config.bPythonAvailable
		? FText::Format(LOCTEXT("PythonOK", "\x2713  Python ({0})"), FText::FromString(Config.PythonVersion))
		: LOCTEXT("PythonMissing", "\x2717  Python not found");
}
FText SBobBotPanel::GetUvPrereqText() const
{
	const FBobBotConfig& Config = FBobBotConfig::Get();
	return Config.bUvAvailable
		? FText::Format(LOCTEXT("UvOK", "\x2713  uv ({0})"), FText::FromString(Config.UvVersion))
		: LOCTEXT("UvMissing", "\x2717  uv not found");
}
FText SBobBotPanel::GetPluginPrereqText() const
{
	return FBobBotConfig::Get().bPythonPluginAvailable
		? LOCTEXT("PluginOK", "\x2713  PythonScriptPlugin") : LOCTEXT("PluginMissing", "\x2717  PythonScriptPlugin");
}
FSlateColor SBobBotPanel::GetPythonPrereqColor() const { return FSlateColor(FBobBotConfig::Get().bPythonAvailable ? FLinearColor::Green : FLinearColor::Red); }
FSlateColor SBobBotPanel::GetUvPrereqColor() const { return FSlateColor(FBobBotConfig::Get().bUvAvailable ? FLinearColor::Green : FLinearColor::Red); }
FSlateColor SBobBotPanel::GetPluginPrereqColor() const { return FSlateColor(FBobBotConfig::Get().bPythonPluginAvailable ? FLinearColor::Green : FLinearColor::Red); }

FText SBobBotPanel::GetConnectedClientsText() const
{
	if (!bServerRunning) return LOCTEXT("NoServer", "Server not running");
	if (ConnectedClientCount == 0) return LOCTEXT("NoClients", "No clients connected");
	return FText::Format(LOCTEXT("ClientCount", "{0} client(s) connected"), FText::AsNumber(ConnectedClientCount));
}

FReply SBobBotPanel::HandleCopyConfig(FString ClientName)
{
	FBobBotModule& Module = FModuleManager::GetModuleChecked<FBobBotModule>("BobBot");
	FPlatformApplicationMisc::ClipboardCopy(*Module.GenerateClientConfig(ClientName));
	AddChatMessage(FChatMessage::ESender::System, FString::Printf(TEXT("Config for %s copied to clipboard."), *ClientName));
	return FReply::Handled();
}

FReply SBobBotPanel::HandleWriteConfig(FString ClientName)
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
		AddChatMessage(FChatMessage::ESender::System, FString::Printf(TEXT("Config written to: %s"), *TargetPath));
	else
		AddChatMessage(FChatMessage::ESender::System, FString::Printf(TEXT("Failed to write config to: %s"), *TargetPath));
	return FReply::Handled();
}

FReply SBobBotPanel::HandleModelSelected(FString ModelName)
{
	FBobBotConfig& Config = FBobBotConfig::Get();
	Config.ChatModel = ModelName;
	Config.Save();

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (PythonPlugin)
		PythonPlugin->ExecPythonCommand(*FString::Printf(TEXT("import bob_chat; bob_chat.set_model('%s')"), *ModelName));

	AddChatMessage(FChatMessage::ESender::System, FString::Printf(TEXT("Model set to: %s"), *ModelName));
	return FReply::Handled();
}

// --------------------------------------------------------------------------- //
// Chat Tab
// --------------------------------------------------------------------------- //

TSharedRef<SWidget> SBobBotPanel::BuildChatTab()
{
	return SNew(SVerticalBox)

		// Model indicator at top
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[
			SNew(STextBlock)
			.Text(this, &SBobBotPanel::GetSendButtonText)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
		]

		// Chat message history
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(ChatScrollBox, SScrollBox)
			+ SScrollBox::Slot()
			[ SAssignNew(ChatMessagesBox, SVerticalBox) ]
		]

		+ SVerticalBox::Slot().AutoHeight() [ SNew(SSeparator) ]

		// Input area
		+ SVerticalBox::Slot().AutoHeight().Padding(4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SAssignNew(CommandInput, SMultiLineEditableTextBox)
				.HintText(LOCTEXT("ChatHint", "Ask BobBot anything... (Enter to send, Shift+Enter for newline)"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.AutoWrapText(true)
				.OnKeyDownHandler_Lambda([this](const FGeometry&, const FKeyEvent& KeyEvent) -> FReply
				{
					// Enter to send (without Shift)
					if (KeyEvent.GetKey() == EKeys::Enter && !KeyEvent.IsShiftDown())
					{
						OnSendClicked();
						return FReply::Handled();
					}
					return FReply::Unhandled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0).VAlign(VAlign_Bottom)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(SendButton, SButton)
					.Text(LOCTEXT("SendBtn", "Send"))
					.OnClicked(this, &SBobBotPanel::OnSendClicked)
					.IsEnabled(this, &SBobBotPanel::IsSendEnabled)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
				[
					SNew(SButton).Text(LOCTEXT("ClearBtn", "Clear"))
					.OnClicked(this, &SBobBotPanel::OnClearChatClicked)
				]
			]
		];
}

// -- Send button state --

bool SBobBotPanel::IsSendEnabled() const
{
	return !bAiThinking;
}

FText SBobBotPanel::GetSendButtonText() const
{
	if (bAiThinking)
	{
		FString Dots;
		for (int32 i = 0; i < (ThinkingDotCount % 3) + 1; i++)
			Dots += TEXT(".");
		return FText::Format(LOCTEXT("ThinkingModel", "Using {0} — thinking{1}"),
			FText::FromString(FBobBotConfig::Get().ChatModel), FText::FromString(Dots));
	}
	return FText::Format(LOCTEXT("ReadyModel", "Using {0} — ready"),
		FText::FromString(FBobBotConfig::Get().ChatModel));
}

// -- Chat messages --

void SBobBotPanel::AddChatMessage(FChatMessage::ESender Sender, const FString& Content, float Cost, int32 DurationMs, int32 NumTurns)
{
	FChatMessage Msg;
	Msg.Sender = Sender;
	Msg.Content = Content;
	Msg.Timestamp = FDateTime::Now();
	Msg.Cost = Cost;
	Msg.DurationMs = DurationMs;
	Msg.NumTurns = NumTurns;
	ChatHistory.Add(MoveTemp(Msg));
	RebuildChatMessages();
}

void SBobBotPanel::RebuildChatMessages()
{
	if (!ChatMessagesBox.IsValid()) return;
	ChatMessagesBox->ClearChildren();

	for (const FChatMessage& Msg : ChatHistory)
	{
		FString SenderLabel;
		FLinearColor SenderColor;

		switch (Msg.Sender)
		{
		case FChatMessage::ESender::User:
			SenderLabel = TEXT("[You]");
			SenderColor = FLinearColor(0.3f, 0.6f, 1.0f);
			break;
		case FChatMessage::ESender::Bot:
			SenderLabel = TEXT("[BobBot]");
			SenderColor = FLinearColor(0.3f, 0.9f, 0.3f);
			break;
		case FChatMessage::ESender::System:
			SenderLabel = TEXT("[System]");
			SenderColor = FLinearColor(0.6f, 0.6f, 0.6f);
			break;
		}

		FString TimeStr = Msg.Timestamp.ToString(TEXT("%H:%M:%S"));

		TSharedRef<SVerticalBox> MessageBox = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock).Text(FText::FromString(SenderLabel))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.ColorAndOpacity(FSlateColor(SenderColor))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
				[
					SNew(STextBlock).Text(FText::FromString(TimeStr))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f)))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4, 2, 0, 4)
			[
				SNew(STextBlock).Text(FText::FromString(Msg.Content))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.AutoWrapText(true)
			];

		// Cost/duration line for bot messages
		if (Msg.Sender == FChatMessage::ESender::Bot && Msg.Cost > 0.f)
		{
			FString CostLine = FString::Printf(TEXT("$%.4f"), Msg.Cost);
			if (Msg.DurationMs > 0)
				CostLine += FString::Printf(TEXT(" \x00B7 %.1fs"), Msg.DurationMs / 1000.f);
			if (Msg.NumTurns > 1)
				CostLine += FString::Printf(TEXT(" \x00B7 %d turns"), Msg.NumTurns);

			MessageBox->AddSlot().AutoHeight().Padding(4, 0, 0, 6)
			[
				SNew(STextBlock).Text(FText::FromString(CostLine))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f)))
			];
		}

		ChatMessagesBox->AddSlot().AutoHeight().Padding(8, 4)
		[
			MessageBox
		];
	}

	if (ChatScrollBox.IsValid())
		ChatScrollBox->ScrollToEnd();
}

FReply SBobBotPanel::OnSendClicked()
{
	if (!CommandInput.IsValid() || bAiThinking) return FReply::Handled();

	FString Message = CommandInput->GetText().ToString().TrimStartAndEnd();
	if (Message.IsEmpty()) return FReply::Handled();

	const FBobBotConfig& Config = FBobBotConfig::Get();
	if (!Config.bClaudeCodeAvailable)
	{
		AddChatMessage(FChatMessage::ESender::System, TEXT("Claude Code not found. Install it first."));
		return FReply::Handled();
	}
	if (!Config.bClaudeAuthenticated)
	{
		AddChatMessage(FChatMessage::ESender::System, TEXT("Not authenticated. Run 'claude login' in a terminal."));
		return FReply::Handled();
	}

	AddChatMessage(FChatMessage::ESender::User, Message);
	CommandInput->SetText(FText::GetEmpty());

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin)
	{
		AddChatMessage(FChatMessage::ESender::System, TEXT("PythonScriptPlugin not available."));
		return FReply::Handled();
	}

	FString TempDir = FPaths::ProjectSavedDir() / TEXT("BobBot");
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	PF.CreateDirectoryTree(*TempDir);

	FString MsgFile = TempDir / TEXT("_chat_msg.txt");
	FFileHelper::SaveStringToFile(Message, *MsgFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	FString Script = FString::Printf(
		TEXT("import bob_chat; bob_chat.send_message(open(r'%s', encoding='utf-8').read())"),
		*MsgFile.Replace(TEXT("\\"), TEXT("/")));

	PythonPlugin->ExecPythonCommand(*Script);
	bAiThinking = true;
	ThinkingDotCount = 0;
	ThinkingAnimTimer = 0.f;

	return FReply::Handled();
}

FReply SBobBotPanel::OnClearChatClicked()
{
	ChatHistory.Empty();
	RebuildChatMessages();

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (PythonPlugin)
		PythonPlugin->ExecPythonCommand(TEXT("import bob_chat; bob_chat.clear_session()"));

	AddChatMessage(FChatMessage::ESender::System, TEXT("Chat cleared."));
	return FReply::Handled();
}

// --------------------------------------------------------------------------- //
// Thinking indicator animation
// --------------------------------------------------------------------------- //

void SBobBotPanel::UpdateThinkingIndicator()
{
	if (bAiThinking && !bWasThinking)
	{
		// Just started thinking — no extra message needed, header shows it
		bWasThinking = true;
	}
	else if (!bAiThinking && bWasThinking)
	{
		// Stopped thinking
		bWasThinking = false;
	}
}

// --------------------------------------------------------------------------- //
// Tick / Polling
// --------------------------------------------------------------------------- //

void SBobBotPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Server status poll (every 2s)
	StatusPollTimer += InDeltaTime;
	if (StatusPollTimer >= 2.0f)
	{
		StatusPollTimer = 0.f;
		PollServerStatus();
	}

	// Chat poll (100ms while thinking, 1s idle)
	ChatPollTimer += InDeltaTime;
	float ChatPollInterval = bAiThinking ? 0.1f : 1.0f;
	if (ChatPollTimer >= ChatPollInterval)
	{
		ChatPollTimer = 0.f;
		PollChatUpdates();
	}

	// Animate thinking dots (cycle every 0.5s)
	if (bAiThinking)
	{
		ThinkingAnimTimer += InDeltaTime;
		if (ThinkingAnimTimer >= 0.5f)
		{
			ThinkingAnimTimer = 0.f;
			ThinkingDotCount++;
		}
	}

	UpdateThinkingIndicator();
}

void SBobBotPanel::PollServerStatus()
{
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin) { bServerRunning = false; ConnectedClientCount = 0; return; }

	FString TempDir = FPaths::ProjectSavedDir() / TEXT("BobBot");
	FString OutputFile = TempDir / TEXT("_status_output.txt");

	FString Script = FString::Printf(
		TEXT("try:\n")
		TEXT("    import bob_mcp_server, json\n")
		TEXT("    open(r'%s', 'w').write(json.dumps(bob_mcp_server.get_status()))\n")
		TEXT("except:\n")
		TEXT("    open(r'%s', 'w').write('{\"running\":false,\"client_count\":0}')\n"),
		*OutputFile.Replace(TEXT("\\"), TEXT("/")),
		*OutputFile.Replace(TEXT("\\"), TEXT("/")));

	PythonPlugin->ExecPythonCommand(*Script);

	FString StatusJson;
	if (FFileHelper::LoadFileToString(StatusJson, *OutputFile))
	{
		bServerRunning = StatusJson.Contains(TEXT("\"running\": true")) || StatusJson.Contains(TEXT("\"running\":true"));
		int32 Idx = StatusJson.Find(TEXT("\"client_count\":"));
		if (Idx == INDEX_NONE) Idx = StatusJson.Find(TEXT("\"client_count\": "));
		if (Idx != INDEX_NONE)
		{
			FString After = StatusJson.Mid(Idx);
			int32 ColonIdx = After.Find(TEXT(":"));
			if (ColonIdx != INDEX_NONE)
				ConnectedClientCount = FCString::Atoi(*After.Mid(ColonIdx + 1).TrimStart());
		}
	}

	FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*OutputFile);
}

void SBobBotPanel::PollChatUpdates()
{
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin) return;

	FString TempDir = FPaths::ProjectSavedDir() / TEXT("BobBot");
	FString PollFile = TempDir / TEXT("_chat_poll.txt");

	FString Script = FString::Printf(
		TEXT("import bob_chat, json\n")
		TEXT("open(r'%s', 'w', encoding='utf-8').write(json.dumps(bob_chat.poll()))\n"),
		*PollFile.Replace(TEXT("\\"), TEXT("/")));

	PythonPlugin->ExecPythonCommand(*Script);

	FString PollJson;
	if (!FFileHelper::LoadFileToString(PollJson, *PollFile)) return;
	FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*PollFile);

	if (PollJson.IsEmpty() || PollJson == TEXT("{}")) return;

	TSharedPtr<FJsonObject> Result;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PollJson);
	if (!FJsonSerializer::Deserialize(Reader, Result) || !Result.IsValid()) return;

	// Response text
	FString ResponseText;
	if (Result->TryGetStringField(TEXT("response"), ResponseText) && !ResponseText.IsEmpty())
	{
		float Cost = 0.f;
		int32 DurationMs = 0;
		int32 NumTurns = 1;
		Result->TryGetNumberField(TEXT("cost"), Cost);

		double DurationDbl = 0;
		if (Result->TryGetNumberField(TEXT("duration_ms"), DurationDbl))
			DurationMs = static_cast<int32>(DurationDbl);

		double TurnsDbl = 1;
		if (Result->TryGetNumberField(TEXT("num_turns"), TurnsDbl))
			NumTurns = static_cast<int32>(TurnsDbl);

		AddChatMessage(FChatMessage::ESender::Bot, ResponseText, Cost, DurationMs, NumTurns);
	}

	// Error
	FString ErrorText;
	if (Result->TryGetStringField(TEXT("error"), ErrorText) && !ErrorText.IsEmpty())
	{
		AddChatMessage(FChatMessage::ESender::System, FString::Printf(TEXT("Error: %s"), *ErrorText));
	}

	// Thinking state
	bool bThinking = false;
	Result->TryGetBoolField(TEXT("thinking"), bThinking);
	bAiThinking = bThinking;
}

#undef LOCTEXT_NAMESPACE
