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
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IPythonScriptPlugin.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "SBobBotPanel"

/** Typedef to avoid angle-bracket issues inside Slate macros. */
using SSpinBox_Int32 = SSpinBox<int32>;

// =========================================================================== //
// Helpers
// =========================================================================== //

namespace BobBotUI
{
	static const FLinearColor Green(0.2f, 0.8f, 0.2f);
	static const FLinearColor Yellow(0.9f, 0.8f, 0.1f);
	static const FLinearColor Red(0.9f, 0.2f, 0.2f);
	static const FLinearColor DimGray(0.5f, 0.5f, 0.5f);
	static const FLinearColor LightGray(0.6f, 0.6f, 0.6f);
	static const FLinearColor Blue(0.3f, 0.6f, 1.0f);
	static const FLinearColor BotGreen(0.3f, 0.9f, 0.3f);
	static const FLinearColor ErrorOrange(1.0f, 0.5f, 0.1f);
	static const FLinearColor ActiveBlue(0.1f, 0.5f, 0.9f, 1.f);
	static const FLinearColor InactiveDark(0.15f, 0.15f, 0.15f, 1.f);
	static const FLinearColor SectionBg(0.02f, 0.02f, 0.02f, 0.5f);

	/** Make a section heading label. */
	static TSharedRef<SWidget> SectionHeading(const FText& Label)
	{
		return SNew(STextBlock).Text(Label)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			.ColorAndOpacity(FSlateColor(LightGray));
	}

	/** Make a key-value row: "Label    Value" with optional color. */
	static TSharedRef<SWidget> KeyValueRow(const FText& Key, TAttribute<FText> Value, TAttribute<FSlateColor> ValueColor = FSlateColor(FLinearColor::White))
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
			[
				SNew(STextBlock).Text(Key)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.ColorAndOpacity(FSlateColor(DimGray))
				.MinDesiredWidth(110.f)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(STextBlock).Text(Value)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.ColorAndOpacity(ValueColor)
			];
	}
}

// =========================================================================== //
// Construction
// =========================================================================== //

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

// =========================================================================== //
// Tab switching
// =========================================================================== //

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

// =========================================================================== //
// Claude CLI detection
// =========================================================================== //

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

// =========================================================================== //
// Connect Tab — SETUP section helpers
// =========================================================================== //

FText SBobBotPanel::GetClaudeInstallStatusText() const
{
	const FBobBotConfig& Config = FBobBotConfig::Get();
	if (Config.bClaudeCodeAvailable)
		return FText::Format(LOCTEXT("ClaudeInstalled", "Installed ({0})"), FText::FromString(Config.ClaudeCodeVersion));
	return LOCTEXT("ClaudeNotInstalled", "Not installed");
}

FSlateColor SBobBotPanel::GetClaudeInstallStatusColor() const
{
	return FSlateColor(FBobBotConfig::Get().bClaudeCodeAvailable ? BobBotUI::Green : BobBotUI::Red);
}

FText SBobBotPanel::GetAuthStatusText() const
{
	const FBobBotConfig& Config = FBobBotConfig::Get();
	if (!Config.bClaudeCodeAvailable) return LOCTEXT("AuthNA", "—");
	return Config.bClaudeAuthenticated
		? LOCTEXT("AuthSignedIn", "Signed in")
		: LOCTEXT("AuthNotSignedIn", "Not signed in");
}

FSlateColor SBobBotPanel::GetAuthStatusColor() const
{
	const FBobBotConfig& Config = FBobBotConfig::Get();
	if (!Config.bClaudeCodeAvailable) return FSlateColor(BobBotUI::DimGray);
	return FSlateColor(Config.bClaudeAuthenticated ? BobBotUI::Green : BobBotUI::Red);
}

bool SBobBotPanel::IsReadyToChat() const
{
	const FBobBotConfig& Config = FBobBotConfig::Get();
	return Config.bClaudeCodeAvailable && Config.bClaudeAuthenticated;
}

FText SBobBotPanel::GetReadyStatusText() const
{
	if (IsReadyToChat()) return LOCTEXT("Ready", "Ready to chat!");
	if (!FBobBotConfig::Get().bClaudeCodeAvailable) return LOCTEXT("NeedInstall", "Install Claude Code first");
	return LOCTEXT("NeedAuth", "Run 'claude login' in terminal");
}

FSlateColor SBobBotPanel::GetReadyStatusColor() const
{
	return FSlateColor(IsReadyToChat() ? BobBotUI::Green : BobBotUI::Yellow);
}

FReply SBobBotPanel::HandleRefreshStatus()
{
	DetectClaudeCli();
	return FReply::Handled();
}

FReply SBobBotPanel::HandleGoToChat()
{
	return OnTabClicked(EBobBotTab::Chat);
}

// =========================================================================== //
// Connect Tab — Model helpers
// =========================================================================== //

FSlateColor SBobBotPanel::GetModelButtonColor(FString ModelName) const
{
	return FSlateColor(FBobBotConfig::Get().ChatModel == ModelName ? BobBotUI::ActiveBlue : BobBotUI::InactiveDark);
}

FReply SBobBotPanel::HandleModelSelected(FString ModelName)
{
	FBobBotConfig& Config = FBobBotConfig::Get();
	Config.ChatModel = ModelName;
	Config.Save();

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (PythonPlugin)
		PythonPlugin->ExecPythonCommand(*FString::Printf(TEXT("import bob_chat; bob_chat.set_model('%s')"), *ModelName));

	return FReply::Handled();
}

// =========================================================================== //
// Connect Tab — Chat Session helpers
// =========================================================================== //

FText SBobBotPanel::GetSessionModelText() const
{
	return FText::FromString(FBobBotConfig::Get().ChatModel);
}

FText SBobBotPanel::GetSessionCostText() const
{
	return FText::FromString(FString::Printf(TEXT("$%.2f"), TotalSessionCost));
}

FText SBobBotPanel::GetSessionMessageCountText() const
{
	return FText::AsNumber(SessionMessageCount);
}

// =========================================================================== //
// Connect Tab — Advanced section helpers
// =========================================================================== //

FReply SBobBotPanel::HandleToggleAdvanced()
{
	bAdvancedExpanded = !bAdvancedExpanded;
	if (AdvancedSection.IsValid())
		AdvancedSection->SetVisibility(bAdvancedExpanded ? EVisibility::Visible : EVisibility::Collapsed);
	return FReply::Handled();
}

FText SBobBotPanel::GetAdvancedToggleText() const
{
	return bAdvancedExpanded ? LOCTEXT("CollapseAdv", "\x25BC  Advanced") : LOCTEXT("ExpandAdv", "\x25B6  Advanced");
}

// Permission mode
FReply SBobBotPanel::HandlePermissionModeChanged(EBobBotPermissionMode Mode)
{
	FBobBotConfig& Config = FBobBotConfig::Get();
	Config.PermissionMode = Mode;
	Config.Save();
	Config.ApplyEnvironmentVars();

	// Update Python's os.environ and the server module directly
	// (FPlatformMisc::SetEnvironmentVar updates OS env but not Python's cached os.environ)
	const TCHAR* ModeStr = TEXT("allow_always");
	if (Mode == EBobBotPermissionMode::AskMe) ModeStr = TEXT("ask_me");
	else if (Mode == EBobBotPermissionMode::ChatOnly) ModeStr = TEXT("chat_only");

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (PythonPlugin)
	{
		PythonPlugin->ExecPythonCommand(*FString::Printf(
			TEXT("import os; os.environ['BOB_PERMISSION_MODE'] = '%s'"), ModeStr));
	}

	return FReply::Handled();
}

ECheckBoxState SBobBotPanel::GetPermissionCheckState(EBobBotPermissionMode Mode) const
{
	return FBobBotConfig::Get().PermissionMode == Mode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

// System prompt
FReply SBobBotPanel::HandleSaveSystemPrompt()
{
	if (!SystemPromptEditor.IsValid()) return FReply::Handled();
	FBobBotConfig& Config = FBobBotConfig::Get();
	Config.SystemPrompt = SystemPromptEditor->GetText().ToString();
	Config.Save();

	// Write to the prompt file for bob_chat.py to pick up
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (PythonPlugin && !Config.SystemPrompt.IsEmpty())
	{
		FString PromptDir = FPaths::ProjectSavedDir() / TEXT("BobBot");
		FString PromptFile = PromptDir / TEXT("_system_prompt.txt");
		FFileHelper::SaveStringToFile(Config.SystemPrompt, *PromptFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	AddChatMessage(FChatMessage::ESender::System, TEXT("System prompt saved."));
	return FReply::Handled();
}

FReply SBobBotPanel::HandleResetSystemPrompt()
{
	FBobBotConfig& Config = FBobBotConfig::Get();
	Config.SystemPrompt.Empty();
	Config.Save();

	if (SystemPromptEditor.IsValid())
		SystemPromptEditor->SetText(FText::GetEmpty());

	// Delete the custom prompt file so bob_chat.py uses its default
	FString PromptFile = FPaths::ProjectSavedDir() / TEXT("BobBot") / TEXT("_system_prompt.txt");
	FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*PromptFile);

	AddChatMessage(FChatMessage::ESender::System, TEXT("System prompt reset to default."));
	return FReply::Handled();
}

// CLAUDE.md
FReply SBobBotPanel::HandleSaveClaudeMd()
{
	if (!ClaudeMdEditor.IsValid()) return FReply::Handled();
	FString Content = ClaudeMdEditor->GetText().ToString();
	FString Path = FPaths::ProjectDir() / TEXT("CLAUDE.md");
	if (FFileHelper::SaveStringToFile(Content, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		AddChatMessage(FChatMessage::ESender::System, TEXT("CLAUDE.md saved."));
	else
		AddChatMessage(FChatMessage::ESender::Error, TEXT("Failed to save CLAUDE.md."));
	return FReply::Handled();
}

FReply SBobBotPanel::HandleLoadClaudeMd()
{
	FString Path = FPaths::ProjectDir() / TEXT("CLAUDE.md");
	FString Content;
	if (FFileHelper::LoadFileToString(Content, *Path) && ClaudeMdEditor.IsValid())
		ClaudeMdEditor->SetText(FText::FromString(Content));
	else if (ClaudeMdEditor.IsValid())
		ClaudeMdEditor->SetText(FText::GetEmpty());
	return FReply::Handled();
}

// Other editors (MCP config) — single Setup/Regenerate button
bool SBobBotPanel::DoesClientConfigExist(const FString& ClientName) const
{
	FString TargetPath;
	if (ClientName == TEXT("claude")) TargetPath = FPaths::ProjectDir() / TEXT(".mcp.json");
	else if (ClientName == TEXT("cursor")) TargetPath = FPaths::ProjectDir() / TEXT(".cursor") / TEXT("mcp.json");
	else if (ClientName == TEXT("vscode")) TargetPath = FPaths::ProjectDir() / TEXT(".vscode") / TEXT("mcp.json");
	else if (ClientName == TEXT("windsurf")) TargetPath = FString(FPlatformProcess::UserHomeDir()) / TEXT(".codeium") / TEXT("windsurf") / TEXT("mcp_config.json");
	if (TargetPath.IsEmpty()) return false;
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*TargetPath);
}

FText SBobBotPanel::GetClientStatusText(FString ClientName) const
{
	return DoesClientConfigExist(ClientName)
		? LOCTEXT("Configured", "Configured")
		: LOCTEXT("NotConfigured", "Not configured");
}

FSlateColor SBobBotPanel::GetClientStatusColor(FString ClientName) const
{
	return FSlateColor(DoesClientConfigExist(ClientName) ? BobBotUI::Green : BobBotUI::DimGray);
}

FReply SBobBotPanel::HandleSetupClient(FString ClientName)
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
		AddChatMessage(FChatMessage::ESender::System, FString::Printf(TEXT("%s configured: %s"), *ClientName, *TargetPath));
	else
		AddChatMessage(FChatMessage::ESender::Error, FString::Printf(TEXT("Failed to configure %s"), *ClientName));
	return FReply::Handled();
}

// Paths
FText SBobBotPanel::GetProjectRootPath() const
{
	FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	bool bExists = FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*Root);
	return FText::FromString(FString::Printf(TEXT("%s   %s"), *Root, bExists ? TEXT("exists") : TEXT("MISSING")));
}

FText SBobBotPanel::GetMcpJsonPath() const
{
	FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT(".mcp.json"));
	bool bExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*Path);
	return FText::FromString(FString::Printf(TEXT("%s   %s"), *Path, bExists ? TEXT("exists") : TEXT("MISSING")));
}

FText SBobBotPanel::GetBridgePath() const
{
	FString Path = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectPluginsDir() / TEXT("BobBot") / TEXT("Scripts") / TEXT("bob_mcp_bridge.py"));
	bool bExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*Path);
	return FText::FromString(FString::Printf(TEXT("%s   %s"), *Path, bExists ? TEXT("exists") : TEXT("MISSING")));
}

FText SBobBotPanel::GetPromptFilePath() const
{
	FString Path = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("BobBot") / TEXT("_system_prompt.txt"));
	bool bExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*Path);
	return FText::FromString(FString::Printf(TEXT("%s   %s"), *Path, bExists ? TEXT("exists") : TEXT("MISSING")));
}

// Prerequisites
FText SBobBotPanel::GetPythonPrereqText() const
{
	const FBobBotConfig& Config = FBobBotConfig::Get();
	return Config.bPythonAvailable
		? FText::Format(LOCTEXT("PythonOK", "Python {0}"), FText::FromString(Config.PythonVersion))
		: LOCTEXT("PythonMissing", "Python not found");
}
FText SBobBotPanel::GetUvPrereqText() const
{
	const FBobBotConfig& Config = FBobBotConfig::Get();
	return Config.bUvAvailable
		? FText::Format(LOCTEXT("UvOK", "uv {0}"), FText::FromString(Config.UvVersion))
		: LOCTEXT("UvMissing", "uv not found");
}
FText SBobBotPanel::GetPluginPrereqText() const
{
	return FBobBotConfig::Get().bPythonPluginAvailable
		? LOCTEXT("PluginOK", "PythonScriptPlugin") : LOCTEXT("PluginMissing", "PythonScriptPlugin missing");
}
FSlateColor SBobBotPanel::GetPythonPrereqColor() const { return FSlateColor(FBobBotConfig::Get().bPythonAvailable ? BobBotUI::Green : BobBotUI::Red); }
FSlateColor SBobBotPanel::GetUvPrereqColor() const { return FSlateColor(FBobBotConfig::Get().bUvAvailable ? BobBotUI::Green : BobBotUI::Red); }
FSlateColor SBobBotPanel::GetPluginPrereqColor() const { return FSlateColor(FBobBotConfig::Get().bPythonPluginAvailable ? BobBotUI::Green : BobBotUI::Red); }

// =========================================================================== //
// Connect Tab — BUILD
// =========================================================================== //

TSharedRef<SWidget> SBobBotPanel::BuildConnectTab()
{
	const FBobBotConfig& Config = FBobBotConfig::Get();

	// -- Build a model button with subtitle --
	auto MakeModelButton = [this](const FText& Name, const FText& Subtitle, const FString& ModelKey) -> TSharedRef<SWidget>
	{
		FString Key = ModelKey;
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SButton).Text(Name)
				.OnClicked(FOnClicked::CreateSP(this, &SBobBotPanel::HandleModelSelected, Key))
				.ButtonColorAndOpacity(this, &SBobBotPanel::GetModelButtonColor, Key)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(Subtitle)
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
				.ColorAndOpacity(FSlateColor(BobBotUI::DimGray))
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
				.Text(this, &SBobBotPanel::GetClientStatusText, Key)
				.ColorAndOpacity(this, &SBobBotPanel::GetClientStatusColor, Key)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text_Lambda([this, Key]() { return DoesClientConfigExist(Key) ? LOCTEXT("Regenerate", "Regenerate") : LOCTEXT("Setup", "Setup"); })
				.OnClicked(FOnClicked::CreateSP(this, &SBobBotPanel::HandleSetupClient, Key))
			];
	};

	// -- Advanced section content --
	TSharedRef<SVerticalBox> AdvancedContent = SNew(SVerticalBox)

		// TOOL PERMISSIONS
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 4) [ BobBotUI::SectionHeading(LOCTEXT("ToolPerms", "TOOL PERMISSIONS")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "RadioButton")
			.IsChecked(this, &SBobBotPanel::GetPermissionCheckState, EBobBotPermissionMode::AllowAlways)
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
			.IsChecked(this, &SBobBotPanel::GetPermissionCheckState, EBobBotPermissionMode::AskMe)
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
			.IsChecked(this, &SBobBotPanel::GetPermissionCheckState, EBobBotPermissionMode::ChatOnly)
			.OnCheckStateChanged_Lambda([this](ECheckBoxState) { HandlePermissionModeChanged(EBobBotPermissionMode::ChatOnly); })
			[
				SNew(STextBlock).Text(LOCTEXT("ChatOnly", "Chat Only \x2014 Claude answers only, no tool access"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8) [ SNew(SSeparator) ]

		// SERVER
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBotUI::SectionHeading(LOCTEXT("ServerSection", "SERVER")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("PortLabel", "Port:")) ]
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
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("AutoStart", "Auto-start:")) ]
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
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("AutoGen", "Auto-generate:")) ]
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
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("MaxClients", "Max Clients:")) ]
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
			+ SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center) [ SNew(STextBlock).Text(LOCTEXT("RateLimit", "Rate Limit:")) ]
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
				[ SNew(STextBlock).Text(LOCTEXT("MsgPerSec", "msg/s")).ColorAndOpacity(FSlateColor(BobBotUI::DimGray)) ]
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8) [ SNew(SSeparator) ]

		// SYSTEM PROMPT
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBotUI::SectionHeading(LOCTEXT("SysPromptSection", "SYSTEM PROMPT")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2).MaxHeight(120.f)
		[
			SAssignNew(SystemPromptEditor, SMultiLineEditableTextBox)
			.Text(FText::FromString(Config.SystemPrompt))
			.HintText(LOCTEXT("SysPromptHint", "Leave empty for default BobBot prompt..."))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0) [ SNew(SButton).Text(LOCTEXT("SavePrompt", "Save")).OnClicked(this, &SBobBotPanel::HandleSaveSystemPrompt) ]
			+ SHorizontalBox::Slot().AutoWidth() [ SNew(SButton).Text(LOCTEXT("ResetPrompt", "Reset to Default")).OnClicked(this, &SBobBotPanel::HandleResetSystemPrompt) ]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8) [ SNew(SSeparator) ]

		// PROJECT CONTEXT (CLAUDE.md)
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBotUI::SectionHeading(LOCTEXT("ClaudeMdSection", "PROJECT CONTEXT (CLAUDE.md)")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2).MaxHeight(120.f)
		[
			SAssignNew(ClaudeMdEditor, SMultiLineEditableTextBox)
			.HintText(LOCTEXT("ClaudeMdHint", "Project context for Claude..."))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0) [ SNew(SButton).Text(LOCTEXT("SaveClaudeMd", "Save")).OnClicked(this, &SBobBotPanel::HandleSaveClaudeMd) ]
			+ SHorizontalBox::Slot().AutoWidth() [ SNew(SButton).Text(LOCTEXT("LoadClaudeMd", "Load from CLAUDE.md")).OnClicked(this, &SBobBotPanel::HandleLoadClaudeMd) ]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8) [ SNew(SSeparator) ]

		// USE IN OTHER EDITORS
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBotUI::SectionHeading(LOCTEXT("OtherEditors", "USE IN OTHER EDITORS")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2) [ MakeEditorRow(LOCTEXT("ClaudeCodeLabel", "Claude Code"), TEXT("claude")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2) [ MakeEditorRow(LOCTEXT("CursorLabel", "Cursor"), TEXT("cursor")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2) [ MakeEditorRow(LOCTEXT("VSCodeLabel", "VS Code"), TEXT("vscode")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2) [ MakeEditorRow(LOCTEXT("WindsurfLabel", "Windsurf"), TEXT("windsurf")) ]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8) [ SNew(SSeparator) ]

		// PATHS (read-only, for troubleshooting)
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBotUI::SectionHeading(LOCTEXT("PathsSection", "PATHS")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[ BobBotUI::KeyValueRow(LOCTEXT("ProjRootKey", "Project Root:"), TAttribute<FText>::CreateSP(this, &SBobBotPanel::GetProjectRootPath)) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[ BobBotUI::KeyValueRow(LOCTEXT("McpJsonKey", ".mcp.json:"), TAttribute<FText>::CreateSP(this, &SBobBotPanel::GetMcpJsonPath)) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[ BobBotUI::KeyValueRow(LOCTEXT("BridgeKey", "Bridge:"), TAttribute<FText>::CreateSP(this, &SBobBotPanel::GetBridgePath)) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[ BobBotUI::KeyValueRow(LOCTEXT("PromptKey", "Prompt File:"), TAttribute<FText>::CreateSP(this, &SBobBotPanel::GetPromptFilePath)) ]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8) [ SNew(SSeparator) ]

		// PREREQUISITES
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4) [ BobBotUI::SectionHeading(LOCTEXT("PrereqSection", "PREREQUISITES")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(STextBlock).Text(this, &SBobBotPanel::GetPythonPrereqText)
			.ColorAndOpacity(this, &SBobBotPanel::GetPythonPrereqColor)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2)
		[
			SNew(STextBlock).Text(this, &SBobBotPanel::GetUvPrereqText)
			.ColorAndOpacity(this, &SBobBotPanel::GetUvPrereqColor)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 2, 8, 8)
		[
			SNew(STextBlock).Text(this, &SBobBotPanel::GetPluginPrereqText)
			.ColorAndOpacity(this, &SBobBotPanel::GetPluginPrereqColor)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
		];

	// Load CLAUDE.md if it exists
	HandleLoadClaudeMd();

	// -- Main layout --
	return SNew(SScrollBox)

		// ---- SETUP ----
		+ SScrollBox::Slot().Padding(8, 8, 8, 4) [ BobBotUI::SectionHeading(LOCTEXT("SetupSection", "SETUP")) ]

		+ SScrollBox::Slot().Padding(16, 4)
		[ BobBotUI::KeyValueRow(LOCTEXT("ClaudeCodeKey", "Claude Code"),
			TAttribute<FText>::CreateSP(this, &SBobBotPanel::GetClaudeInstallStatusText),
			TAttribute<FSlateColor>::CreateSP(this, &SBobBotPanel::GetClaudeInstallStatusColor)) ]

		+ SScrollBox::Slot().Padding(16, 4)
		[ BobBotUI::KeyValueRow(LOCTEXT("AuthKey", "Authentication"),
			TAttribute<FText>::CreateSP(this, &SBobBotPanel::GetAuthStatusText),
			TAttribute<FSlateColor>::CreateSP(this, &SBobBotPanel::GetAuthStatusColor)) ]

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
					.ColorAndOpacity(FSlateColor(BobBotUI::DimGray))
					.MinDesiredWidth(110.f)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock)
					.Text(this, &SBobBotPanel::GetReadyStatusText)
					.ColorAndOpacity(this, &SBobBotPanel::GetReadyStatusColor)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
			[
				SNew(SButton).Text(LOCTEXT("Refresh", "Refresh"))
				.OnClicked(this, &SBobBotPanel::HandleRefreshStatus)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
			[
				SNew(SButton).Text(LOCTEXT("GoToChat", "Go to Chat"))
				.OnClicked(this, &SBobBotPanel::HandleGoToChat)
				.IsEnabled_Lambda([this]() { return IsReadyToChat(); })
			]
		]

		+ SScrollBox::Slot().Padding(0, 4) [ SNew(SSeparator) ]

		// ---- MODEL ----
		+ SScrollBox::Slot().Padding(8, 8, 8, 4) [ BobBotUI::SectionHeading(LOCTEXT("ModelSection", "MODEL")) ]

		+ SScrollBox::Slot().Padding(16, 2, 8, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0) [ MakeModelButton(LOCTEXT("Sonnet", "Sonnet"), LOCTEXT("SonnetSub", "Fast"), TEXT("sonnet")) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0) [ MakeModelButton(LOCTEXT("Opus", "Opus"), LOCTEXT("OpusSub", "Best"), TEXT("opus")) ]
			+ SHorizontalBox::Slot().AutoWidth() [ MakeModelButton(LOCTEXT("Haiku", "Haiku"), LOCTEXT("HaikuSub", "Cheapest"), TEXT("haiku")) ]
		]

		+ SScrollBox::Slot().Padding(0, 4) [ SNew(SSeparator) ]

		// ---- CHAT SESSION ----
		+ SScrollBox::Slot().Padding(8, 8, 8, 4) [ BobBotUI::SectionHeading(LOCTEXT("SessionSection", "CHAT SESSION")) ]

		+ SScrollBox::Slot().Padding(16, 2) [ BobBotUI::KeyValueRow(LOCTEXT("SessModelKey", "Model:"), TAttribute<FText>::CreateSP(this, &SBobBotPanel::GetSessionModelText)) ]
		+ SScrollBox::Slot().Padding(16, 2) [ BobBotUI::KeyValueRow(LOCTEXT("SessCostKey", "Session cost:"), TAttribute<FText>::CreateSP(this, &SBobBotPanel::GetSessionCostText)) ]
		+ SScrollBox::Slot().Padding(16, 2, 8, 8) [ BobBotUI::KeyValueRow(LOCTEXT("SessMsgKey", "Messages:"), TAttribute<FText>::CreateSP(this, &SBobBotPanel::GetSessionMessageCountText)) ]

		+ SScrollBox::Slot().Padding(0, 4) [ SNew(SSeparator) ]

		// ---- ADVANCED (collapsed by default) ----
		+ SScrollBox::Slot().Padding(8, 4)
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.OnClicked(this, &SBobBotPanel::HandleToggleAdvanced)
			.ContentPadding(FMargin(0))
			[
				SNew(STextBlock)
				.Text(this, &SBobBotPanel::GetAdvancedToggleText)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(FSlateColor(BobBotUI::LightGray))
			]
		]

		+ SScrollBox::Slot()
		[
			SAssignNew(AdvancedSection, SBox)
			.Visibility(EVisibility::Collapsed)  // hidden by default
			[
				AdvancedContent
			]
		];
}

// =========================================================================== //
// Chat Tab — BUILD
// =========================================================================== //

FText SBobBotPanel::GetChatHeaderText() const
{
	return FText::Format(LOCTEXT("ChatHeader", "Model: {0}  |  Session: {1}  |  {2} messages"),
		FText::FromString(FBobBotConfig::Get().ChatModel),
		FText::FromString(FString::Printf(TEXT("$%.2f"), TotalSessionCost)),
		FText::AsNumber(SessionMessageCount));
}

TSharedRef<SWidget> SBobBotPanel::BuildChatTab()
{
	return SNew(SVerticalBox)

		// Chat header bar: Model | Session cost | Messages | [Clear]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SBobBotPanel::GetChatHeaderText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.ColorAndOpacity(FSlateColor(BobBotUI::DimGray))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton).Text(LOCTEXT("ClearBtn", "Clear"))
				.OnClicked(this, &SBobBotPanel::OnClearChatClicked)
			]
		]

		+ SVerticalBox::Slot().AutoHeight() [ SNew(SSeparator) ]

		// Chat message history
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(ChatScrollBox, SScrollBox)
			+ SScrollBox::Slot()
			[ SAssignNew(ChatMessagesBox, SVerticalBox) ]
		]

		+ SVerticalBox::Slot().AutoHeight() [ SNew(SSeparator) ]

		// Input area with Send and Stop buttons
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
					SAssignNew(StopButton, SButton)
					.Text(LOCTEXT("StopBtn", "Stop"))
					.OnClicked(this, &SBobBotPanel::OnStopClicked)
					.Visibility_Lambda([this]() { return bAiThinking ? EVisibility::Visible : EVisibility::Collapsed; })
					.ButtonColorAndOpacity(FLinearColor(0.8f, 0.2f, 0.2f))
				]
			]
		];
}

// =========================================================================== //
// Send / Clear / Stop
// =========================================================================== //

bool SBobBotPanel::IsSendEnabled() const
{
	return !bAiThinking;
}

bool SBobBotPanel::IsStopVisible() const
{
	return bAiThinking;
}

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

	if (Sender == FChatMessage::ESender::User || Sender == FChatMessage::ESender::Bot)
		SessionMessageCount++;
	if (Sender == FChatMessage::ESender::Bot && Cost > 0.f)
		TotalSessionCost += Cost;

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
			SenderColor = BobBotUI::Blue;
			break;
		case FChatMessage::ESender::Bot:
			SenderLabel = TEXT("[BobBot]");
			SenderColor = BobBotUI::BotGreen;
			break;
		case FChatMessage::ESender::System:
			SenderLabel = TEXT("[System]");
			SenderColor = BobBotUI::DimGray;
			break;
		case FChatMessage::ESender::Error:
			SenderLabel = TEXT("[Error]");
			SenderColor = BobBotUI::ErrorOrange;
			break;
		case FChatMessage::ESender::Approval:
			SenderLabel = TEXT("[Tool Request]");
			SenderColor = BobBotUI::Yellow;
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
				.ColorAndOpacity(FSlateColor(
					Msg.Sender == FChatMessage::ESender::Error ? BobBotUI::ErrorOrange :
					Msg.Sender == FChatMessage::ESender::Approval ? BobBotUI::Yellow :
					FLinearColor::White))
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

		// Approval buttons — only on the most recent Approval message while pending
		if (Msg.Sender == FChatMessage::ESender::Approval && bHasPendingApproval
			&& &Msg == &ChatHistory.Last())
		{
			MessageBox->AddSlot().AutoHeight().Padding(4, 4, 0, 6)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("ApproveBtn", "Approve"))
					.OnClicked(this, &SBobBotPanel::OnApproveClicked)
					.ButtonColorAndOpacity(FLinearColor(0.2f, 0.7f, 0.2f))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("DenyBtn", "Deny"))
					.OnClicked(this, &SBobBotPanel::OnDenyClicked)
					.ButtonColorAndOpacity(FLinearColor(0.7f, 0.2f, 0.2f))
				]
			];
		}

		ChatMessagesBox->AddSlot().AutoHeight().Padding(8, 4)
		[
			MessageBox
		];
	}

	// Show thinking indicator as a real message in the chat
	if (bAiThinking)
	{
		FString Dots;
		for (int32 i = 0; i < (ThinkingDotCount % 3) + 1; i++)
			Dots += TEXT(".");

		ChatMessagesBox->AddSlot().AutoHeight().Padding(8, 4)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(LOCTEXT("BotThinking", "[BobBot]"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(FSlateColor(BobBotUI::BotGreen))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4, 2, 0, 4)
			[
				SAssignNew(ThinkingTextBlock, STextBlock)
				.Text(FText::Format(LOCTEXT("ThinkingDots", "Thinking{0}"), FText::FromString(Dots)))
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 10))
				.ColorAndOpacity(FSlateColor(BobBotUI::DimGray))
			]
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

	CommandInput->SetText(FText::GetEmpty());

	// -- Slash commands --
	if (Message.StartsWith(TEXT("/")))
	{
		FString Cmd = Message.ToLower();
		if (Cmd == TEXT("/clear"))
		{
			OnClearChatClicked();
			return FReply::Handled();
		}
		if (Cmd == TEXT("/cost"))
		{
			AddChatMessage(FChatMessage::ESender::System, FString::Printf(
				TEXT("Session cost: $%.4f  |  Messages: %d  |  Model: %s"),
				TotalSessionCost, SessionMessageCount, *FBobBotConfig::Get().ChatModel));
			return FReply::Handled();
		}
		if (Cmd.StartsWith(TEXT("/model")))
		{
			FString ModelArg = Message.Mid(6).TrimStartAndEnd().ToLower();
			if (ModelArg == TEXT("sonnet") || ModelArg == TEXT("opus") || ModelArg == TEXT("haiku"))
			{
				HandleModelSelected(ModelArg);
				AddChatMessage(FChatMessage::ESender::System, FString::Printf(TEXT("Model switched to %s."), *ModelArg));
			}
			else
			{
				AddChatMessage(FChatMessage::ESender::System, FString::Printf(
					TEXT("Current model: %s.  Usage: /model sonnet|opus|haiku"), *FBobBotConfig::Get().ChatModel));
			}
			return FReply::Handled();
		}
		if (Cmd == TEXT("/help"))
		{
			AddChatMessage(FChatMessage::ESender::System,
				TEXT("Commands: /clear  /cost  /model [sonnet|opus|haiku]  /help"));
			return FReply::Handled();
		}
		// Unknown slash command — fall through to send as regular message
	}

	const FBobBotConfig& Config = FBobBotConfig::Get();
	if (!Config.bClaudeCodeAvailable)
	{
		AddChatMessage(FChatMessage::ESender::Error, TEXT("Claude Code not found. Install it first."));
		return FReply::Handled();
	}
	if (!Config.bClaudeAuthenticated)
	{
		AddChatMessage(FChatMessage::ESender::Error, TEXT("Not authenticated. Run 'claude login' in a terminal."));
		return FReply::Handled();
	}

	AddChatMessage(FChatMessage::ESender::User, Message);

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin)
	{
		AddChatMessage(FChatMessage::ESender::Error, TEXT("PythonScriptPlugin not available."));
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

	// Immediately rebuild to show thinking indicator
	RebuildChatMessages();

	return FReply::Handled();
}

FReply SBobBotPanel::OnClearChatClicked()
{
	ChatHistory.Empty();
	SessionMessageCount = 0;
	TotalSessionCost = 0.f;
	RebuildChatMessages();

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (PythonPlugin)
		PythonPlugin->ExecPythonCommand(TEXT("import bob_chat; bob_chat.clear_session()"));

	AddChatMessage(FChatMessage::ESender::System, TEXT("Chat cleared."));
	return FReply::Handled();
}

FReply SBobBotPanel::OnStopClicked()
{
	// Kill the claude subprocess
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (PythonPlugin)
	{
		PythonPlugin->ExecPythonCommand(
			TEXT("import bob_chat\n")
			TEXT("if bob_chat._process:\n")
			TEXT("    bob_chat._process.kill()\n")
			TEXT("    bob_chat._process = None\n")
			TEXT("with bob_chat._lock:\n")
			TEXT("    bob_chat._is_thinking = False\n")
			TEXT("    bob_chat._error_message = 'Stopped by user'\n"));
	}

	bAiThinking = false;
	AddChatMessage(FChatMessage::ESender::System, TEXT("Request stopped."));
	return FReply::Handled();
}

// =========================================================================== //
// Tool Approval ("Ask Me" mode)
// =========================================================================== //

void SBobBotPanel::PollApprovalRequests()
{
	if (FBobBotConfig::Get().PermissionMode != EBobBotPermissionMode::AskMe)
		return;

	FString ApprovalFile = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("BobBot") / TEXT("_approval_pending.json"));
	FString ApprovalJson;
	if (!FFileHelper::LoadFileToString(ApprovalJson, *ApprovalFile))
	{
		if (bHasPendingApproval)
		{
			// File was removed by server (approval resolved or timed out)
			bHasPendingApproval = false;
			RebuildChatMessages();
		}
		return;
	}

	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ApprovalJson);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) return;

	double IdDbl = 0;
	Obj->TryGetNumberField(TEXT("id"), IdDbl);
	int32 Id = static_cast<int32>(IdDbl);

	if (Id == PendingApprovalId && bHasPendingApproval)
		return; // Already showing this one

	PendingApprovalId = Id;
	Obj->TryGetStringField(TEXT("tool"), PendingApprovalTool);
	Obj->TryGetStringField(TEXT("code"), PendingApprovalCode);
	bHasPendingApproval = true;

	// Show the approval request in chat
	FString ApprovalContent = FString::Printf(
		TEXT("Tool: %s\n\n%s"),
		*PendingApprovalTool, *PendingApprovalCode);
	AddChatMessage(FChatMessage::ESender::Approval, ApprovalContent);
}

FReply SBobBotPanel::OnApproveClicked()
{
	FString ResponseFile = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("BobBot") / TEXT("_approval_response.json"));
	FString Json = FString::Printf(TEXT("{\"id\":%d,\"decision\":\"approved\"}"), PendingApprovalId);
	FFileHelper::SaveStringToFile(Json, *ResponseFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	bHasPendingApproval = false;
	AddChatMessage(FChatMessage::ESender::System, TEXT("Tool execution approved."));
	return FReply::Handled();
}

FReply SBobBotPanel::OnDenyClicked()
{
	FString ResponseFile = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("BobBot") / TEXT("_approval_response.json"));
	FString Json = FString::Printf(TEXT("{\"id\":%d,\"decision\":\"denied\"}"), PendingApprovalId);
	FFileHelper::SaveStringToFile(Json, *ResponseFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	bHasPendingApproval = false;
	AddChatMessage(FChatMessage::ESender::System, TEXT("Tool execution denied."));
	return FReply::Handled();
}

// =========================================================================== //
// Thinking indicator animation
// =========================================================================== //

void SBobBotPanel::UpdateThinkingIndicator()
{
	if (bAiThinking && !bWasThinking)
	{
		// Just started thinking — rebuild to show thinking message
		bWasThinking = true;
		RebuildChatMessages();
	}
	else if (!bAiThinking && bWasThinking)
	{
		// Stopped thinking — rebuild to remove thinking message
		bWasThinking = false;
		RebuildChatMessages();
	}
	else if (bAiThinking && ThinkingTextBlock.IsValid())
	{
		// Update thinking dots in-place
		FString Dots;
		for (int32 i = 0; i < (ThinkingDotCount % 3) + 1; i++)
			Dots += TEXT(".");
		ThinkingTextBlock->SetText(FText::Format(LOCTEXT("ThinkingDotsUpdate", "Thinking{0}"), FText::FromString(Dots)));
	}
}

// =========================================================================== //
// Tick / Polling
// =========================================================================== //

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
		PollApprovalRequests();
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

	// Error — now shown in orange as Error sender
	FString ErrorText;
	if (Result->TryGetStringField(TEXT("error"), ErrorText) && !ErrorText.IsEmpty())
	{
		AddChatMessage(FChatMessage::ESender::Error, ErrorText);
	}

	// Thinking state
	bool bThinking = false;
	Result->TryGetBoolField(TEXT("thinking"), bThinking);
	bAiThinking = bThinking;
}

#undef LOCTEXT_NAMESPACE
