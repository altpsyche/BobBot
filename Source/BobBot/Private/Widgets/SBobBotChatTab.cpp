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
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "BobBotStyle.h"
#include "HAL/PlatformApplicationMisc.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "AssetRegistry/AssetData.h"

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
				PathsText += Asset.GetObjectPathString();
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

		// Chat header bar: [Chat Title v] [+]  Model | $cost | msgs  [Clear]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
			[
				SNew(SComboButton)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return Controller ? FText::FromString(Controller->GetActiveChatTitle()) : LOCTEXT("NoChat", "Chat");
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				]
				.MenuContent()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).MaxDesiredHeight(300.f)
						[
							SAssignNew(ChatListBox, SVerticalBox)
						]
					]
				]
				.OnComboBoxOpened_Lambda([this]() { RebuildChatList(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("NewChatBtn", "+"))
				.ToolTipText(LOCTEXT("NewChatTip", "New conversation"))
				.OnClicked_Lambda([this]() { if (Controller) Controller->NewChat(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SBobBotChatTab::GetChatHeaderText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton).Text(LOCTEXT("ClearBtn", "Clear"))
				.OnClicked(this, &SBobBotChatTab::OnClearChatClicked)
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
					.HintText(LOCTEXT("ChatHint", "Ask BobBot anything... (Enter to send, Shift+Enter for newline, drag assets here)"))
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
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0).VAlign(VAlign_Bottom)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(SendButton, SButton)
					.Text(LOCTEXT("SendBtn", "Send"))
					.OnClicked(this, &SBobBotChatTab::OnSendClicked)
					.IsEnabled(this, &SBobBotChatTab::IsSendEnabled)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
				[
					SAssignNew(StopButton, SButton)
					.Text(LOCTEXT("StopBtn", "Stop"))
					.OnClicked(this, &SBobBotChatTab::OnStopClicked)
					.Visibility_Lambda([this]() { return (Controller && Controller->IsThinking()) ? EVisibility::Visible : EVisibility::Collapsed; })
					.ButtonColorAndOpacity(FLinearColor(0.8f, 0.2f, 0.2f))
				]
			]
		]
	];

	// Initial rebuild from loaded history
	RebuildChatMessages();
}

// =========================================================================== //
// Chat header
// =========================================================================== //

FText SBobBotChatTab::GetChatHeaderText() const
{
	if (!Controller)
		return LOCTEXT("ChatHeaderNA", "No controller");

	return FText::Format(LOCTEXT("ChatHeader", "Model: {0}  |  Session: {1}  |  {2} messages"),
		FText::FromString(FBobBotConfig::Get().ChatModel),
		FText::FromString(FString::Printf(TEXT("$%.2f"), Controller->GetSessionCost())),
		FText::AsNumber(Controller->GetMessageCount()));
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

void SBobBotChatTab::CopyToClipboard(FString Text)
{
	FPlatformApplicationMisc::ClipboardCopy(*Text);
}

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
// Message content widget builder
// =========================================================================== //

TSharedRef<SWidget> SBobBotChatTab::BuildMessageContentWidget(const FString& Content, FBobBotChatMessage::ESender Sender)
{
	// For non-bot messages, render as plain text with appropriate color
	if (Sender != FBobBotChatMessage::ESender::Bot)
	{
		FLinearColor TextColor =
			Sender == FBobBotChatMessage::ESender::Error ? BobBot::Colors::ErrorOrange :
			Sender == FBobBotChatMessage::ESender::Approval ? BobBot::Colors::Yellow :
			FLinearColor::White;

		return SNew(STextBlock).Text(FText::FromString(Content))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(TextColor));
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
				{
					ContentBox->AddSlot().AutoHeight().Padding(0, 2)
					[
						SNew(SBorder)
						.BorderBackgroundColor(BobBot::Colors::CodeBlockBg)
						.Padding(FMargin(8, 4))
						[
							SNew(STextBlock).Text(FText::FromString(Segment))
							.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
							.AutoWrapText(true)
							.ColorAndOpacity(FSlateColor(BobBot::Colors::CodeText))
						]
					];
				}
				else
				{
					// Prose: convert inline markdown and render with SRichTextBlock
					FString Markup = MarkdownToRichText(Segment);
					ContentBox->AddSlot().AutoHeight()
					[
						SNew(SRichTextBlock)
						.Text(FText::FromString(Markup))
						.DecoratorStyleSet(&FBobBotStyle::Get())
						.TextStyle(&FBobBotStyle::Get(), "BobBot.Normal")
						.AutoWrapText(true)
					];
				}
			}
			break;
		}

		FString Before = Remaining.Left(TickIdx).TrimEnd();
		if (!Before.IsEmpty())
		{
			if (bInCodeBlock)
			{
				ContentBox->AddSlot().AutoHeight().Padding(0, 2)
				[
					SNew(SBorder)
					.BorderBackgroundColor(BobBot::Colors::CodeBlockBg)
					.Padding(FMargin(8, 4))
					[
						SNew(STextBlock).Text(FText::FromString(Before))
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
						.AutoWrapText(true)
						.ColorAndOpacity(FSlateColor(BobBot::Colors::CodeText))
					]
				];
			}
			else
			{
				FString Markup = MarkdownToRichText(Before);
				ContentBox->AddSlot().AutoHeight()
				[
					SNew(SRichTextBlock)
					.Text(FText::FromString(Markup))
					.DecoratorStyleSet(&FBobBotStyle::Get())
					.TextStyle(&FBobBotStyle::Get(), "BobBot.Normal")
					.AutoWrapText(true)
				];
			}
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
	FString SenderLabel;
	FLinearColor SenderColor;

	switch (Msg.Sender)
	{
	case FBobBotChatMessage::ESender::User:
		SenderLabel = TEXT("[You]");
		SenderColor = BobBot::Colors::Blue;
		break;
	case FBobBotChatMessage::ESender::Bot:
		SenderLabel = TEXT("[BobBot]");
		SenderColor = BobBot::Colors::BotGreen;
		break;
	case FBobBotChatMessage::ESender::System:
		SenderLabel = TEXT("[System]");
		SenderColor = BobBot::Colors::DimGray;
		break;
	case FBobBotChatMessage::ESender::Error:
		SenderLabel = TEXT("[Error]");
		SenderColor = BobBot::Colors::ErrorOrange;
		break;
	case FBobBotChatMessage::ESender::Approval:
		SenderLabel = TEXT("[Tool Request]");
		SenderColor = BobBot::Colors::Yellow;
		break;
	case FBobBotChatMessage::ESender::ToolCall:
		// Tool calls use a dedicated widget builder
		return BuildToolCallWidget(Msg);
	}

	FString TimeStr = Msg.Timestamp.ToString(TEXT("%H:%M:%S"));
	FString MsgContent = Msg.Content;

	// Build tooltip: timestamp, and cost/duration if available
	FString Tooltip = TimeStr;
	if (Msg.Sender == FBobBotChatMessage::ESender::Bot && Msg.Cost > 0.f)
	{
		Tooltip += FString::Printf(TEXT("  |  $%.4f"), Msg.Cost);
		if (Msg.DurationMs > 0)
			Tooltip += FString::Printf(TEXT("  |  %.1fs"), Msg.DurationMs / 1000.f);
		if (Msg.NumTurns > 1)
			Tooltip += FString::Printf(TEXT("  |  %d turns"), Msg.NumTurns);
	}

	TSharedRef<SVerticalBox> MessageBox = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(STextBlock).Text(FText::FromString(SenderLabel))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(FSlateColor(SenderColor))
				.ToolTipText(FText::FromString(Tooltip))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNullWidget::NullWidget
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(2, 0))
				.OnClicked_Lambda([MsgContent]() { CopyToClipboard(MsgContent); return FReply::Handled(); })
				.ToolTipText(LOCTEXT("CopyTip", "Copy message"))
				[
					SNew(STextBlock).Text(LOCTEXT("CopyIcon", "\x2398"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f)))
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4, 2, 0, 4)
		[
			BuildMessageContentWidget(Msg.Content, Msg.Sender)
		];

	return MessageBox;
}

TSharedRef<SWidget> SBobBotChatTab::BuildToolCallWidget(const FBobBotChatMessage& Msg)
{
	FLinearColor StatusColor = Msg.bToolComplete ? BobBot::Colors::Green : BobBot::Colors::Yellow;
	FString StatusText = Msg.bToolComplete ? TEXT("Complete") : TEXT("Running...");
	FString HeaderText = FString::Printf(TEXT("Tool: %s  %s"), *Msg.ToolName, *StatusText);

	TSharedRef<SWidget> CodeContent = SNullWidget::NullWidget;
	if (!Msg.ToolInput.IsEmpty())
	{
		CodeContent = SNew(SBorder)
			.BorderBackgroundColor(BobBot::Colors::CodeBlockBg)
			.Padding(FMargin(8, 4))
			[
				SNew(STextBlock).Text(FText::FromString(Msg.ToolInput))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(BobBot::Colors::CodeText))
			];
	}

	return SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.8f))
		.Padding(FMargin(4, 2))
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(Msg.bToolComplete)
			.HeaderPadding(FMargin(4, 2))
			.HeaderContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(HeaderText))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FSlateColor(StatusColor))
			]
			.BodyContent()
			[
				CodeContent
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

	for (int32 MsgIdx = 0; MsgIdx < History.Num(); ++MsgIdx)
	{
		const FBobBotChatMessage& Msg = History[MsgIdx];
		TSharedRef<SWidget> MessageWidget = BuildChatMessageWidget(Msg);

		// Approval buttons — only on the most recent Approval message while pending
		if (Msg.Sender == FBobBotChatMessage::ESender::Approval && Controller->HasPendingApproval()
			&& MsgIdx == History.Num() - 1)
		{
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
						.ButtonColorAndOpacity(FLinearColor(0.2f, 0.7f, 0.2f))
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("DenyBtn", "Deny"))
						.OnClicked_Lambda([this]() { if (Controller) Controller->DenyExecution(); return FReply::Handled(); })
						.ButtonColorAndOpacity(FLinearColor(0.7f, 0.2f, 0.2f))
					]
				];
			MessageWidget = ApprovalBox;
		}

		ChatMessagesBox->AddSlot().AutoHeight().Padding(8, 4)
		[
			MessageWidget
		];
	}

	// Show thinking indicator as a real message in the chat
	if (Controller->IsThinking())
	{
		FString Dots;
		for (int32 i = 0; i < (Controller->GetThinkingDotCount() % 3) + 1; i++)
			Dots += TEXT(".");

		ChatMessagesBox->AddSlot().AutoHeight().Padding(8, 4)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(LOCTEXT("BotThinking", "[BobBot]"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(FSlateColor(BobBot::Colors::BotGreen))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4, 2, 0, 4)
			[
				SAssignNew(ThinkingTextBlock, STextBlock)
				.Text(FText::Format(LOCTEXT("ThinkingDots", "Thinking{0}"), FText::FromString(Dots)))
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 10))
				.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
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

	const TArray<FBobBotChatEntry>& Index = Controller->GetChatIndex();
	FString ActiveId = Controller->GetActiveChatId();

	for (const FBobBotChatEntry& Entry : Index)
	{
		FString ChatId = Entry.Id;
		bool bIsActive = (ChatId == ActiveId);
		FLinearColor TextColor = bIsActive ? BobBot::Colors::ActiveBlue : FLinearColor::White;

		ChatListBox->AddSlot().AutoHeight().Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(4, 2))
				.OnClicked_Lambda([this, ChatId]()
				{
					if (Controller) Controller->SwitchChat(ChatId);
					return FReply::Handled();
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(Entry.Title))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.ColorAndOpacity(FSlateColor(TextColor))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("%s  $%.2f"), *Entry.Model, Entry.Cost)))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
						.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(4, 0))
				.ToolTipText(LOCTEXT("DeleteChatTip", "Delete conversation"))
				.Visibility(bIsActive ? EVisibility::Collapsed : EVisibility::Visible)
				.OnClicked_Lambda([this, ChatId]()
				{
					if (Controller) Controller->DeleteChat(ChatId);
					RebuildChatList();
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DeleteIcon", "x"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FSlateColor(BobBot::Colors::Red))
				]
			]
		];
	}

	if (Index.Num() == 0)
	{
		ChatListBox->AddSlot().AutoHeight().Padding(8, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoChats", "No conversations"))
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
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
		ChatMessagesBox->AddSlot().AutoHeight().Padding(8, 4)
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

#undef LOCTEXT_NAMESPACE
