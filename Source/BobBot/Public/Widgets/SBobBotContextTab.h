// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FBobBotChatController;

/**
 * Context tab — system prompt, project context (PROJECT.md), and tool reference documentation.
 */
class SBobBotContextTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBobBotContextTab) {}
		SLATE_ARGUMENT(FBobBotChatController*, Controller)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FBobBotChatController* Controller = nullptr;

	// -- System prompt --
	TSharedPtr<class SMultiLineEditableTextBox> SystemPromptEditor;
	FReply HandleSaveSystemPrompt();
	FReply HandleResetSystemPrompt();
	void LoadSystemPromptIntoEditor();

	// -- Project context (PROJECT.md) --
	TSharedPtr<class SMultiLineEditableTextBox> ProjectContextEditor;
	FReply HandleSaveProjectContext();
	FReply HandleReloadProjectContext();
	void LoadProjectContext();
	FString GetProjectMdPath() const;
	FText GetProjectMdPathInfoText() const;

	// -- Memory --
	TSharedPtr<class SMultiLineEditableTextBox> MemoryPreview;
	TSharedPtr<class STextBlock> MemoryStatsText;
	FReply HandleViewMemoryFolder();
	FReply HandleRefreshMemory();
	void LoadMemoryPreview();
	static FString GetMemoryDir();
	FText GetMemoryPathInfoText() const;
	FText GetMemoryStatsText() const;

	// Cached memory stats
	int32 MemoryFileCount = 0;
	FString MemoryLastUpdated;

	// -- Tool reference --
	FReply HandleViewClaudeMd();
	FString GetClaudeMdPath() const;
	FText GetClaudeMdPathInfoText() const;
};
