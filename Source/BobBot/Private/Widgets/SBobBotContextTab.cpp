// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "Widgets/SBobBotContextTab.h"
#include "BobBot.h"
#include "BobBotChatController.h"
#include "BobBotConfig.h"
#include "BobBotConstants.h"
#include "BobBotPythonBridge.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"

#define LOCTEXT_NAMESPACE "SBobBotContextTab"

// =========================================================================== //
// Construction
// =========================================================================== //

void SBobBotContextTab::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;

	// Pre-build sections in containers
	TSharedRef<SWidget> SysPromptSection = BobBot::UI::Container(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
		[ SNew(STextBlock).Text(LOCTEXT("SysPromptDesc", "BobBot's base personality and instructions.")).Font(BobBot::Theme::FontSmall()).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)).AutoWrapText(true) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).MaxDesiredHeight(300.f)
			[
				SAssignNew(SystemPromptEditor, SMultiLineEditableTextBox)
				.HintText(LOCTEXT("SysPromptHint", "Leave empty for the default BobBot prompt..."))
				.Font(BobBot::Theme::FontSmall())
				.AutoWrapText(true)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[ SNew(SButton).Text(LOCTEXT("SavePrompt", "Save")).OnClicked(this, &SBobBotContextTab::HandleSaveSystemPrompt) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ SNew(SButton).Text(LOCTEXT("ResetPrompt", "Reset to Default")).OnClicked(this, &SBobBotContextTab::HandleResetSystemPrompt) ]
		]
	, FMargin(10, 8));

	TSharedRef<SWidget> ProjectSection = BobBot::UI::Container(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
		[ SNew(STextBlock).Text(LOCTEXT("ProjectContextDesc", "Describe your project so BobBot understands what you're building.")).Font(BobBot::Theme::FontSmall()).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)).AutoWrapText(true) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).MaxDesiredHeight(300.f)
			[
				SAssignNew(ProjectContextEditor, SMultiLineEditableTextBox)
				.HintText(LOCTEXT("ProjectContextHint", "Project conventions, architecture, goals..."))
				.Font(BobBot::Theme::FontSmall())
				.AutoWrapText(true)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[ SNew(SButton).Text(LOCTEXT("SaveProjectContext", "Save")).OnClicked(this, &SBobBotContextTab::HandleSaveProjectContext) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ SNew(SButton).Text(LOCTEXT("ReloadProjectContext", "Reload")).ToolTipText(LOCTEXT("ReloadProjectContextTip", "Reload from disk")).OnClicked(this, &SBobBotContextTab::HandleReloadProjectContext) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
		[ SNew(STextBlock).Text(this, &SBobBotContextTab::GetProjectMdPathInfoText).Font(BobBot::Theme::FontCaption()).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)).AutoWrapText(true) ]
	, FMargin(10, 8));

	TSharedRef<SWidget> MemorySection = BobBot::UI::Container(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
		[ SNew(STextBlock).Text(LOCTEXT("MemoryDesc", "BobBot automatically remembers project details, architecture, and your preferences across conversations.")).Font(BobBot::Theme::FontSmall()).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)).AutoWrapText(true) ]
		+ SVerticalBox::Slot().AutoHeight()
		[ SNew(STextBlock).Text(this, &SBobBotContextTab::GetMemoryPathInfoText).Font(BobBot::Theme::FontSmall()).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)).AutoWrapText(true) ]
		+ SVerticalBox::Slot().AutoHeight()
		[ SAssignNew(MemoryStatsText, STextBlock).Font(BobBot::Theme::FontSmall()).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[ SNew(SButton).Text(LOCTEXT("ViewMemoryBtn", "View Memory Folder")).OnClicked(this, &SBobBotContextTab::HandleViewMemoryFolder) ]
			+ SHorizontalBox::Slot().AutoWidth()
			[ SNew(SButton).Text(LOCTEXT("RefreshMemoryBtn", "Refresh")).OnClicked(this, &SBobBotContextTab::HandleRefreshMemory) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(SBox).MaxDesiredHeight(200.f)
			[
				SAssignNew(MemoryPreview, SMultiLineEditableTextBox)
				.IsReadOnly(true)
				.Font(BobBot::Theme::FontSmall())
				.AutoWrapText(true)
			]
		]
	, FMargin(10, 8));

	TSharedRef<SWidget> ToolRefSection = BobBot::UI::Container(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
		[ SNew(STextBlock).Text(LOCTEXT("ToolRefDesc", "BobBot's tool reference and domain-specific rules.")).Font(BobBot::Theme::FontSmall()).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)).AutoWrapText(true) ]
		+ SVerticalBox::Slot().AutoHeight()
		[ BobBot::UI::KeyValueRow(LOCTEXT("ClaudeMdKey", "CLAUDE.md"),
			TAttribute<FText>::CreateSP(this, &SBobBotContextTab::GetClaudeMdPathInfoText)) ]
		+ SVerticalBox::Slot().AutoHeight()
		[ SNew(STextBlock).Text(LOCTEXT("ClaudeMdContents", "Tool overview, BobBotLib API, UE essentials")).Font(BobBot::Theme::FontSmall()).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
		[ SNew(SButton).Text(LOCTEXT("ViewClaudeMd", "View")).OnClicked(this, &SBobBotContextTab::HandleViewClaudeMd) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 4)
		[ SNew(STextBlock).Text(LOCTEXT("RulesLabel", "Rules loaded on demand:")).Font(BobBot::Theme::FontBody()) ]
		+ SVerticalBox::Slot().AutoHeight()
		[ SAssignNew(RulesListBox, SVerticalBox) ]
	, FMargin(10, 8));

	ChildSlot
	[
		SNew(SScrollBox)

		// SYSTEM PROMPT
		+ SScrollBox::Slot().Padding(8, 8, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("SysPromptSection", "SYSTEM PROMPT")) ]
		+ SScrollBox::Slot().Padding(8, 0, 8, 8) [ SysPromptSection ]

		// PROJECT CONTEXT
		+ SScrollBox::Slot().Padding(8, 4, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("ProjectContextSection", "PROJECT CONTEXT")) ]
		+ SScrollBox::Slot().Padding(8, 0, 8, 8) [ ProjectSection ]

		// MEMORY
		+ SScrollBox::Slot().Padding(8, 4, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("MemorySection", "MEMORY")) ]
		+ SScrollBox::Slot().Padding(8, 0, 8, 8) [ MemorySection ]

		// ================================================================= //
		// SECTION 4: TOOL REFERENCE
		// ================================================================= //
		// TOOL REFERENCE
		+ SScrollBox::Slot().Padding(8, 4, 8, 4) [ BobBot::UI::SectionHeading(LOCTEXT("ToolRefSection", "TOOL REFERENCE")) ]
		+ SScrollBox::Slot().Padding(8, 0, 8, 8) [ ToolRefSection ]
	];

	// Load current content into editors
	LoadSystemPromptIntoEditor();
	LoadProjectContext();
	LoadMemoryPreview();
	PopulateRulesList();
}

// =========================================================================== //
// System prompt
// =========================================================================== //

void SBobBotContextTab::LoadSystemPromptIntoEditor()
{
	if (!SystemPromptEditor.IsValid()) return;

	// Config override takes priority; otherwise fetch the built-in default from Python
	const FBobBotConfig& Config = FBobBotConfig::Get();
	if (!Config.SystemPrompt.IsEmpty())
	{
		SystemPromptEditor->SetText(FText::FromString(Config.SystemPrompt));
		return;
	}

	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (!Bridge.IsAvailable()) return;

	TSharedPtr<FJsonObject> Result = Bridge.ExecPythonWithJsonResult(
		TEXT("import bob_chat, json\nopen(output_path, 'w').write(json.dumps({'prompt': bob_chat.get_default_prompt()}))\n"),
		TEXT("_default_prompt.txt"));
	if (Result.IsValid())
	{
		FString Prompt;
		Result->TryGetStringField(TEXT("prompt"), Prompt);
		SystemPromptEditor->SetText(FText::FromString(Prompt));
	}
}

FReply SBobBotContextTab::HandleSaveSystemPrompt()
{
	if (!SystemPromptEditor.IsValid()) return FReply::Handled();

	FBobBotConfig& Config = FBobBotConfig::Get();
	FString NewPrompt = SystemPromptEditor->GetText().ToString().TrimStartAndEnd();
	Config.SystemPrompt = NewPrompt;
	Config.Save();

	// Write to the override file so Python picks it up on next reconnect
	if (!NewPrompt.IsEmpty())
	{
		FString PromptFile = FBobBotPythonBridge::Get().GetTempDir() / BobBot::TempFiles::SystemPrompt;
		FFileHelper::SaveStringToFile(NewPrompt, *PromptFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
	else
	{
		// Empty = use default, delete override file
		FString PromptFile = FBobBotPythonBridge::Get().GetTempDir() / BobBot::TempFiles::SystemPrompt;
		FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*PromptFile);
	}

	if (Controller)
		Controller->AddExternalMessage(FBobBotChatMessage::ESender::System, TEXT("System prompt saved. Takes effect on next message."));
	return FReply::Handled();
}

FReply SBobBotContextTab::HandleResetSystemPrompt()
{
	FBobBotConfig& Config = FBobBotConfig::Get();
	Config.SystemPrompt.Empty();
	Config.Save();

	// Delete override file
	FString PromptFile = FBobBotPythonBridge::Get().GetTempDir() / BobBot::TempFiles::SystemPrompt;
	FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*PromptFile);

	// Load default back into editor
	LoadSystemPromptIntoEditor();

	if (Controller)
		Controller->AddExternalMessage(FBobBotChatMessage::ESender::System, TEXT("System prompt reset to default."));
	return FReply::Handled();
}

// =========================================================================== //
// Project context (PROJECT.md)
// =========================================================================== //

FString SBobBotContextTab::GetProjectMdPath() const
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("PROJECT.md"));
}

void SBobBotContextTab::LoadProjectContext()
{
	FString Path = GetProjectMdPath();
	FString Content;
	if (FFileHelper::LoadFileToString(Content, *Path) && ProjectContextEditor.IsValid())
		ProjectContextEditor->SetText(FText::FromString(Content));
	else if (ProjectContextEditor.IsValid())
		ProjectContextEditor->SetText(FText::GetEmpty());
}

FReply SBobBotContextTab::HandleSaveProjectContext()
{
	if (!ProjectContextEditor.IsValid()) return FReply::Handled();
	FString Content = ProjectContextEditor->GetText().ToString();
	FString Path = GetProjectMdPath();
	if (FFileHelper::SaveStringToFile(Content, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		if (Controller)
			Controller->AddExternalMessage(FBobBotChatMessage::ESender::System, TEXT("PROJECT.md saved."));
	}
	else
	{
		if (Controller)
			Controller->AddExternalMessage(FBobBotChatMessage::ESender::Error, TEXT("Failed to save PROJECT.md."));
	}
	return FReply::Handled();
}

FReply SBobBotContextTab::HandleReloadProjectContext()
{
	LoadProjectContext();
	return FReply::Handled();
}

FText SBobBotContextTab::GetProjectMdPathInfoText() const
{
	FString Path = GetProjectMdPath();
	bool bExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*Path);
	return FText::FromString(FString::Printf(TEXT("%s   %s"), *Path, bExists ? TEXT("exists") : TEXT("not created")));
}

// =========================================================================== //
// Memory
// =========================================================================== //

FString SBobBotContextTab::GetMemoryDir()
{
	// Claude Code convention: C:\UGW\game -> c--UGW-game
	FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	// Normalize to forward slashes, strip trailing
	FPaths::NormalizeDirectoryName(ProjectRoot);

	// Build project hash matching Claude Code's directory naming
	FString ProjectHash = ProjectRoot;
	if (ProjectHash.Len() >= 2 && ProjectHash[1] == TEXT(':'))
	{
		ProjectHash[0] = FChar::ToLower(ProjectHash[0]);
	}
	// Replace :/ or :\ with -- (drive separator)
	ProjectHash.ReplaceInline(TEXT(":/"), TEXT("--"));
	ProjectHash.ReplaceInline(TEXT(":\\"), TEXT("--"));
	// Replace remaining slashes with -
	ProjectHash.ReplaceInline(TEXT("/"), TEXT("-"));
	ProjectHash.ReplaceInline(TEXT("\\"), TEXT("-"));

	// Use USERPROFILE env var for reliable home directory on Windows
	FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	if (Home.IsEmpty())
	{
		Home = FPlatformProcess::UserHomeDir();
	}

	return Home / TEXT(".claude") / TEXT("projects") / ProjectHash / TEXT("memory");
}

void SBobBotContextTab::LoadMemoryPreview()
{
	FString MemDir = GetMemoryDir();
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();

	// Count .md files and find most recent
	MemoryFileCount = 0;
	FDateTime MostRecent = FDateTime::MinValue();

	if (PF.DirectoryExists(*MemDir))
	{
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(MemDir / TEXT("*.md")), true, false);
		MemoryFileCount = Files.Num();

		for (const FString& File : Files)
		{
			FDateTime ModTime = IFileManager::Get().GetTimeStamp(*(MemDir / File));
			if (ModTime > MostRecent)
				MostRecent = ModTime;
		}
	}

	if (MostRecent > FDateTime::MinValue())
		MemoryLastUpdated = MostRecent.ToString(TEXT("%Y-%m-%d %H:%M"));
	else
		MemoryLastUpdated = TEXT("never");

	// Update stats text
	if (MemoryStatsText.IsValid())
	{
		FString Stats = FString::Printf(TEXT("%d memory file(s), last updated %s"), MemoryFileCount, *MemoryLastUpdated);
		MemoryStatsText->SetText(FText::FromString(Stats));
	}

	// Load MEMORY.md preview
	if (MemoryPreview.IsValid())
	{
		FString MemoryMdPath = MemDir / TEXT("MEMORY.md");
		FString Content;
		if (FFileHelper::LoadFileToString(Content, *MemoryMdPath))
		{
			MemoryPreview->SetText(FText::FromString(Content));
		}
		else
		{
			MemoryPreview->SetText(FText::FromString(TEXT("No memory yet. BobBot will remember things as you work together.")));
		}
	}
}

FText SBobBotContextTab::GetMemoryPathInfoText() const
{
	FString MemDir = GetMemoryDir();
	bool bExists = FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*MemDir);
	FString MemoryMdPath = MemDir / TEXT("MEMORY.md");
	bool bMemoryExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*MemoryMdPath);
	return FText::FromString(FString::Printf(TEXT("MEMORY.md   %s"), bMemoryExists ? TEXT("exists") : TEXT("not yet created")));
}

FText SBobBotContextTab::GetMemoryStatsText() const
{
	return FText::FromString(FString::Printf(TEXT("%d memory file(s), last updated %s"), MemoryFileCount, *MemoryLastUpdated));
}

FReply SBobBotContextTab::HandleViewMemoryFolder()
{
	FString MemDir = GetMemoryDir();
	FPlatformProcess::ExploreFolder(*MemDir);
	return FReply::Handled();
}

FReply SBobBotContextTab::HandleRefreshMemory()
{
	LoadMemoryPreview();
	return FReply::Handled();
}

// =========================================================================== //
// Tool reference (CLAUDE.md)
// =========================================================================== //

FString SBobBotContextTab::GetClaudeMdPath() const
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::ProjectPluginsDir() / TEXT("BobBot") / TEXT("CLAUDE.md"));
}

FText SBobBotContextTab::GetClaudeMdPathInfoText() const
{
	FString RelPath = TEXT("Plugins/BobBot/CLAUDE.md");
	FString FullPath = GetClaudeMdPath();
	bool bExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullPath);
	return FText::FromString(FString::Printf(TEXT("%s   %s"), *RelPath, bExists ? TEXT("exists") : TEXT("MISSING")));
}

FReply SBobBotContextTab::HandleViewClaudeMd()
{
	FString Path = GetClaudeMdPath();
	FPlatformProcess::LaunchFileInDefaultExternalApplication(*Path);
	return FReply::Handled();
}

FString SBobBotContextTab::GetRulesDir()
{
	return FPaths::ConvertRelativePathToFull(
		FPaths::ProjectPluginsDir() / TEXT("BobBot") / TEXT("Config") / TEXT("Rules"));
}

void SBobBotContextTab::PopulateRulesList()
{
	if (!RulesListBox.IsValid()) return;

	RulesListBox->ClearChildren();

	FString RulesPath = GetRulesDir();
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(RulesPath / TEXT("*.md")), true, false);
	Files.Sort();

	for (const FString& FileName : Files)
	{
		// Read the first heading (# ...) after frontmatter to use as description
		FString FullPath = RulesPath / FileName;
		FString FileContent;
		FString Description;
		if (FFileHelper::LoadFileToString(FileContent, *FullPath))
		{
			bool bPastFrontmatter = false;
			int32 FenceCount = 0;
			TArray<FString> Lines;
			FileContent.ParseIntoArrayLines(Lines);
			for (const FString& Line : Lines)
			{
				if (Line.TrimEnd() == TEXT("---"))
				{
					FenceCount++;
					if (FenceCount >= 2) bPastFrontmatter = true;
					continue;
				}
				if (bPastFrontmatter && Line.StartsWith(TEXT("# ")))
				{
					Description = Line.Mid(2).TrimStartAndEnd();
					break;
				}
			}
		}

		FString Display = FString::Printf(TEXT("%s - %s"),
			*FileName, Description.IsEmpty() ? TEXT("") : *Description);

		RulesListBox->AddSlot()
		.Padding(0, 1)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Display))
			.Font(BobBot::Theme::FontSmall())
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
		];
	}

	if (Files.Num() == 0)
	{
		RulesListBox->AddSlot()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoRules", "No rules found"))
			.Font(BobBot::Theme::FontSmall())
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
		];
	}
}

#undef LOCTEXT_NAMESPACE
