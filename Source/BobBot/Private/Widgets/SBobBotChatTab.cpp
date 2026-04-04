// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "Widgets/SBobBotChatTab.h"
#include "BobBot.h"
#include "BobBotConfig.h"
#include "BobBotConstants.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Framework/Text/RichTextLayoutMarshaller.h"
#include "BobBotStyle.h"
#include "HAL/PlatformApplicationMisc.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "AssetRegistry/AssetData.h"
#include "Brushes/SlateImageBrush.h"

#define LOCTEXT_NAMESPACE "SBobBotChatTab"

// =========================================================================== //
// Asset drop target — wraps the chat input to accept Content Browser drags
// =========================================================================== //

DECLARE_DELEGATE_OneParam(FOnAssetPathsDropped, const FString& /*Paths*/);

class SAssetDropTarget : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SAssetDropTarget) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_EVENT(FOnAssetPathsDropped, OnDropped)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnDroppedDelegate = InArgs._OnDropped;

		SBorder::Construct(SBorder::FArguments()
			.BorderBackgroundColor_Lambda([this]() -> FSlateColor
			{
				return bDragActive
					? FSlateColor(BobBot::Colors::ActiveBlue)
					: FSlateColor(FLinearColor::Transparent);
			})
			.Padding(0)
			[
				InArgs._Content.Widget
			]
		);
	}

	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		if (DragDropEvent.GetOperationAs<FAssetDragDropOp>().IsValid())
		{
			bDragActive = true;
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override
	{
		SBorder::OnDragLeave(DragDropEvent);
		bDragActive = false;
	}

	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		bDragActive = false;

		TSharedPtr<FAssetDragDropOp> AssetOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
		if (AssetOp.IsValid() && AssetOp->HasAssets())
		{
			FString PathsText;
			for (const FAssetData& Asset : AssetOp->GetAssets())
			{
				if (!PathsText.IsEmpty())
					PathsText += TEXT(" ");
				// Friendly format: "SM_Cube (StaticMesh) /Game/Meshes/SM_Cube"
				FString ClassName = Asset.AssetClassPath.GetAssetName().ToString();
				PathsText += FString::Printf(TEXT("%s (%s) %s"),
					*Asset.AssetName.ToString(), *ClassName, *Asset.GetObjectPathString());
			}
			OnDroppedDelegate.ExecuteIfBound(PathsText);
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

private:
	FOnAssetPathsDropped OnDroppedDelegate;
	bool bDragActive = false;
};

// =========================================================================== //
// Construction
// =========================================================================== //

void SBobBotChatTab::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;

	// Subscribe to controller delegates
	if (Controller)
	{
		Controller->OnMessageAdded.AddSP(this, &SBobBotChatTab::OnMessageAdded);
		Controller->OnHistoryCleared.AddSP(this, &SBobBotChatTab::OnHistoryCleared);
		Controller->OnApprovalStateChanged.AddSP(this, &SBobBotChatTab::OnApprovalChanged);
		Controller->OnThinkingStateChanged.AddSP(this, &SBobBotChatTab::OnThinkingChanged);
		Controller->OnMessageUpdated.AddSP(this, &SBobBotChatTab::OnMessageUpdated);
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		// Header bar
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 6, 6, 0)
		[ BuildHeaderBar() ]

		// Chat messages (scrollable, fills all available space)
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(ChatScrollBox, SScrollBox)
			+ SScrollBox::Slot() [ SAssignNew(ChatMessagesBox, SVerticalBox) ]
		]

		// Context bar (thin line above input area)
		+ SVerticalBox::Slot().AutoHeight()
		[ BuildContextBar() ]

		// Input area with integrated controls
		+ SVerticalBox::Slot().AutoHeight().Padding(6, 4, 6, 6)
		[ BuildInputArea() ]
	];

	// Initial rebuild from loaded history
	RebuildChatMessages();
}

// =========================================================================== //
// BuildHeaderBar — top bar with chat picker, cost, context, MCP status
// =========================================================================== //

TSharedRef<SWidget> SBobBotChatTab::BuildHeaderBar()
{
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
		.BorderBackgroundColor(BobBot::Theme::Surface)
		.Padding(FMargin(6, 5))
		[
			SNew(SHorizontalBox)

			// Chat history dropdown
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 2, 0)
			[
				SNew(SComboButton)
				.ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(6, 3))
				.HasDownArrow(true)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return Controller ? FText::FromString(Controller->GetActiveChatTitle()) : LOCTEXT("NoChat", "Chat"); })
					.Font(BobBot::Theme::FontHeading())
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
				]
				.MenuContent()
				[
					SNew(SBox).MaxDesiredHeight(400.f).MinDesiredWidth(280.f)
					[ SNew(SScrollBox) + SScrollBox::Slot() [ SAssignNew(ChatListBox, SVerticalBox) ] ]
				]
				.OnComboBoxOpened_Lambda([this]() { RebuildChatList(); })
			]

			// New chat
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 2, 0)
			[
				SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(6, 3))
				.ToolTipText(LOCTEXT("NewChatTip", "New conversation"))
				.OnClicked_Lambda([this]() { if (Controller) Controller->NewChat(); return FReply::Handled(); })
				[ SNew(SImage).Image(FBobBotStyle::Get().GetBrush("BobBot.Icon.Plus")).ColorAndOpacity(BobBot::Colors::DimGray) ]
			]

			// Fork
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
			[
				SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(6, 3))
				.ToolTipText(LOCTEXT("ForkTip", "Fork conversation from this point"))
				.OnClicked_Lambda([this]() { if (Controller) Controller->ForkChat(); return FReply::Handled(); })
				.Visibility_Lambda([this]() { return (Controller && Controller->CanFork()) ? EVisibility::Visible : EVisibility::Collapsed; })
				[ SNew(SImage).Image(FBobBotStyle::Get().GetBrush("BobBot.Icon.Fork")).ColorAndOpacity(BobBot::Colors::DimGray) ]
			]

			// Spacer
			+ SHorizontalBox::Slot().FillWidth(1.f) [ SNullWidget::NullWidget ]

			// Cost (hidden when zero)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.Text(this, &SBobBotChatTab::GetCostHeaderText)
				.Font(BobBot::Theme::FontSmall())
				.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
				.Visibility_Lambda([this]() {
					return (Controller && Controller->GetSessionCost() > 0.001f) ? EVisibility::Visible : EVisibility::Collapsed;
				})
			]

			// Context %
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
			[
				SNew(STextBlock)
				.Text(this, &SBobBotChatTab::GetContextHeaderText)
				.Font(BobBot::Theme::FontSmall())
				.ColorAndOpacity(this, &SBobBotChatTab::GetContextHeaderColor)
				.Visibility(this, &SBobBotChatTab::GetContextVisibility)
				.ToolTipText_Lambda([this]() {
					if (!Controller || Controller->GetContextTokensMax() <= 0) return FText::GetEmpty();
					return FText::FromString(FString::Printf(TEXT("Context: %s / %s tokens"),
						*FString::FormatAsNumber(Controller->GetContextTokensUsed()),
						*FString::FormatAsNumber(Controller->GetContextTokensMax())));
				})
			]

			// MCP status dot
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0)
			[
				SNew(SImage)
				.Image(FBobBotStyle::Get().GetBrush("BobBot.Icon.Status"))
				.ColorAndOpacity_Lambda([this]() { return Controller && Controller->IsServerRunning() ? BobBot::Colors::BotGreen : BobBot::Colors::Red; })
				.ToolTipText_Lambda([this]() {
					if (!Controller) return FText::FromString(TEXT("MCP: unknown"));
					return FText::FromString(Controller->IsServerRunning()
						? FString::Printf(TEXT("MCP: running (%d clients)"), Controller->GetConnectedClientCount()) : TEXT("MCP: offline"));
				})
			]

		];
}

// =========================================================================== //
// BuildContextBar -- progress bar showing context token usage
// =========================================================================== //

TSharedRef<SWidget> SBobBotChatTab::BuildContextBar()
{
	return SNew(SBox).HeightOverride(3.f)
		.Visibility(this, &SBobBotChatTab::GetContextVisibility)
		.ToolTipText_Lambda([this]() {
			if (!Controller || Controller->GetContextTokensMax() <= 0) return FText::GetEmpty();
			return FText::FromString(FString::Printf(TEXT("Context: %s / %s tokens (%.1f%%)"),
				*FString::FormatAsNumber(Controller->GetContextTokensUsed()),
				*FString::FormatAsNumber(Controller->GetContextTokensMax()),
				Controller->GetContextPercent()));
		})
		[
			SNew(SProgressBar)
			.Percent_Lambda([this]() -> TOptional<float> {
				if (!Controller || Controller->GetContextTokensMax() <= 0) return 0.f;
				return FMath::Clamp(Controller->GetContextPercent() / 100.f, 0.f, 1.f);
			})
			.FillColorAndOpacity_Lambda([this]() {
				float Pct = Controller ? Controller->GetContextPercent() : 0.f;
				if (Pct >= 95.f) return BobBot::Colors::Red;
				if (Pct >= 85.f) return BobBot::Colors::ErrorOrange;
				if (Pct >= 70.f) return BobBot::Colors::Yellow;
				return BobBot::Theme::ContextBar;  // Subtle blue
			})
		];
}

// =========================================================================== //
// BuildInputArea -- text box + bottom toolbar with dropdowns and buttons
// =========================================================================== //

TSharedRef<SWidget> SBobBotChatTab::BuildInputArea()
{
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
		.BorderBackgroundColor(BobBot::Theme::Surface)
		.Padding(FMargin(0))
		[
			SNew(SVerticalBox)

			// Text input area
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).Padding(FMargin(10, 8, 10, 4))
				[
					SNew(SAssetDropTarget)
					.OnDropped_Lambda([this](const FString& Paths)
					{
						if (CommandInput.IsValid())
						{
							FString Current = CommandInput->GetText().ToString();
							FString Insert = Paths;
							if (!Current.IsEmpty() && !Current.EndsWith(TEXT(" ")))
								Insert = TEXT(" ") + Insert;
							CommandInput->SetText(FText::FromString(Current + Insert));
						}
					})
					[
						SAssignNew(CommandInput, SMultiLineEditableTextBox)
						.HintText(LOCTEXT("ChatHint", "Ask BobBot anything..."))
						.Font(BobBot::Theme::FontBody())
						.AutoWrapText(true)
						.BackgroundColor(BobBot::Theme::Surface)
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
				]
			]

			// Bottom toolbar: [Auto ▾] [Model ▾] [gear]  spacer  [undo] [stop] [send]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
				.BorderBackgroundColor(BobBot::Theme::Elevated)
				.Padding(FMargin(6, 4))
				[
					SNew(SHorizontalBox)

					// Permission mode dropdown
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
					[ BuildPermissionDropdown() ]

					// Model selector dropdown
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
					[ BuildModelDropdown() ]

					// Settings gear (thinking + effort popover)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
					[ BuildSettingsPopover() ]

					// Spacer
					+ SHorizontalBox::Slot().FillWidth(1.f) [ SNullWidget::NullWidget ]

					// Undo button (contextual)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 2, 0)
					[
						SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder")
						.ContentPadding(FMargin(6, 3))
						.ToolTipText(LOCTEXT("UndoTip", "Undo last response (revert file changes)"))
						.OnClicked_Lambda([this]() { if (Controller) Controller->UndoLastMessage(); return FReply::Handled(); })
						.Visibility_Lambda([this]() {
							if (!Controller || Controller->IsThinking()) return EVisibility::Collapsed;
							for (const auto& Msg : Controller->GetHistory())
								if (Msg.Sender == FBobBotChatMessage::ESender::Bot) return EVisibility::Visible;
							return EVisibility::Collapsed;
						})
						[ SNew(SImage).Image(FBobBotStyle::Get().GetBrush("BobBot.Icon.Undo")).ColorAndOpacity(BobBot::Colors::DimGray) ]
					]

					// Interrupt button (during thinking)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 2, 0)
					[
						SAssignNew(StopButton, SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder")
						.ContentPadding(FMargin(6, 3))
						.ToolTipText(LOCTEXT("InterruptTip", "Interrupt response"))
						.OnClicked(this, &SBobBotChatTab::OnStopClicked)
						.Visibility_Lambda([this]() { return (Controller && Controller->IsThinking()) ? EVisibility::Visible : EVisibility::Collapsed; })
						[ SNew(SImage).Image(FBobBotStyle::Get().GetBrush("BobBot.Icon.Stop")).ColorAndOpacity(BobBot::Colors::Red) ]
					]

					// Send button
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SAssignNew(SendButton, SButton)
						.ButtonColorAndOpacity(BobBot::Theme::Primary)
						.ContentPadding(FMargin(8, 4))
						.ToolTipText(LOCTEXT("SendTip", "Send message (Enter)"))
						.OnClicked(this, &SBobBotChatTab::OnSendClicked)
						.IsEnabled(this, &SBobBotChatTab::IsSendEnabled)
						[ SNew(SImage).Image(FBobBotStyle::Get().GetBrush("BobBot.Icon.Send")).ColorAndOpacity(FLinearColor::White) ]
					]
				]
			]
		];
}

// =========================================================================== //
// BuildPermissionDropdown -- Plan / Ask / Auto mode picker
// =========================================================================== //

TSharedRef<SWidget> SBobBotChatTab::BuildPermissionDropdown()
{
	return SNew(SComboButton)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		.ContentPadding(FMargin(6, 2))
		.HasDownArrow(true)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text_Lambda([]() {
				auto Mode = FBobBotConfig::Get().PermissionMode;
				if (Mode == EBobBotPermissionMode::Plan) return LOCTEXT("PlanLabel", "Plan");
				if (Mode == EBobBotPermissionMode::AskBeforeEdits) return LOCTEXT("AskLabel", "Ask");
				return LOCTEXT("AutoLabel", "Auto");
			})
			.Font(BobBot::Theme::FontSmall())
			.ColorAndOpacity_Lambda([]() {
				auto Mode = FBobBotConfig::Get().PermissionMode;
				if (Mode == EBobBotPermissionMode::Plan) return FSlateColor(BobBot::Colors::Yellow);
				if (Mode == EBobBotPermissionMode::AskBeforeEdits) return FSlateColor(BobBot::Colors::Blue);
				return FSlateColor(BobBot::Colors::BotGreen);
			})
		]
		.MenuContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(FMargin(12, 6))
				.OnClicked_Lambda([]() { FBobBotConfig::Get().PermissionMode = EBobBotPermissionMode::Plan; FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); return FReply::Handled(); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("PlanOpt", "Plan")).Font(BobBot::Theme::FontDropdownTitle()).ColorAndOpacity(FSlateColor(BobBot::Colors::Yellow)) ]
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("PlanDesc", "Read-only. BobBot suggests but doesn't execute.")).Font(BobBot::Theme::FontCaption()).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)) ]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(FMargin(12, 6))
				.OnClicked_Lambda([]() { FBobBotConfig::Get().PermissionMode = EBobBotPermissionMode::AskBeforeEdits; FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); return FReply::Handled(); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("AskOpt", "Ask before edits")).Font(BobBot::Theme::FontDropdownTitle()).ColorAndOpacity(FSlateColor(BobBot::Colors::Blue)) ]
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("AskDesc", "Allows reads, asks before writes and creates.")).Font(BobBot::Theme::FontCaption()).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)) ]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(FMargin(12, 6))
				.OnClicked_Lambda([]() { FBobBotConfig::Get().PermissionMode = EBobBotPermissionMode::EditAutomatically; FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); return FReply::Handled(); })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("AutoOpt", "Auto")).Font(BobBot::Theme::FontDropdownTitle()).ColorAndOpacity(FSlateColor(BobBot::Colors::BotGreen)) ]
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("AutoDesc", "BobBot does everything without asking.")).Font(BobBot::Theme::FontCaption()).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)) ]
				]
			]
		];
}

// =========================================================================== //
// BuildModelDropdown -- Sonnet / Opus / Haiku model picker
// =========================================================================== //

TSharedRef<SWidget> SBobBotChatTab::BuildModelDropdown()
{
	return SNew(SComboButton)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		.ContentPadding(FMargin(6, 2))
		.HasDownArrow(true)
		.ToolTipText(LOCTEXT("ModelTip", "Select AI model"))
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text_Lambda([]() {
				FString M = FBobBotConfig::Get().ChatModel;
				if (M == BobBot::ModelNames::Opus) return LOCTEXT("OpusLabel", "Opus");
				if (M == BobBot::ModelNames::Haiku) return LOCTEXT("HaikuLabel", "Haiku");
				return LOCTEXT("SonnetLabel", "Sonnet");
			})
			.Font(BobBot::Theme::FontSmall())
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
		]
		.MenuContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(FMargin(12, 6))
				.OnClicked_Lambda([this]() {
					FBobBotConfig::Get().ChatModel = BobBot::ModelNames::Sonnet; FBobBotConfig::Get().Save();
					FBobBotPythonBridge::Get().ExecCallWithString(TEXT("bob_chat"), TEXT("set_model"), BobBot::ModelNames::Sonnet);
					return FReply::Handled();
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("SonnetOpt", "Sonnet")).Font(BobBot::Theme::FontDropdownTitle()) ]
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("SonnetDesc", "Fast, balanced performance")).Font(BobBot::Theme::FontCaption()).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)) ]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(FMargin(12, 6))
				.OnClicked_Lambda([this]() {
					FBobBotConfig::Get().ChatModel = BobBot::ModelNames::Opus; FBobBotConfig::Get().Save();
					FBobBotPythonBridge::Get().ExecCallWithString(TEXT("bob_chat"), TEXT("set_model"), BobBot::ModelNames::Opus);
					return FReply::Handled();
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("OpusOpt", "Opus")).Font(BobBot::Theme::FontDropdownTitle()) ]
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("OpusDesc", "Most capable, best for complex tasks")).Font(BobBot::Theme::FontCaption()).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)) ]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(FMargin(12, 6))
				.OnClicked_Lambda([this]() {
					FBobBotConfig::Get().ChatModel = BobBot::ModelNames::Haiku; FBobBotConfig::Get().Save();
					FBobBotPythonBridge::Get().ExecCallWithString(TEXT("bob_chat"), TEXT("set_model"), BobBot::ModelNames::Haiku);
					return FReply::Handled();
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("HaikuOpt", "Haiku")).Font(BobBot::Theme::FontDropdownTitle()) ]
					+ SVerticalBox::Slot().AutoHeight() [ SNew(STextBlock).Text(LOCTEXT("HaikuDesc", "Fastest, cheapest, simple tasks")).Font(BobBot::Theme::FontCaption()).ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray)) ]
				]
			]
		];
}

// =========================================================================== //
// BuildSettingsPopover -- gear icon with thinking + effort controls
// =========================================================================== //

TSharedRef<SWidget> SBobBotChatTab::BuildSettingsPopover()
{
	return SNew(SComboButton)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		.ContentPadding(FMargin(4, 2))
		.HasDownArrow(false)
		.ToolTipText(LOCTEXT("SettingsTip", "Thinking & Effort settings"))
		.ButtonContent()
		[
			SNew(SImage)
			.Image(FBobBotStyle::Get().GetBrush("BobBot.Icon.Gear"))
			.ColorAndOpacity(BobBot::Colors::DimGray)
		]
		.MenuContent()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(BobBot::Theme::Overlay)
			.Padding(FMargin(12, 8))
			[
				SNew(SVerticalBox)

				// Thinking section
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[ SNew(STextBlock).Text(LOCTEXT("ThinkHead", "Extended Thinking")).Font(BobBot::Theme::FontDropdownTitle()).ColorAndOpacity(FSlateColor(FLinearColor::White)) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 2, 0)
					[
						SNew(SCheckBox).Style(FAppStyle::Get(), "ToggleButtonCheckbox")
						.IsChecked_Lambda([]() { return FBobBotConfig::Get().ThinkingMode == TEXT("disabled") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([](ECheckBoxState) { FBobBotConfig::Get().ThinkingMode = TEXT("disabled"); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
						[ SNew(STextBlock).Text(LOCTEXT("ThOff", "Off")).Font(BobBot::Theme::FontSmall()).Margin(FMargin(6, 2)) ]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 2, 0)
					[
						SNew(SCheckBox).Style(FAppStyle::Get(), "ToggleButtonCheckbox")
						.IsChecked_Lambda([]() { return FBobBotConfig::Get().ThinkingMode == TEXT("enabled") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([](ECheckBoxState) { FBobBotConfig::Get().ThinkingMode = TEXT("enabled"); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
						[ SNew(STextBlock).Text(LOCTEXT("ThOn", "On")).Font(BobBot::Theme::FontSmall()).Margin(FMargin(6, 2)) ]
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SCheckBox).Style(FAppStyle::Get(), "ToggleButtonCheckbox")
						.IsChecked_Lambda([]() { return FBobBotConfig::Get().ThinkingMode == TEXT("adaptive") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([](ECheckBoxState) { FBobBotConfig::Get().ThinkingMode = TEXT("adaptive"); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
						[ SNew(STextBlock).Text(LOCTEXT("ThAdapt", "Adaptive")).Font(BobBot::Theme::FontSmall()).Margin(FMargin(6, 2)) ]
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4) [ SNew(SSeparator) ]

				// Effort section
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 6)
				[ SNew(STextBlock).Text(LOCTEXT("EffortHead", "Effort Level")).Font(BobBot::Theme::FontDropdownTitle()).ColorAndOpacity(FSlateColor(FLinearColor::White)) ]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 2, 0)
					[
						SNew(SCheckBox).Style(FAppStyle::Get(), "ToggleButtonCheckbox")
						.IsChecked_Lambda([]() { return FBobBotConfig::Get().EffortLevel == TEXT("low") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([](ECheckBoxState) { FBobBotConfig::Get().EffortLevel = TEXT("low"); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
						[ SNew(STextBlock).Text(LOCTEXT("EfLow", "Low")).Font(BobBot::Theme::FontSmall()).Margin(FMargin(6, 2)) ]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 2, 0)
					[
						SNew(SCheckBox).Style(FAppStyle::Get(), "ToggleButtonCheckbox")
						.IsChecked_Lambda([]() { return FBobBotConfig::Get().EffortLevel == TEXT("medium") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([](ECheckBoxState) { FBobBotConfig::Get().EffortLevel = TEXT("medium"); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
						[ SNew(STextBlock).Text(LOCTEXT("EfMed", "Medium")).Font(BobBot::Theme::FontSmall()).Margin(FMargin(6, 2)) ]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 2, 0)
					[
						SNew(SCheckBox).Style(FAppStyle::Get(), "ToggleButtonCheckbox")
						.IsChecked_Lambda([]() { return FBobBotConfig::Get().EffortLevel == TEXT("high") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([](ECheckBoxState) { FBobBotConfig::Get().EffortLevel = TEXT("high"); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
						[ SNew(STextBlock).Text(LOCTEXT("EfHigh", "High")).Font(BobBot::Theme::FontSmall()).Margin(FMargin(6, 2)) ]
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SCheckBox).Style(FAppStyle::Get(), "ToggleButtonCheckbox")
						.IsChecked_Lambda([]() { return FBobBotConfig::Get().EffortLevel == TEXT("max") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([](ECheckBoxState) { FBobBotConfig::Get().EffortLevel = TEXT("max"); FBobBotConfig::Get().Save(); FBobBotConfig::Get().ApplyEnvironmentVars(); })
						[ SNew(STextBlock).Text(LOCTEXT("EfMax", "Max")).Font(BobBot::Theme::FontSmall()).Margin(FMargin(6, 2)) ]
					]
				]
			]
		];
}


// =========================================================================== //
// Chat header
// =========================================================================== //

FText SBobBotChatTab::GetChatHeaderText() const
{
	if (!Controller)
		return LOCTEXT("ChatHeaderNA", "No controller");

	return FText::Format(LOCTEXT("ChatHeader", "{0}  |  {1} msgs  |  "),
		FText::FromString(FBobBotConfig::Get().ChatModel),
		FText::AsNumber(Controller->GetMessageCount()));
}

FText SBobBotChatTab::GetCostHeaderText() const
{
	if (!Controller)
		return FText::GetEmpty();

	float Cost = Controller->GetSessionCost();
	float Budget = FBobBotConfig::Get().MaxBudgetUsd;

	if (Controller->IsThinking() && Budget > 0.f)
		return FText::FromString(FString::Printf(TEXT("$%.2f/$%.2f"), Cost, Budget));
	return FText::FromString(FString::Printf(TEXT("$%.2f"), Cost));
}

FSlateColor SBobBotChatTab::GetCostHeaderColor() const
{
	if (!Controller)
		return FSlateColor(BobBot::Colors::DimGray);

	float Budget = FBobBotConfig::Get().MaxBudgetUsd;
	if (Budget <= 0.f)
		return FSlateColor(BobBot::Colors::DimGray);

	float Ratio = Controller->GetSessionCost() / Budget;
	if (Ratio >= 1.0f) return FSlateColor(BobBot::Colors::Red);
	if (Ratio >= 0.95f) return FSlateColor(BobBot::Colors::ErrorOrange);
	if (Ratio >= 0.80f) return FSlateColor(BobBot::Colors::Yellow);
	return FSlateColor(BobBot::Colors::DimGray);
}

FText SBobBotChatTab::GetContextHeaderText() const
{
	if (!Controller || Controller->GetContextTokensMax() <= 0)
		return FText::GetEmpty();

	return FText::FromString(FString::Printf(TEXT("  |  %.0f%% ctx"), Controller->GetContextPercent()));
}

FSlateColor SBobBotChatTab::GetContextHeaderColor() const
{
	if (!Controller)
		return FSlateColor(BobBot::Colors::DimGray);

	float Pct = Controller->GetContextPercent();
	if (Pct >= 95.f) return FSlateColor(BobBot::Colors::Red);
	if (Pct >= 85.f) return FSlateColor(BobBot::Colors::ErrorOrange);
	if (Pct >= 70.f) return FSlateColor(BobBot::Colors::Yellow);
	return FSlateColor(BobBot::Colors::DimGray);
}

EVisibility SBobBotChatTab::GetContextVisibility() const
{
	return (Controller && Controller->GetContextTokensMax() > 0) ? EVisibility::Visible : EVisibility::Collapsed;
}

// =========================================================================== //
// Send / Clear / Stop
// =========================================================================== //

bool SBobBotChatTab::IsSendEnabled() const
{
	return Controller && !Controller->IsThinking();
}

FReply SBobBotChatTab::OnSendClicked()
{
	if (!CommandInput.IsValid() || !Controller) return FReply::Handled();

	FString Message = CommandInput->GetText().ToString().TrimStartAndEnd();
	if (Message.IsEmpty()) return FReply::Handled();

	CommandInput->SetText(FText::GetEmpty());
	Controller->SendMessage(Message);
	return FReply::Handled();
}

FReply SBobBotChatTab::OnClearChatClicked()
{
	if (Controller)
		Controller->ClearSession();
	return FReply::Handled();
}

FReply SBobBotChatTab::OnStopClicked()
{
	if (Controller)
		Controller->StopChat();
	return FReply::Handled();
}

// =========================================================================== //
// Clipboard
// =========================================================================== //

// =========================================================================== //
// Markdown to SRichTextBlock markup converter
// =========================================================================== //

/**
 * Convert inline markdown to SRichTextBlock markup tags.
 * Handles **bold**, *italic*, and `inline code`.
 * Does NOT handle block-level elements (those are handled by the code block splitter).
 */
static FString MarkdownToRichText(const FString& Markdown)
{
	FString Result;
	Result.Reserve(Markdown.Len() * 2);

	const TCHAR* Ptr = *Markdown;
	const TCHAR* End = Ptr + Markdown.Len();

	while (Ptr < End)
	{
		// ** bold **
		if (Ptr + 1 < End && Ptr[0] == TEXT('*') && Ptr[1] == TEXT('*'))
		{
			Ptr += 2;
			const TCHAR* Start = Ptr;
			while (Ptr + 1 < End && !(Ptr[0] == TEXT('*') && Ptr[1] == TEXT('*')))
				Ptr++;
			FString Inner(UE_PTRDIFF_TO_INT32(Ptr - Start), Start);
			Result += TEXT("<BobBot.Bold>");
			Result += Inner;
			Result += TEXT("</>");
			if (Ptr + 1 < End)
				Ptr += 2; // skip closing **
			continue;
		}

		// `inline code`
		if (Ptr[0] == TEXT('`'))
		{
			Ptr++;
			const TCHAR* Start = Ptr;
			while (Ptr < End && Ptr[0] != TEXT('`'))
				Ptr++;
			FString Inner(UE_PTRDIFF_TO_INT32(Ptr - Start), Start);
			Result += TEXT("<BobBot.Code>");
			Result += Inner;
			Result += TEXT("</>");
			if (Ptr < End)
				Ptr++; // skip closing `
			continue;
		}

		// * italic *
		if (Ptr[0] == TEXT('*'))
		{
			Ptr++;
			const TCHAR* Start = Ptr;
			while (Ptr < End && Ptr[0] != TEXT('*'))
				Ptr++;
			FString Inner(UE_PTRDIFF_TO_INT32(Ptr - Start), Start);
			Result += TEXT("<BobBot.Italic>");
			Result += Inner;
			Result += TEXT("</>");
			if (Ptr < End)
				Ptr++; // skip closing *
			continue;
		}

		// Escape angle brackets that would confuse the markup parser
		if (Ptr[0] == TEXT('<'))
		{
			Result += TEXT("&lt;");
			Ptr++;
			continue;
		}
		if (Ptr[0] == TEXT('>'))
		{
			Result += TEXT("&gt;");
			Ptr++;
			continue;
		}

		Result += Ptr[0];
		Ptr++;
	}

	return Result;
}

// =========================================================================== //
// Code block / prose widget helpers
// =========================================================================== //

void SBobBotChatTab::AddCodeBlockWidget(SVerticalBox& ContentBox, const FString& Text)
{
	ContentBox.AddSlot().AutoHeight().Padding(0, 2)
	[
		SNew(SBorder)
		.BorderBackgroundColor(BobBot::Colors::CodeBlockBg)
		.Padding(FMargin(8, 4))
		[
			SNew(SMultiLineEditableTextBox)
			.Text(FText::FromString(Text))
			.Font(BobBot::Theme::FontCode())
			.AutoWrapText(true)
			.ForegroundColor(FSlateColor(BobBot::Theme::TextCode))
			.BackgroundColor(FLinearColor::Transparent)
			.ReadOnlyForegroundColor(FSlateColor(BobBot::Theme::TextCode))
			.IsReadOnly(true)
		]
	];
}

void SBobBotChatTab::AddProseWidget(SVerticalBox& ContentBox, const FString& Text)
{
	FString Markup = MarkdownToRichText(Text);
	ContentBox.AddSlot().AutoHeight()
	[
		SNew(SMultiLineEditableTextBox)
		.Text(FText::FromString(Markup))
		.Marshaller(FRichTextLayoutMarshaller::Create(TArray<TSharedRef<ITextDecorator>>(), &FBobBotStyle::Get()))
		.Font(BobBot::Theme::FontBody())
		.ForegroundColor(FSlateColor(BobBot::Theme::TextPrimary))
		.BackgroundColor(FLinearColor::Transparent)
		.ReadOnlyForegroundColor(FSlateColor(BobBot::Theme::TextPrimary))
		.AutoWrapText(true)
		.IsReadOnly(true)
	];
}

// =========================================================================== //
// Message content widget builder
// =========================================================================== //

TSharedRef<SWidget> SBobBotChatTab::BuildMessageContentWidget(const FString& Content, FBobBotChatMessage::ESender Sender)
{
	// For non-bot messages, use read-only editable text (supports drag-select + Ctrl+C)
	if (Sender != FBobBotChatMessage::ESender::Bot)
	{
		using namespace BobBot::Theme;
		FSlateFontInfo Font =
			Sender == FBobBotChatMessage::ESender::System ? FontSmall() : FontBody();
		FSlateColor TextColor =
			Sender == FBobBotChatMessage::ESender::Error ? FSlateColor(ErrorAccent) :
			Sender == FBobBotChatMessage::ESender::System ? FSlateColor(TextSecondary) :
			FSlateColor(TextPrimary);

		return SNew(SMultiLineEditableTextBox)
			.Text(FText::FromString(Content))
			.Font(Font)
			.ForegroundColor(TextColor)
			.BackgroundColor(FLinearColor::Transparent)
			.ReadOnlyForegroundColor(TextColor)
			.AutoWrapText(true)
			.IsReadOnly(true);
	}

	// Split on ``` delimiters: code blocks get monospace, prose gets SRichTextBlock
	TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox);
	FString Remaining = Content;
	bool bInCodeBlock = false;

	while (!Remaining.IsEmpty())
	{
		int32 TickIdx = Remaining.Find(TEXT("```"));
		if (TickIdx == INDEX_NONE)
		{
			FString Segment = Remaining.TrimStartAndEnd();
			if (!Segment.IsEmpty())
			{
				if (bInCodeBlock)
					AddCodeBlockWidget(*ContentBox, Segment);
				else
					AddProseWidget(*ContentBox, Segment);
			}
			break;
		}

		FString Before = Remaining.Left(TickIdx).TrimEnd();
		if (!Before.IsEmpty())
		{
			if (bInCodeBlock)
				AddCodeBlockWidget(*ContentBox, Before);
			else
				AddProseWidget(*ContentBox, Before);
		}

		Remaining = Remaining.Mid(TickIdx + 3);
		if (!bInCodeBlock)
		{
			int32 NewlineIdx = Remaining.Find(TEXT("\n"));
			if (NewlineIdx != INDEX_NONE)
				Remaining = Remaining.Mid(NewlineIdx + 1);
		}
		bInCodeBlock = !bInCodeBlock;
	}

	return ContentBox;
}

// =========================================================================== //
// Chat message widget builder
// =========================================================================== //

TSharedRef<SWidget> SBobBotChatTab::BuildChatMessageWidget(const FBobBotChatMessage& Msg)
{
	using namespace BobBot::Theme;

	// Dispatch to specialized builders
	if (Msg.Sender == FBobBotChatMessage::ESender::ToolCall)
		return BuildToolCallWidget(Msg);
	if (Msg.Sender == FBobBotChatMessage::ESender::Subagent)
		return BuildSubagentWidget(Msg);

	// System messages — minimal, no sender label, selectable
	if (Msg.Sender == FBobBotChatMessage::ESender::System)
	{
		return SNew(SMultiLineEditableTextBox)
			.Text(FText::FromString(Msg.Content))
			.Font(FontSmall())
			.ForegroundColor(FSlateColor(TextSecondary))
			.BackgroundColor(FLinearColor::Transparent)
			.ReadOnlyForegroundColor(FSlateColor(TextSecondary))
			.AutoWrapText(true)
			.IsReadOnly(true);
	}

	// Determine sender info
	FString SenderLabel;
	FLinearColor SenderColor;
	FString MetaText;  // right-aligned info (timestamp or cost)

	FString TimeStr = Msg.Timestamp.ToString(TEXT("%H:%M:%S"));
	FString MsgContent = Msg.Content;

	switch (Msg.Sender)
	{
	case FBobBotChatMessage::ESender::User:
		SenderLabel = TEXT("You");
		SenderColor = Info;
		MetaText = TimeStr;
		break;
	case FBobBotChatMessage::ESender::Bot:
		SenderLabel = TEXT("BobBot");
		SenderColor = BotAccent;
		MetaText = Msg.Cost > 0.001f ? FString::Printf(TEXT("$%.4f"), Msg.Cost) : TimeStr;
		break;
	case FBobBotChatMessage::ESender::Error:
		SenderLabel = TEXT("Error");
		SenderColor = ErrorAccent;
		MetaText = TimeStr;
		break;
	case FBobBotChatMessage::ESender::Approval:
		SenderLabel = TEXT("Tool Request");
		SenderColor = Warning;
		MetaText = TimeStr;
		break;
	default:
		SenderLabel = TEXT("System");
		SenderColor = TextSecondary;
		MetaText = TimeStr;
		break;
	}

	// Tooltip: full details
	FString Tooltip = TimeStr;
	if (Msg.Sender == FBobBotChatMessage::ESender::Bot && Msg.Cost > 0.f)
	{
		Tooltip += FString::Printf(TEXT("  |  $%.4f"), Msg.Cost);
		if (Msg.DurationMs > 0)
			Tooltip += FString::Printf(TEXT("  |  %.1fs"), Msg.DurationMs / 1000.f);
		if (Msg.NumTurns > 1)
			Tooltip += FString::Printf(TEXT("  |  %d turns"), Msg.NumTurns);
	}

	// Build message content
	TSharedRef<SVerticalBox> MessageBox = SNew(SVerticalBox)
		// Sender line: "You" ................ "12:34"
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(STextBlock).Text(FText::FromString(SenderLabel))
				.Font(FontHeading())
				.ColorAndOpacity(FSlateColor(SenderColor))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f) [ SNullWidget::NullWidget ]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(STextBlock).Text(FText::FromString(MetaText))
				.Font(FontCaption())
				.ColorAndOpacity(FSlateColor(TextSecondary))
				.ToolTipText(FText::FromString(Tooltip))
			]
		]
		// Message content
		+ SVerticalBox::Slot().AutoHeight().Padding(0, SpaceXS, 0, 0)
		[
			BuildMessageContentWidget(Msg.Content, Msg.Sender)
		];

	// Inline image preview (for bot messages referencing captured images)
	if (Msg.Sender == FBobBotChatMessage::ESender::Bot)
	{
		FString ImagePath = ExtractImagePath(Msg.Content);
		if (!ImagePath.IsEmpty())
		{
			MessageBox->AddSlot().AutoHeight().Padding(0, SpaceSM, 0, 0)
			[
				BuildInlineImageWidget(ImagePath)
			];
		}
	}

	// Error messages — add orange left accent bar
	if (Msg.Sender == FBobBotChatMessage::ESender::Error)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
				.BorderBackgroundColor(ErrorAccent)
				.Padding(FMargin(1.5f, 0.f))
				[ SNullWidget::NullWidget ]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(SpaceSM, 0, 0, 0)
			[ MessageBox ];
	}

	return MessageBox;
}

TSharedRef<SWidget> SBobBotChatTab::BuildToolCallWidget(const FBobBotChatMessage& Msg)
{
	using namespace BobBot::Theme;

	FLinearColor StatusColor = Msg.bToolComplete ? Success : Warning;
	FString StatusIcon = Msg.bToolComplete ? TEXT("done") : TEXT("...");
	FString HeaderText = FString::Printf(TEXT("%s %s"), *StatusIcon, *Msg.ToolName);
	if (Msg.bToolComplete && Msg.DurationMs > 0)
		HeaderText += FString::Printf(TEXT("  %.1fs"), Msg.DurationMs / 1000.f);

	TSharedRef<SWidget> CodeContent = SNullWidget::NullWidget;
	if (!Msg.ToolInput.IsEmpty())
	{
		CodeContent = SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(Overlay)
			.Padding(PadCodeBlock())
			[
				SNew(STextBlock).Text(FText::FromString(Msg.ToolInput))
				.Font(FontCodeSmall())
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(TextCode))
			];
	}

	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
		.BorderBackgroundColor(Surface)
		.Padding(FMargin(SpaceSM, SpaceXS))
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(Msg.bToolComplete)
			.BorderBackgroundColor(Surface)
			.HeaderPadding(FMargin(SpaceSM, SpaceXS))
			.HeaderContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(HeaderText))
				.Font(FontDropdownTitle())
				.ColorAndOpacity(FSlateColor(StatusColor))
			]
			.BodyContent()
			[ CodeContent ]
		];
}

// =========================================================================== //
// Build subagent widget — colored accent bar + expandable area
// =========================================================================== //

TSharedRef<SWidget> SBobBotChatTab::BuildSubagentWidget(const FBobBotChatMessage& Msg)
{
	bool bRunning = (Msg.SubagentStatus == TEXT("running"));
	bool bFailed = (Msg.SubagentStatus == TEXT("failed") || Msg.SubagentStatus == TEXT("stopped"));
	FLinearColor AccentColor = bRunning ? BobBot::Colors::Yellow
		: bFailed ? BobBot::Colors::Red
		: BobBot::Colors::Green;

	FString HeaderText = Msg.SubagentDescription;
	if (Msg.SubagentToolUses > 0)
		HeaderText += FString::Printf(TEXT("  |  %d tools"), Msg.SubagentToolUses);
	if (Msg.SubagentDurationMs > 0)
		HeaderText += FString::Printf(TEXT("  |  %.1fs"), Msg.SubagentDurationMs / 1000.f);

	// Build body: tool call list + summary
	TSharedRef<SVerticalBox> BodyBox = SNew(SVerticalBox);

	// Nested tool calls
	for (const FBobBotChatMessage& Tool : Msg.SubagentToolCalls)
	{
		FLinearColor ToolColor = Tool.bToolComplete ? BobBot::Colors::Green : BobBot::Colors::Yellow;
		FString ToolLine = FString::Printf(TEXT("  %s  %s"),
			*Tool.ToolName, Tool.bToolComplete ? TEXT("Complete") : TEXT("Running"));
		BodyBox->AddSlot().AutoHeight().Padding(0, 1)
		[
			SNew(STextBlock)
			.Text(FText::FromString(ToolLine))
			.Font(BobBot::Theme::FontCodeSmall())
			.ColorAndOpacity(FSlateColor(ToolColor))
		];
	}

	// Summary (shown on completion)
	if (!Msg.SubagentSummary.IsEmpty())
	{
		BodyBox->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Msg.SubagentSummary))
			.Font(BobBot::Theme::FontSmall())
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
		];
	}
	else if (bRunning && Msg.SubagentToolCalls.Num() == 0)
	{
		BodyBox->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SubagentWorking", "Working..."))
			.Font(BobBot::Theme::FontSmall())
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
		];
	}

	TSharedRef<SWidget> BodyContent = SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
		.BorderBackgroundColor(BobBot::Colors::CodeBlockBg)
		.Padding(FMargin(8, 4))
		[ BodyBox ];

	return SNew(SHorizontalBox)
		// Left accent bar
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(AccentColor)
			.Padding(FMargin(1.5f, 0))
			[ SNullWidget::NullWidget ]
		]
		// Expandable content
		+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4, 0, 0, 0)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(!bRunning)
			.BorderBackgroundColor(BobBot::Theme::Surface)
			.HeaderPadding(FMargin(4, 2))
			.Padding(FMargin(0))
			.HeaderContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(HeaderText))
					.Font(BobBot::Theme::FontDropdownTitle())
					.ColorAndOpacity(FSlateColor(AccentColor))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[
					SNew(SButton)
					.ButtonStyle(FCoreStyle::Get(), "NoBorder")
					.ContentPadding(BobBot::Theme::PadButton())
					.ToolTipText(LOCTEXT("StopTaskTip", "Cancel this subagent task"))
					[SNew(SImage).Image(FBobBotStyle::Get().GetBrush("BobBot.Icon.Stop")).ColorAndOpacity(BobBot::Colors::Red)]
					.Visibility(bRunning ? EVisibility::Visible : EVisibility::Collapsed)
					.OnClicked_Lambda([TaskId = Msg.SubagentTaskId]() {
						FString Script = FString::Printf(
							TEXT("import bob_chat; bob_chat.stop_task('%s')\n"), *TaskId);
						FBobBotPythonBridge::Get().ExecPythonCommand(Script);
						return FReply::Handled();
					})
				]
			]
			.BodyContent()
			[
				BodyContent
			]
		];
}

// =========================================================================== //
// Rebuild chat messages
// =========================================================================== //

void SBobBotChatTab::RebuildChatMessages()
{
	if (!ChatMessagesBox.IsValid() || !Controller) return;
	ChatMessagesBox->ClearChildren();

	const TArray<FBobBotChatMessage>& History = Controller->GetHistory();

	// Helper: emit a collapsed group for consecutive auto-approved tool calls
	auto EmitAutoApprovedGroup = [this](const TArray<FBobBotChatMessage>& Hist, int32 StartIdx, int32 Count)
	{
		FString HeaderText = FString::Printf(TEXT("Auto-approved: %d tools"), Count);
		TSharedRef<SVerticalBox> ToolList = SNew(SVerticalBox);
		for (int32 j = StartIdx; j < StartIdx + Count; ++j)
		{
			const FBobBotChatMessage& T = Hist[j];
			FString Line = T.ToolName;
			if (T.bToolComplete && T.DurationMs > 0)
				Line += FString::Printf(TEXT("  (%.1fs)"), T.DurationMs / 1000.f);
			ToolList->AddSlot().AutoHeight().Padding(4, 1)
			[
				SNew(STextBlock).Text(FText::FromString(Line))
				.Font(BobBot::Theme::FontCodeSmall())
				.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
			];
		}

		ChatMessagesBox->AddSlot().AutoHeight().Padding(8, 2)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(true)
			.BorderBackgroundColor(BobBot::Theme::Surface)
			.HeaderPadding(FMargin(6, 2))
			.Padding(FMargin(0))
			.HeaderContent()
			[
				SNew(STextBlock).Text(FText::FromString(HeaderText))
				.Font(BobBot::Theme::FontCaption())
				.ColorAndOpacity(FSlateColor(BobBot::Colors::Green))
			]
			.BodyContent()
			[
				ToolList
			]
		];
	};

	for (int32 MsgIdx = 0; MsgIdx < History.Num(); ++MsgIdx)
	{
		const FBobBotChatMessage& Msg = History[MsgIdx];

		// Group consecutive auto-approved tool calls
		if (Msg.Sender == FBobBotChatMessage::ESender::ToolCall
			&& FBobBotChatController::IsToolAutoApproved(Msg.ToolName))
		{
			int32 RunStart = MsgIdx;
			int32 RunCount = 1;
			while (MsgIdx + 1 < History.Num()
				&& History[MsgIdx + 1].Sender == FBobBotChatMessage::ESender::ToolCall
				&& FBobBotChatController::IsToolAutoApproved(History[MsgIdx + 1].ToolName))
			{
				++MsgIdx;
				++RunCount;
			}
			if (RunCount >= 2)
			{
				EmitAutoApprovedGroup(History, RunStart, RunCount);
				continue;
			}
			// Single auto-approved tool — render normally
			MsgIdx = RunStart;
		}

		TSharedRef<SWidget> MessageWidget = BuildChatMessageWidget(Msg);

		// Approval buttons — only on the most recent Approval message while pending
		if (Msg.Sender == FBobBotChatMessage::ESender::Approval && Controller->HasPendingApproval()
			&& MsgIdx == History.Num() - 1)
		{
			FString Category = Controller->GetPendingApprovalCategory();
			FString CategoryDisplay;
			if (Category == TEXT("read_only")) CategoryDisplay = TEXT("Read-only");
			else if (Category == TEXT("viewport")) CategoryDisplay = TEXT("Viewport");
			else if (Category == TEXT("create")) CategoryDisplay = TEXT("Create");
			else if (Category == TEXT("modify")) CategoryDisplay = TEXT("Modify");
			else if (Category == TEXT("code_exec")) CategoryDisplay = TEXT("Code execution");
			else CategoryDisplay = TEXT("Unknown");

			TSharedRef<SVerticalBox> ApprovalBox = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight() [ MessageWidget ]
				+ SVerticalBox::Slot().AutoHeight().Padding(4, 4, 0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("ApproveBtn", "Approve"))
						.OnClicked_Lambda([this]() { if (Controller) Controller->ApproveExecution(); return FReply::Handled(); })
						.ButtonColorAndOpacity(BobBot::Theme::ApproveGreen)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("DenyBtn", "Deny"))
						.OnClicked_Lambda([this]() { if (Controller) Controller->DenyExecution(); return FReply::Handled(); })
						.ButtonColorAndOpacity(BobBot::Theme::DenyRed)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(FText::Format(LOCTEXT("AlwaysAllow", "Always allow {0}"), FText::FromString(CategoryDisplay)))
						.OnClicked_Lambda([this, Category]()
						{
							if (!Controller) return FReply::Handled();
							// Toggle the auto-approve config for this category
							FBobBotConfig& Cfg = FBobBotConfig::Get();
							if (Category == TEXT("read_only")) Cfg.bAutoApproveReadOnly = true;
							else if (Category == TEXT("viewport")) Cfg.bAutoApproveViewport = true;
							else if (Category == TEXT("create")) Cfg.bAutoApproveCreate = true;
							else if (Category == TEXT("modify")) Cfg.bAutoApproveModify = true;
							else if (Category == TEXT("code_exec")) Cfg.bAutoApproveCodeExec = true;
							Cfg.Save();
							Cfg.ApplyEnvironmentVars();
							Controller->ApproveExecution();
							return FReply::Handled();
						})
						.ToolTipText(FText::Format(LOCTEXT("AlwaysAllowTip", "Auto-approve all {0} tools from now on"), FText::FromString(CategoryDisplay)))
					]
				];
			MessageWidget = ApprovalBox;
		}

		// Add separator between user→bot exchanges
		if (Msg.Sender == FBobBotChatMessage::ESender::User && MsgIdx > 0)
		{
			const auto PrevSender = History[MsgIdx - 1].Sender;
			if (PrevSender == FBobBotChatMessage::ESender::Bot
				|| PrevSender == FBobBotChatMessage::ESender::ToolCall
				|| PrevSender == FBobBotChatMessage::ESender::Subagent)
			{
				ChatMessagesBox->AddSlot().AutoHeight().Padding(BobBot::Theme::SpaceMD, BobBot::Theme::SpaceSM)
				[ SNew(SSeparator) ];
			}
		}

		ChatMessagesBox->AddSlot().AutoHeight().Padding(BobBot::Theme::PadMessage())
		[
			MessageWidget
		];
	}

	// Quickstart suggestion chips (shown only on fresh conversations)
	if (ShouldShowQuickstartChips())
	{
		ChatMessagesBox->AddSlot().AutoHeight().Padding(16, 8, 16, 4)
		[
			SNew(SWrapBox).UseAllottedSize(true)
			+ SWrapBox::Slot().Padding(4, 2) [ BuildQuickstartChip(LOCTEXT("QS1", "Set up a basic level"), TEXT("Set up a basic level with a floor, some lighting, and a player start")) ]
			+ SWrapBox::Slot().Padding(4, 2) [ BuildQuickstartChip(LOCTEXT("QS2", "Show me what you can do"), TEXT("Show me what you can do")) ]
			+ SWrapBox::Slot().Padding(4, 2) [ BuildQuickstartChip(LOCTEXT("QS3", "Create a material"), TEXT("Create a simple material with a base color and roughness")) ]
			+ SWrapBox::Slot().Padding(4, 2) [ BuildQuickstartChip(LOCTEXT("QS4", "Describe my project"), TEXT("Describe my project - what's in it, what map is loaded, and what's in the level")) ]
		];
	}

	// Show thinking indicator as a real message in the chat
	if (Controller->IsThinking())
	{
		FString Dots;
		for (int32 i = 0; i < (Controller->GetThinkingDotCount() % 3) + 1; i++)
			Dots += TEXT(".");

		ChatMessagesBox->AddSlot().AutoHeight().Padding(BobBot::Theme::PadMessage())
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(LOCTEXT("BotThinking", "BobBot"))
				.Font(BobBot::Theme::FontHeading())
				.ColorAndOpacity(FSlateColor(BobBot::Theme::BotAccent))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, BobBot::Theme::SpaceXS, 0, 0)
			[
				SAssignNew(ThinkingTextBlock, STextBlock)
				.Text(FText::Format(LOCTEXT("ThinkingDots", "Thinking{0}"), FText::FromString(Dots)))
				.Font(BobBot::Theme::FontItalic())
				.ColorAndOpacity(FSlateColor(BobBot::Theme::TextSecondary))
			]
		];
	}

	if (ChatScrollBox.IsValid())
		ChatScrollBox->ScrollToEnd();
}

// =========================================================================== //
// Chat list (dropdown)
// =========================================================================== //

void SBobBotChatTab::RebuildChatList()
{
	if (!ChatListBox.IsValid() || !Controller) return;
	ChatListBox->ClearChildren();

	// Refresh from SDK sessions
	Controller->RefreshChatIndex();
	const TArray<FBobBotChatEntry>& Index = Controller->GetChatIndex();
	FString ActiveId = Controller->GetActiveChatId();

	// Capture locals for use in nested lambdas (MSVC doesn't allow `this` in nested lambdas)
	FBobBotChatController* Ctrl = Controller;
	SBobBotChatTab* Self = this;

	// Helper to add a chat entry row (supports indentation for branches)
	auto AddChatRow = [&, Ctrl, Self](const FBobBotChatEntry& Entry, int32 IndentLevel)
	{
		FString ChatId = Entry.Id;
		bool bIsActive = (ChatId == ActiveId);
		FLinearColor TextColor = bIsActive ? BobBot::Colors::ActiveBlue : FLinearColor::White;

		FString DisplayTitle = Entry.Title;
		if (IndentLevel > 0)
			DisplayTitle = FString::Printf(TEXT("  %s"), *Entry.Title);

		float LeftPad = IndentLevel > 0 ? 20.f : 0.f;

		ChatListBox->AddSlot().AutoHeight().Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(LeftPad, 0, 0, 0)
			[
				SNew(SButton)
				.ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(4, 2))
				.OnClicked_Lambda([Ctrl, ChatId]()
				{
					if (Ctrl) Ctrl->SwitchChat(ChatId);
					return FReply::Handled();
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(DisplayTitle))
						.Font(IndentLevel > 0 ? BobBot::Theme::FontCaption() : BobBot::Theme::FontSmall())
						.ColorAndOpacity(FSlateColor(IndentLevel > 0 ? BobBot::Colors::DimGray : TextColor))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("%s  $%.2f"), *Entry.Model, Entry.Cost)))
						.Font(BobBot::Theme::FontCaption())
						.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
					]
				]
			]
			// Rename button (active chat only)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(4, 0))
				.ToolTipText(LOCTEXT("RenameChatTip", "Rename conversation"))
				.Visibility(bIsActive ? EVisibility::Visible : EVisibility::Collapsed)
				.OnClicked_Lambda([Ctrl, Self, ChatId]()
				{
					TSharedRef<SWindow> RenameWin = SNew(SWindow)
						.Title(LOCTEXT("RenameTitle", "Rename Chat"))
						.ClientSize(FVector2D(340, 100))
						.SupportsMinimize(false).SupportsMaximize(false)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().FillHeight(1.f).Padding(12)
							[
								SNew(SEditableTextBox)
								.HintText(LOCTEXT("RenameHint", "Enter new title..."))
								.OnTextCommitted_Lambda([Self, ChatId](const FText& Text, ETextCommit::Type Type)
								{
									if (Type != ETextCommit::OnCleared && !Text.IsEmpty())
									{
										FString Script = FString::Printf(
											TEXT("import bob_chat, json\nopen(output_path, 'w').write(json.dumps(bob_chat.rename_saved_session('%s', '%s')))\n"),
											*ChatId, *Text.ToString().Replace(TEXT("'"), TEXT("\\'")));
										FBobBotPythonBridge::Get().ExecPythonWithJsonResult(Script, TEXT("_rename_result.txt"));
										Self->RebuildChatList();
									}
								})
							]
						];
					FSlateApplication::Get().AddWindow(RenameWin);
					return FReply::Handled();
				})
				[
					SNew(SImage)
					.Image(FBobBotStyle::Get().GetBrush("BobBot.Icon.Edit"))
					.ColorAndOpacity(BobBot::Colors::DimGray)
				]
			]
			// Delete button (non-active chats only)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(4, 0))
				.ToolTipText(LOCTEXT("DeleteChatTip", "Delete conversation"))
				.Visibility(bIsActive ? EVisibility::Collapsed : EVisibility::Visible)
				.OnClicked_Lambda([Ctrl, Self, ChatId]()
				{
					if (Ctrl) Ctrl->DeleteChat(ChatId);
					Self->RebuildChatList();
					return FReply::Handled();
				})
				[
					SNew(SImage)
					.Image(FBobBotStyle::Get().GetBrush("BobBot.Icon.Trash"))
					.ColorAndOpacity(BobBot::Colors::Red)
				]
			]
		];
	};

	// Tree view: roots first, then their children indented
	TSet<FString> Emitted;
	for (const FBobBotChatEntry& Entry : Index)
	{
		if (!Entry.ParentId.IsEmpty()) continue;  // Skip children in first pass
		AddChatRow(Entry, 0);
		Emitted.Add(Entry.Id);

		// Emit branches under this root
		for (const FBobBotChatEntry& Child : Index)
		{
			if (Child.ParentId == Entry.Id)
			{
				AddChatRow(Child, 1);
				Emitted.Add(Child.Id);
			}
		}
	}
	// Emit orphans (parent deleted) as roots
	for (const FBobBotChatEntry& Entry : Index)
	{
		if (!Emitted.Contains(Entry.Id))
			AddChatRow(Entry, 0);
	}

	if (Index.Num() == 0)
	{
		ChatListBox->AddSlot().AutoHeight().Padding(8, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoChats", "No conversations"))
			.Font(BobBot::Theme::FontItalicSmall())
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
		];
	}
}

// =========================================================================== //
// Thinking indicator update
// =========================================================================== //

void SBobBotChatTab::UpdateThinkingIndicator()
{
	if (!Controller) return;

	bool bNowThinking = Controller->IsThinking();

	if (bNowThinking && !bWasThinking)
	{
		// Just started thinking — rebuild to show thinking message
		bWasThinking = true;
		RebuildChatMessages();
	}
	else if (!bNowThinking && bWasThinking)
	{
		// Stopped thinking — rebuild to remove thinking message
		bWasThinking = false;
		RebuildChatMessages();
	}
	else if (bNowThinking && ThinkingTextBlock.IsValid())
	{
		// Update thinking dots in-place
		FString Dots;
		for (int32 i = 0; i < (Controller->GetThinkingDotCount() % 3) + 1; i++)
			Dots += TEXT(".");
		ThinkingTextBlock->SetText(FText::Format(LOCTEXT("ThinkingDotsUpdate", "Thinking{0}"), FText::FromString(Dots)));
	}
}

// =========================================================================== //
// Delegate handlers
// =========================================================================== //

void SBobBotChatTab::OnMessageAdded(const FBobBotChatMessage& Msg)
{
	// Approval messages need a full rebuild (approval buttons only on last message)
	if (Msg.Sender == FBobBotChatMessage::ESender::Approval || (Controller && Controller->HasPendingApproval()))
	{
		RebuildChatMessages();
		return;
	}

	// Append-only: add just the new widget instead of rebuilding all
	if (ChatMessagesBox.IsValid())
	{
		ChatMessagesBox->AddSlot().AutoHeight().Padding(BobBot::Theme::PadMessage())
		[
			BuildChatMessageWidget(Msg)
		];

		if (ChatScrollBox.IsValid())
			ChatScrollBox->ScrollToEnd();
	}
}

void SBobBotChatTab::OnMessageUpdated(int32 MessageIndex)
{
	// A message was modified in-place (tool call completed, cost added, etc.)
	// Full rebuild is the simplest correct approach for now
	RebuildChatMessages();
}

void SBobBotChatTab::OnHistoryCleared()
{
	RebuildChatMessages();
}

void SBobBotChatTab::OnApprovalChanged()
{
	RebuildChatMessages();
}

void SBobBotChatTab::OnThinkingChanged()
{
	UpdateThinkingIndicator();
}

// =========================================================================== //
// Inline image preview
// =========================================================================== //

FString SBobBotChatTab::ExtractImagePath(const FString& Text)
{
	// Look for file paths ending with image extensions
	static const TArray<FString> ImageExtensions = { TEXT(".png"), TEXT(".jpg"), TEXT(".jpeg"), TEXT(".bmp") };

	for (const FString& Ext : ImageExtensions)
	{
		int32 ExtIdx = Text.Find(Ext, ESearchCase::IgnoreCase);
		while (ExtIdx != INDEX_NONE)
		{
			// Walk backwards from the extension to find the start of the path
			int32 PathStart = ExtIdx;
			while (PathStart > 0)
			{
				TCHAR C = Text[PathStart - 1];
				if (C == TEXT('\n') || C == TEXT('\r') || C == TEXT(' ') || C == TEXT('"') || C == TEXT('\''))
					break;
				--PathStart;
			}

			FString Candidate = Text.Mid(PathStart, ExtIdx - PathStart + Ext.Len());
			Candidate.TrimStartAndEndInline();

			// Must look like an absolute path (drive letter or UNC)
			if (Candidate.Len() > 4 &&
				(Candidate[1] == TEXT(':') || Candidate.StartsWith(TEXT("/")) || Candidate.StartsWith(TEXT("\\"))))
			{
				// Normalize slashes
				Candidate.ReplaceInline(TEXT("/"), TEXT("\\"));
				IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
				if (PF.FileExists(*Candidate))
				{
					return Candidate;
				}
			}

			// Continue searching for more occurrences
			ExtIdx = Text.Find(Ext, ESearchCase::IgnoreCase, ESearchDir::FromStart, ExtIdx + 1);
		}
	}
	return FString();
}

TSharedRef<SWidget> SBobBotChatTab::BuildInlineImageWidget(const FString& ImagePath)
{
	// Create a dynamic brush from the image file
	// Note: FSlateDynamicImageBrush is self-cleaning; Slate owns the lifecycle
	const FVector2D ImageSize(480.f, 270.f);  // Preview size
	TSharedPtr<FSlateDynamicImageBrush> ImageBrush = MakeShareable(
		new FSlateDynamicImageBrush(FName(*ImagePath), ImageSize));

	if (!ImageBrush.IsValid() || !ImageBrush->HasUObject())
	{
		return SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("[Image: %s]"), *FPaths::GetCleanFilename(ImagePath))))
			.Font(BobBot::Theme::FontSmall())
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray));
	}

	return SNew(SBox)
		.MaxDesiredWidth(480.f)
		.MaxDesiredHeight(270.f)
		[
			SNew(SImage)
			.Image(ImageBrush.Get())
		];
}

// =========================================================================== //
// Quickstart chips
// =========================================================================== //

bool SBobBotChatTab::ShouldShowQuickstartChips() const
{
	if (!Controller) return false;
	const auto& History = Controller->GetHistory();
	// Show chips only when the chat is fresh (just the welcome system message, no user messages)
	if (History.Num() > 2) return false;
	for (const auto& Msg : History)
	{
		if (Msg.Sender == FBobBotChatMessage::ESender::User
			|| Msg.Sender == FBobBotChatMessage::ESender::Bot)
			return false;
	}
	return true;
}

TSharedRef<SWidget> SBobBotChatTab::BuildQuickstartChip(const FText& Label, const FString& Message)
{
	FString MessageCopy = Message;
	return SNew(SButton)
		.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
		.ContentPadding(FMargin(12, 6))
		.OnClicked_Lambda([this, MessageCopy]() -> FReply
		{
			if (Controller)
				Controller->SendMessage(MessageCopy);
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(BobBot::Theme::FontSmall())
			.ColorAndOpacity(FSlateColor(BobBot::Colors::LightGray))
		];
}

#undef LOCTEXT_NAMESPACE
