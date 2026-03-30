// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "Widgets/SBobBotPanel.h"
#include "BobBotChatController.h"
#include "Widgets/SBobBotConnectTab.h"
#include "Widgets/SBobBotChatTab.h"
#include "Widgets/SBobBotContextTab.h"
#include "Widgets/SBobBotInfoTab.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SBobBotPanel"

// =========================================================================== //
// Construction
// =========================================================================== //

void SBobBotPanel::Construct(const FArguments& InArgs)
{
	ChatController = MakeUnique<FBobBotChatController>();

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
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton).Text(LOCTEXT("ChatTab", "Chat"))
				.OnClicked(this, &SBobBotPanel::OnTabClicked, EBobBotTab::Chat)
				.ButtonColorAndOpacity(this, &SBobBotPanel::GetTabColor, EBobBotTab::Chat)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton).Text(LOCTEXT("ContextTab", "Context"))
				.OnClicked(this, &SBobBotPanel::OnTabClicked, EBobBotTab::Context)
				.ButtonColorAndOpacity(this, &SBobBotPanel::GetTabColor, EBobBotTab::Context)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("InfoTab", "Info"))
				.OnClicked(this, &SBobBotPanel::OnTabClicked, EBobBotTab::Info)
				.ButtonColorAndOpacity(this, &SBobBotPanel::GetTabColor, EBobBotTab::Info)
			]
		]

		+ SVerticalBox::Slot().AutoHeight() [ SNew(SSeparator) ]

		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(TabSwitcher, SWidgetSwitcher)
			.WidgetIndex(0)
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(ConnectTab, SBobBotConnectTab)
				.Controller(ChatController.Get())
				.OnGoToChat(FSimpleDelegate::CreateLambda([this]() { OnTabClicked(EBobBotTab::Chat); }))
			]
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(ChatTab, SBobBotChatTab)
				.Controller(ChatController.Get())
			]
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(ContextTab, SBobBotContextTab)
				.Controller(ChatController.Get())
			]
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(InfoTab, SBobBotInfoTab)
				.Controller(ChatController.Get())
			]
		]
	];
}

// =========================================================================== //
// Tick
// =========================================================================== //

void SBobBotPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (ChatController)
	{
		ChatController->Tick(InDeltaTime);
	}
}

// =========================================================================== //
// Shutdown
// =========================================================================== //

void SBobBotPanel::Shutdown()
{
	ChatController.Reset(); // destructor fires -> SaveChatHistory + KillChatProcess
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

#undef LOCTEXT_NAMESPACE
