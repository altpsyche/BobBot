// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FBobBotChatController;

/**
 * Info/reference tab: slash commands, tool catalog, BobBotLib API, and about.
 * All content is read-only.
 */
class SBobBotInfoTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBobBotInfoTab) {}
		SLATE_ARGUMENT(FBobBotChatController*, Controller)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FBobBotChatController* Controller = nullptr;

	TSharedRef<SWidget> BuildSlashCommandsSection();
	TSharedRef<SWidget> BuildTraceRecipesSection();
	TSharedRef<SWidget> BuildToolsSection();
	TSharedRef<SWidget> BuildBobBotLibSection();
	TSharedRef<SWidget> BuildAboutSection();
};
