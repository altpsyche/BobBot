// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "BobBotChatController.h"

/**
 * Chat tab — displays message history, input area, send/stop buttons, thinking indicator.
 * Subscribes to FBobBotChatController delegates for reactive updates.
 */
class SBobBotChatTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBobBotChatTab) {}
		SLATE_ARGUMENT(FBobBotChatController*, Controller)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FBobBotChatController* Controller = nullptr;

	// -- Widget state --
	TSharedPtr<class SScrollBox> ChatScrollBox;
	TSharedPtr<class SMultiLineEditableTextBox> CommandInput;
	TSharedPtr<class SVerticalBox> ChatMessagesBox;
	TSharedPtr<class STextBlock> ThinkingTextBlock;
	TSharedPtr<class SButton> SendButton;
	TSharedPtr<class SButton> StopButton;
	TSharedPtr<class SVerticalBox> ChatListBox;
	bool bWasThinking = false;

	// -- Chat header --
	FText GetChatHeaderText() const;
	FText GetCostHeaderText() const;
	FSlateColor GetCostHeaderColor() const;
	FText GetContextHeaderText() const;
	FSlateColor GetContextHeaderColor() const;
	EVisibility GetContextVisibility() const;

	// -- Send button state --
	bool IsSendEnabled() const;

	// -- Actions --
	FReply OnSendClicked();
	FReply OnClearChatClicked();
	FReply OnStopClicked();

	// -- Message widget builders (static) --
	static TSharedRef<SWidget> BuildChatMessageWidget(const FBobBotChatMessage& Msg);
	static TSharedRef<SWidget> BuildMessageContentWidget(const FString& Content, FBobBotChatMessage::ESender Sender);
	static TSharedRef<SWidget> BuildToolCallWidget(const FBobBotChatMessage& Msg);
	static TSharedRef<SWidget> BuildSubagentWidget(const FBobBotChatMessage& Msg);

	// -- Rebuild / update --
	void RebuildChatMessages();
	void RebuildChatList();
	void UpdateThinkingIndicator();

	// -- Delegate handlers --
	void OnMessageAdded(const FBobBotChatMessage& Msg);
	void OnMessageUpdated(int32 MessageIndex);
	void OnHistoryCleared();
	void OnApprovalChanged();
	void OnThinkingChanged();
};
