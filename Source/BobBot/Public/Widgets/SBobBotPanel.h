// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FBobBotChatController;
class SBobBotConnectTab;
class SBobBotChatTab;

/**
 * Main BobBot editor panel — slim orchestrator.
 * Owns the ChatController and wires up ConnectTab and ChatTab.
 */
class SBobBotPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBobBotPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Explicit cleanup — called from tab close callback before widget destruction. */
	void Shutdown();

private:
	enum class EBobBotTab : uint8 { Connect, Chat };
	EBobBotTab ActiveTab = EBobBotTab::Connect;

	FReply OnTabClicked(EBobBotTab Tab);
	FSlateColor GetTabColor(EBobBotTab Tab) const;

	TUniquePtr<FBobBotChatController> ChatController;
	TSharedPtr<SBobBotConnectTab> ConnectTab;
	TSharedPtr<SBobBotChatTab> ChatTab;
	TSharedPtr<class SWidgetSwitcher> TabSwitcher;
};
