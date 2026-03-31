// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/**
 * Welcome/FTUE tab — shown on first launch to set up the environment.
 * Auto-advances through prerequisites check, venv creation, dependency install,
 * bridge start, and SDK verification. Fires OnSetupComplete when done.
 */
class SBobBotWelcomeTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBobBotWelcomeTab) {}
		SLATE_EVENT(FSimpleDelegate, OnSetupComplete)
		SLATE_EVENT(FSimpleDelegate, OnSkipped)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Reset state machine to run setup from scratch (used by Factory Reset). */
	void Reset();

private:
	// Setup step IDs (ordered)
	enum class ESetupStep : uint8
	{
		CheckPrerequisites,   // Verify PythonScriptPlugin, Claude Code
		CreateVenv,           // Create .venv from UE's Python
		InstallBridge,        // pip install mcp[cli]
		InstallSDK,           // pip install claude-agent-sdk
		StartBridge,          // Launch HTTP bridge
		VerifySDK,            // Confirm SDK importable from UE Python
		Complete              // All done
	};
	static constexpr int32 NumSteps = 6;  // Excluding Complete

	// Per-step status
	enum class EStepStatus : uint8 { Pending, InProgress, Success, Warning, Failed };
	EStepStatus StepStatuses[NumSteps] = {};
	FString StepMessages[NumSteps];

	// State machine
	ESetupStep CurrentStep = ESetupStep::CheckPrerequisites;
	bool bAutoRunning = false;
	bool bAsyncInFlight = false;
	bool bFinished = false;
	float StepDelay = 0.f;  // Brief pause between steps for readability

	FSimpleDelegate OnSetupComplete;
	FSimpleDelegate OnSkipped;

	// Step execution
	void BeginStep(ESetupStep Step);
	void FinishStep(int32 Index, EStepStatus Status, const FString& Message);
	void RunStepAsync(ESetupStep Step);

	// Poll throttle and timeout
	float PollTimer = 0.f;
	float StepTimeout = 0.f;
	static constexpr float PollInterval = 0.5f;
	static constexpr float MaxStepTimeout = 120.f;

	// UI getters (called by Slate attributes)
	FText GetStepLabel(int32 Index) const;
	FText GetStepStatusIcon(int32 Index) const;
	FSlateColor GetStepStatusColor(int32 Index) const;
	FText GetStepDetail(int32 Index) const;
	FText GetOverallText() const;
	FSlateColor GetOverallColor() const;
	EVisibility GetRetryVisibility() const;

	FReply HandleSkip();
	FReply HandleRetry();

	// Step row widget builder
	TSharedRef<SWidget> MakeStepRow(int32 Index, const FText& Label);

	// Step row containers for refreshing
	TSharedPtr<class SVerticalBox> StepListBox;
};
