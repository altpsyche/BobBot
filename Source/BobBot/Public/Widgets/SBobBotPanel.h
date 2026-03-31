// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FBobBotChatController;
class SBobBotWelcomeTab;
class SBobBotConnectTab;
class SBobBotChatTab;
class SBobBotContextTab;
class SBobBotInfoTab;

/**
 * Main BobBot editor panel — slim orchestrator.
 * Owns the ChatController and wires up all tabs including the FTUE Welcome tab.
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
	enum class EBobBotTab : uint8 { Welcome, Connect, Chat, Context, Info };
	EBobBotTab ActiveTab = EBobBotTab::Connect;

	FReply OnTabClicked(EBobBotTab Tab);
	FSlateColor GetTabColor(EBobBotTab Tab) const;
	bool ShouldShowWelcome() const;
	void OnWelcomeComplete();
	void OnWelcomeSkipped();

	bool bWelcomeActive = false;

	TUniquePtr<FBobBotChatController> ChatController;
	TSharedPtr<SBobBotWelcomeTab> WelcomeTab;
	TSharedPtr<SBobBotConnectTab> ConnectTab;
	TSharedPtr<SBobBotChatTab> ChatTab;
	TSharedPtr<SBobBotContextTab> ContextTab;
	TSharedPtr<SBobBotInfoTab> InfoTab;
	TSharedPtr<class SWidgetSwitcher> TabSwitcher;
};
