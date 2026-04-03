// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "Widgets/SBobBotPanel.h"
#include "BobBotChatController.h"
#include "BobBotConfig.h"
#include "BobBotRuntimeStatus.h"
#include "Widgets/SBobBotWelcomeTab.h"
#include "Widgets/SBobBotConnectTab.h"
#include "Widgets/SBobBotChatTab.h"
#include "Widgets/SBobBotContextTab.h"
#include "Widgets/SBobBotInfoTab.h"
#include "BobBotPythonBridge.h"
#include "BobBotConstants.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

#define LOCTEXT_NAMESPACE "SBobBotPanel"

// =========================================================================== //
// Construction
// =========================================================================== //

void SBobBotPanel::Construct(const FArguments& InArgs)
{
	ChatController = MakeUnique<FBobBotChatController>();

	bWelcomeActive = ShouldShowWelcome();

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(6, 6, 6, 0)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(BobBot::Theme::Surface)
			.Padding(FMargin(6, 4))
			[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton).Text(LOCTEXT("ConnectTab", "Connect"))
				.OnClicked(this, &SBobBotPanel::OnTabClicked, EBobBotTab::Connect)
				.ButtonColorAndOpacity(this, &SBobBotPanel::GetTabColor, EBobBotTab::Connect)
				.IsEnabled_Lambda([this]() { return !bWelcomeActive; })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton).Text(LOCTEXT("ChatTab", "Chat"))
				.OnClicked(this, &SBobBotPanel::OnTabClicked, EBobBotTab::Chat)
				.ButtonColorAndOpacity(this, &SBobBotPanel::GetTabColor, EBobBotTab::Chat)
				.IsEnabled_Lambda([this]() { return !bWelcomeActive; })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton).Text(LOCTEXT("ContextTab", "Context"))
				.OnClicked(this, &SBobBotPanel::OnTabClicked, EBobBotTab::Context)
				.ButtonColorAndOpacity(this, &SBobBotPanel::GetTabColor, EBobBotTab::Context)
				.IsEnabled_Lambda([this]() { return !bWelcomeActive; })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("InfoTab", "Info"))
				.OnClicked(this, &SBobBotPanel::OnTabClicked, EBobBotTab::Info)
				.ButtonColorAndOpacity(this, &SBobBotPanel::GetTabColor, EBobBotTab::Info)
				.IsEnabled_Lambda([this]() { return !bWelcomeActive; })
			]
			]
		]

		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(TabSwitcher, SWidgetSwitcher)
			.WidgetIndex(bWelcomeActive ? 0 : 1)

			// Index 0: Welcome (FTUE)
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(WelcomeTab, SBobBotWelcomeTab)
				.OnSetupComplete(FSimpleDelegate::CreateSP(this, &SBobBotPanel::OnWelcomeComplete))
				.OnSkipped(FSimpleDelegate::CreateSP(this, &SBobBotPanel::OnWelcomeSkipped))
			]

			// Index 1: Connect
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(ConnectTab, SBobBotConnectTab)
				.Controller(ChatController.Get())
				.OnGoToChat(FSimpleDelegate::CreateLambda([this]() { OnTabClicked(EBobBotTab::Chat); }))
				.OnFactoryReset(FSimpleDelegate::CreateSP(this, &SBobBotPanel::OnFactoryReset))
			]

			// Index 2: Chat
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(ChatTab, SBobBotChatTab)
				.Controller(ChatController.Get())
			]

			// Index 3: Context
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(ContextTab, SBobBotContextTab)
				.Controller(ChatController.Get())
			]

			// Index 4: Info
			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(InfoTab, SBobBotInfoTab)
				.Controller(ChatController.Get())
			]
		]
	];

	if (bWelcomeActive)
	{
		ActiveTab = EBobBotTab::Welcome;
	}
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
	ChatController.Reset(); // destructor fires -> KillChatProcess
}

// =========================================================================== //
// Welcome tab logic
// =========================================================================== //

bool SBobBotPanel::ShouldShowWelcome() const
{
	return !FBobBotConfig::Get().bSetupComplete;
}

void SBobBotPanel::OnWelcomeComplete()
{
	bWelcomeActive = false;
	FBobBotConfig& Cfg = FBobBotConfig::Get();
	Cfg.bSetupComplete = true;
	Cfg.Save();
	Cfg.ApplyEnvironmentVars();

	// Now that venv is built, do the same init that StartupModule step 6 does for returning users:
	// sync env vars, import SDK, and ensure the bridge is running.
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (Bridge.IsAvailable())
	{
		Bridge.ExecPythonCommand(BobBot::Scripts::EnvSync);
		// Load SDK — ensure_ready() retries the import if it failed during Welcome
		Bridge.ExecPythonCommand(TEXT("import bob_chat_sdk; bob_chat_sdk.ensure_ready()"));
	}

	OnTabClicked(EBobBotTab::Connect);
}

void SBobBotPanel::OnWelcomeSkipped()
{
	bWelcomeActive = false;
	FBobBotConfig& Cfg = FBobBotConfig::Get();
	Cfg.bSetupComplete = true;
	Cfg.Save();
	Cfg.ApplyEnvironmentVars();
	OnTabClicked(EBobBotTab::Connect);
}

void SBobBotPanel::OnFactoryReset()
{
	bWelcomeActive = true;
	ActiveTab = EBobBotTab::Welcome;
	if (TabSwitcher.IsValid())
		TabSwitcher->SetActiveWidgetIndex(0);

	// Reset state machine — re-runs all setup steps from scratch
	if (WelcomeTab.IsValid())
		WelcomeTab->Reset();
}

// =========================================================================== //
// Tab switching
// =========================================================================== //

FReply SBobBotPanel::OnTabClicked(EBobBotTab Tab)
{
	if (bWelcomeActive && Tab != EBobBotTab::Welcome)
		return FReply::Handled();

	ActiveTab = Tab;
	if (TabSwitcher.IsValid())
		TabSwitcher->SetActiveWidgetIndex(static_cast<int32>(Tab));
	return FReply::Handled();
}

FSlateColor SBobBotPanel::GetTabColor(EBobBotTab Tab) const
{
	if (bWelcomeActive)
		return FSlateColor(BobBot::Theme::InactiveTab);
	return FSlateColor(ActiveTab == Tab ? BobBot::Theme::ActiveTab : BobBot::Theme::TextSecondary);
}

#undef LOCTEXT_NAMESPACE
