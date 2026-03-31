// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "Widgets/SBobBotWelcomeTab.h"
#include "BobBotConstants.h"
#include "BobBotRuntimeStatus.h"
#include "BobBotPythonBridge.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Dom/JsonObject.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "SBobBotWelcomeTab"

// Step labels
static const FText GStepLabels[] = {
	LOCTEXT("Step1", "Checking prerequisites"),
	LOCTEXT("Step2", "Creating Python environment"),
	LOCTEXT("Step3", "Installing MCP bridge"),
	LOCTEXT("Step4", "Installing Agent SDK"),
	LOCTEXT("Step5", "Starting HTTP bridge"),
	LOCTEXT("Step6", "Verifying SDK"),
};

// =========================================================================== //
// Construction
// =========================================================================== //

void SBobBotWelcomeTab::Construct(const FArguments& InArgs)
{
	OnSetupComplete = InArgs._OnSetupComplete;
	OnSkipped = InArgs._OnSkipped;

	for (int32 i = 0; i < NumSteps; ++i)
	{
		StepStatuses[i] = EStepStatus::Pending;
	}

	ChildSlot
	[
		SNew(SScrollBox)

		// Title
		+ SScrollBox::Slot().Padding(24, 24, 24, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("WelcomeTitle", "Welcome to BobBot"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
			.ColorAndOpacity(FSlateColor(BobBot::Colors::BotGreen))
		]

		+ SScrollBox::Slot().Padding(24, 0, 24, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("WelcomeSub", "MCP AI Tool for Unreal Engine"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
		]

		+ SScrollBox::Slot().Padding(24, 8, 24, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("WelcomeDesc", "Setting up your environment. This only happens once."))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			.AutoWrapText(true)
		]

		+ SScrollBox::Slot().Padding(16, 12) [ SNew(SSeparator) ]

		// Step list
		+ SScrollBox::Slot().Padding(24, 4)
		[
			SAssignNew(StepListBox, SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeStepRow(0, GStepLabels[0]) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeStepRow(1, GStepLabels[1]) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeStepRow(2, GStepLabels[2]) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeStepRow(3, GStepLabels[3]) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeStepRow(4, GStepLabels[4]) ]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4) [ MakeStepRow(5, GStepLabels[5]) ]
		]

		+ SScrollBox::Slot().Padding(16, 12) [ SNew(SSeparator) ]

		// Progress bar
		+ SScrollBox::Slot().Padding(24, 4)
		[
			SNew(SProgressBar)
			.Percent_Lambda([this]() {
				int32 Done = 0;
				for (int32 i = 0; i < NumSteps; ++i)
					if (StepStatuses[i] == EStepStatus::Success || StepStatuses[i] == EStepStatus::Warning)
						++Done;
				return static_cast<float>(Done) / NumSteps;
			})
			.FillColorAndOpacity(BobBot::Colors::BotGreen)
		]

		// Overall status
		+ SScrollBox::Slot().Padding(24, 8)
		[
			SNew(STextBlock)
			.Text(this, &SBobBotWelcomeTab::GetOverallText)
			.ColorAndOpacity(this, &SBobBotWelcomeTab::GetOverallColor)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		// Buttons
		+ SScrollBox::Slot().Padding(24, 8, 24, 24)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("Retry", "Retry"))
				.Visibility(this, &SBobBotWelcomeTab::GetRetryVisibility)
				.OnClicked(this, &SBobBotWelcomeTab::HandleRetry)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f) [ SNullWidget::NullWidget ]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Skip", "Skip Setup"))
				.ToolTipText(LOCTEXT("SkipTip", "Skip automatic setup and configure manually in the Connect tab"))
				.ButtonStyle(FCoreStyle::Get(), "NoBorder")
				.OnClicked(this, &SBobBotWelcomeTab::HandleSkip)
				.Visibility_Lambda([this]() { return bFinished ? EVisibility::Collapsed : EVisibility::Visible; })
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SkipText", "Skip Setup"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
				]
			]
		]
	];
}

// =========================================================================== //
// Step row builder
// =========================================================================== //

TSharedRef<SWidget> SBobBotWelcomeTab::MakeStepRow(int32 Index, const FText& Label)
{
	return SNew(SHorizontalBox)

		// Status icon
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
		[
			SNew(SBox).WidthOverride(18.f)
			[
				SNew(STextBlock)
				.Text(this, &SBobBotWelcomeTab::GetStepStatusIcon, Index)
				.ColorAndOpacity(this, &SBobBotWelcomeTab::GetStepStatusColor, Index)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]
		]

		// Label
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
		[
			SNew(STextBlock)
			.Text(this, &SBobBotWelcomeTab::GetStepLabel, Index)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			.ColorAndOpacity(this, &SBobBotWelcomeTab::GetStepStatusColor, Index)
		]

		// Detail message
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &SBobBotWelcomeTab::GetStepDetail, Index)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
			.AutoWrapText(true)
		];
}

// =========================================================================== //
// Tick — drives the state machine
// =========================================================================== //

void SBobBotWelcomeTab::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// After setup completes, wait for delay then fire the delegate
	if (bFinished)
	{
		if (StepDelay > 0.f)
		{
			StepDelay -= InDeltaTime;
			if (StepDelay <= 0.f)
				OnSetupComplete.ExecuteIfBound();
		}
		return;
	}

	// Start auto-run on first tick
	if (!bAutoRunning)
	{
		bAutoRunning = true;
		BeginStep(ESetupStep::CheckPrerequisites);
		return;
	}

	// Wait for delay between steps
	if (StepDelay > 0.f)
	{
		StepDelay -= InDeltaTime;
		if (StepDelay <= 0.f)
		{
			// Advance to next step after delay
			int32 NextIdx = static_cast<int32>(CurrentStep) + 1;
			if (NextIdx < static_cast<int32>(ESetupStep::Complete))
			{
				BeginStep(static_cast<ESetupStep>(NextIdx));
			}
		}
		return;
	}

	// Poll async result
	if (bAsyncInFlight)
	{
		FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
		if (!Bridge.IsAvailable()) return;

		FString Script =
			TEXT("import json, os\n")
			TEXT("p = os.path.join(os.environ.get('BOB_PROJECT_ROOT','.'), 'Saved', 'BobBot', '_welcome_step.json')\n")
			TEXT("d = '{}'\n")
			TEXT("if os.path.isfile(p):\n")
			TEXT("    with open(p) as f: d = f.read()\n")
			TEXT("    os.remove(p)\n")
			TEXT("open(output_path, 'w').write(d)\n");

		TSharedPtr<FJsonObject> Obj = Bridge.ExecPythonWithJsonResult(Script, TEXT("_welcome_poll.json"));
		if (!Obj.IsValid() || !Obj->HasField(TEXT("ok")))
			return;  // Result not ready yet

		bAsyncInFlight = false;
		bool bOk = false;
		Obj->TryGetBoolField(TEXT("ok"), bOk);
		FString Msg;
		Obj->TryGetStringField(TEXT("message"), Msg);
		bool bWarn = false;
		Obj->TryGetBoolField(TEXT("warn"), bWarn);
		int32 StepIdx = static_cast<int32>(CurrentStep);

		if (bOk)
			FinishStep(StepIdx, bWarn ? EStepStatus::Warning : EStepStatus::Success, Msg);
		else
			FinishStep(StepIdx, EStepStatus::Failed, Msg);
	}
}

// =========================================================================== //
// Step execution
// =========================================================================== //

void SBobBotWelcomeTab::BeginStep(ESetupStep Step)
{
	int32 Idx = static_cast<int32>(Step);
	CurrentStep = Step;
	StepStatuses[Idx] = EStepStatus::InProgress;
	StepMessages[Idx] = TEXT("");

	if (Step == ESetupStep::CheckPrerequisites)
	{
		// Synchronous — read from already-populated FBobBotRuntimeStatus
		const FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();

		if (!Status.bPythonPluginAvailable)
		{
			FinishStep(Idx, EStepStatus::Failed,
				TEXT("PythonScriptPlugin not enabled. Enable in Edit > Plugins, then restart."));
			return;
		}
		if (!Status.bClaudeCodeAvailable)
		{
			FinishStep(Idx, EStepStatus::Failed,
				TEXT("Claude Code not found. Install with: npm install -g @anthropic-ai/claude-code"));
			return;
		}
		if (!Status.bClaudeAuthenticated)
		{
			FinishStep(Idx, EStepStatus::Failed,
				TEXT("Claude Code not authenticated. Run 'claude login' in a terminal."));
			return;
		}

		FString Msg = FString::Printf(TEXT("PythonScriptPlugin, Claude Code %s"), *Status.ClaudeCodeVersion);
		FinishStep(Idx, EStepStatus::Success, Msg);
	}
	else
	{
		RunStepAsync(Step);
	}
}

void SBobBotWelcomeTab::RunStepAsync(ESetupStep Step)
{
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (!Bridge.IsAvailable())
	{
		FinishStep(static_cast<int32>(Step), EStepStatus::Failed, TEXT("Python bridge not available"));
		return;
	}

	FString PythonCode;
	switch (Step)
	{
	case ESetupStep::CreateVenv:
		PythonCode =
			TEXT("import bob_bridge_launcher, json, os, threading\n")
			TEXT("def _run():\n")
			TEXT("    r = bob_bridge_launcher.setup_create_venv()\n")
			TEXT("    out = os.path.join(os.environ.get('BOB_PROJECT_ROOT','.'), 'Saved', 'BobBot', '_welcome_step.json')\n")
			TEXT("    with open(out, 'w') as f: json.dump(r, f)\n")
			TEXT("threading.Thread(target=_run, daemon=True).start()\n");
		break;

	case ESetupStep::InstallBridge:
		PythonCode =
			TEXT("import bob_bridge_launcher, json, os, threading\n")
			TEXT("def _run():\n")
			TEXT("    r = bob_bridge_launcher.setup_install_mcp()\n")
			TEXT("    out = os.path.join(os.environ.get('BOB_PROJECT_ROOT','.'), 'Saved', 'BobBot', '_welcome_step.json')\n")
			TEXT("    with open(out, 'w') as f: json.dump(r, f)\n")
			TEXT("threading.Thread(target=_run, daemon=True).start()\n");
		break;

	case ESetupStep::InstallSDK:
		PythonCode =
			TEXT("import bob_bridge_launcher, json, os, threading\n")
			TEXT("def _run():\n")
			TEXT("    r = bob_bridge_launcher.setup_install_sdk()\n")
			TEXT("    r['warn'] = not r.get('ok', False)\n")  // SDK is non-critical
			TEXT("    if not r['ok']: r['ok'] = True\n")       // Don't block on SDK failure
			TEXT("    out = os.path.join(os.environ.get('BOB_PROJECT_ROOT','.'), 'Saved', 'BobBot', '_welcome_step.json')\n")
			TEXT("    with open(out, 'w') as f: json.dump(r, f)\n")
			TEXT("threading.Thread(target=_run, daemon=True).start()\n");
		break;

	case ESetupStep::StartBridge:
		PythonCode =
			TEXT("import bob_bridge_launcher, json, os, threading\n")
			TEXT("def _run():\n")
			TEXT("    ok = bob_bridge_launcher.start()\n")
			TEXT("    r = {'ok': ok, 'message': 'Bridge started' if ok else 'Bridge failed to start'}\n")
			TEXT("    out = os.path.join(os.environ.get('BOB_PROJECT_ROOT','.'), 'Saved', 'BobBot', '_welcome_step.json')\n")
			TEXT("    with open(out, 'w') as f: json.dump(r, f)\n")
			TEXT("threading.Thread(target=_run, daemon=True).start()\n");
		break;

	case ESetupStep::VerifySDK:
		PythonCode =
			TEXT("import json, os, sys, threading\n")
			TEXT("def _run():\n")
			TEXT("    try:\n")
			TEXT("        # Add venv site-packages so UE Python can import the SDK\n")
			TEXT("        import bob_bridge_launcher\n")
			TEXT("        sp = bob_bridge_launcher.get_venv_site_packages()\n")
			TEXT("        if sp and os.path.isdir(sp) and sp not in sys.path:\n")
			TEXT("            sys.path.insert(0, sp)\n")
			TEXT("            if sys.platform == 'win32':\n")
			TEXT("                dll = os.path.join(sp, 'pywin32_system32')\n")
			TEXT("                if os.path.isdir(dll):\n")
			TEXT("                    os.add_dll_directory(dll)\n")
			TEXT("                    if dll not in sys.path: sys.path.insert(0, dll)\n")
			TEXT("        import bob_chat_sdk\n")
			TEXT("        r = bob_chat_sdk.check_sdk_available()\n")
			TEXT("        r['message'] = 'Agent SDK ready' if r.get('ok') else 'SDK not available (subprocess mode will be used)'\n")
			TEXT("        if not r['ok']: r['warn'] = True; r['ok'] = True\n")
			TEXT("    except Exception as e:\n")
			TEXT("        r = {'ok': True, 'warn': True, 'message': 'SDK check: ' + str(e)}\n")
			TEXT("    out = os.path.join(os.environ.get('BOB_PROJECT_ROOT','.'), 'Saved', 'BobBot', '_welcome_step.json')\n")
			TEXT("    with open(out, 'w') as f: json.dump(r, f)\n")
			TEXT("threading.Thread(target=_run, daemon=True).start()\n");
		break;

	default:
		return;
	}

	bAsyncInFlight = true;
	Bridge.ExecPythonCommand(*PythonCode);
}

void SBobBotWelcomeTab::FinishStep(int32 Index, EStepStatus Status, const FString& Message)
{
	StepStatuses[Index] = Status;
	StepMessages[Index] = Message;

	if (Status == EStepStatus::Failed)
	{
		// Stop advancing
		return;
	}

	// Brief delay for readability, then advance
	StepDelay = 0.3f;

	ESetupStep NextStep = static_cast<ESetupStep>(Index + 1);
	if (NextStep == ESetupStep::Complete)
	{
		bFinished = true;
		// Slight delay before auto-transitioning so user sees "Complete"
		StepDelay = 1.5f;

		// Use a ticker to fire the delegate after delay
		auto TimerDelegate = FTimerDelegate::CreateLambda([this]() {
			OnSetupComplete.ExecuteIfBound();
		});
		// Fire via next tick cycle since we can't use FTimerManager from Slate easily
		// The Tick will handle this via bFinished + StepDelay
	}
}

void SBobBotWelcomeTab::AdvanceToNextStep()
{
	// Called from Tick when StepDelay expires and no async in flight
	// Nothing to do here — Tick handles the flow via CurrentStep progression
}

// =========================================================================== //
// UI getters
// =========================================================================== //

FText SBobBotWelcomeTab::GetStepLabel(int32 Index) const
{
	if (Index >= 0 && Index < NumSteps)
		return GStepLabels[Index];
	return FText::GetEmpty();
}

FText SBobBotWelcomeTab::GetStepStatusIcon(int32 Index) const
{
	if (Index < 0 || Index >= NumSteps) return FText::GetEmpty();
	switch (StepStatuses[Index])
	{
	case EStepStatus::Pending:    return LOCTEXT("IconPending", "\x2014");   // —
	case EStepStatus::InProgress: return LOCTEXT("IconProgress", "\x25CB");  // ○
	case EStepStatus::Success:    return LOCTEXT("IconSuccess", "\x2713");   // ✓
	case EStepStatus::Warning:    return LOCTEXT("IconWarning", "!");
	case EStepStatus::Failed:     return LOCTEXT("IconFailed", "\x2717");    // ✗
	default: return FText::GetEmpty();
	}
}

FSlateColor SBobBotWelcomeTab::GetStepStatusColor(int32 Index) const
{
	if (Index < 0 || Index >= NumSteps) return FSlateColor(BobBot::Colors::DimGray);
	switch (StepStatuses[Index])
	{
	case EStepStatus::Pending:    return FSlateColor(BobBot::Colors::DimGray);
	case EStepStatus::InProgress: return FSlateColor(BobBot::Colors::Yellow);
	case EStepStatus::Success:    return FSlateColor(BobBot::Colors::Green);
	case EStepStatus::Warning:    return FSlateColor(BobBot::Colors::Yellow);
	case EStepStatus::Failed:     return FSlateColor(BobBot::Colors::Red);
	default: return FSlateColor(BobBot::Colors::DimGray);
	}
}

FText SBobBotWelcomeTab::GetStepDetail(int32 Index) const
{
	if (Index < 0 || Index >= NumSteps) return FText::GetEmpty();
	if (StepStatuses[Index] == EStepStatus::InProgress)
		return LOCTEXT("StepRunning", "...");
	return FText::FromString(StepMessages[Index]);
}

FText SBobBotWelcomeTab::GetOverallText() const
{
	if (bFinished)
		return LOCTEXT("SetupDone", "Setup complete! BobBot is ready.");

	for (int32 i = 0; i < NumSteps; ++i)
	{
		if (StepStatuses[i] == EStepStatus::Failed)
			return LOCTEXT("SetupFailed", "Setup failed \x2014 fix the issue above and click Retry.");
		if (StepStatuses[i] == EStepStatus::InProgress)
			return LOCTEXT("SetupRunning", "Setting up... this takes about 30 seconds on first launch.");
	}
	return LOCTEXT("SetupStarting", "Starting setup...");
}

FSlateColor SBobBotWelcomeTab::GetOverallColor() const
{
	if (bFinished)
		return FSlateColor(BobBot::Colors::BotGreen);

	for (int32 i = 0; i < NumSteps; ++i)
	{
		if (StepStatuses[i] == EStepStatus::Failed)
			return FSlateColor(BobBot::Colors::Red);
	}
	return FSlateColor(BobBot::Colors::Yellow);
}

EVisibility SBobBotWelcomeTab::GetRetryVisibility() const
{
	for (int32 i = 0; i < NumSteps; ++i)
	{
		if (StepStatuses[i] == EStepStatus::Failed)
			return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

FReply SBobBotWelcomeTab::HandleSkip()
{
	OnSkipped.ExecuteIfBound();
	return FReply::Handled();
}

FReply SBobBotWelcomeTab::HandleRetry()
{
	// Reset from the failed step onward
	for (int32 i = 0; i < NumSteps; ++i)
	{
		if (StepStatuses[i] == EStepStatus::Failed)
		{
			for (int32 j = i; j < NumSteps; ++j)
			{
				StepStatuses[j] = EStepStatus::Pending;
				StepMessages[j] = TEXT("");
			}
			CurrentStep = static_cast<ESetupStep>(i);
			bAsyncInFlight = false;
			bFinished = false;
			BeginStep(CurrentStep);
			break;
		}
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
