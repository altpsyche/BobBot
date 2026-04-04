// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "BobBotChatController.h"
#include "BobBot.h"
#include "BobBotConfig.h"
#include "BobBotConstants.h"
#include "BobBotRuntimeStatus.h"
#include "BobBotPythonBridge.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "BobBotChatController"

// =========================================================================== //
// Construction
// =========================================================================== //

FBobBotChatController::FBobBotChatController()
{
	// Register slash commands
	SlashCommands.Add(TEXT("/clear"), [this](const FString&)
	{
		ClearSession();
	});

	SlashCommands.Add(TEXT("/cost"), [this](const FString&)
	{
		AddMessage(FBobBotChatMessage::ESender::System, FString::Printf(
			TEXT("Session cost: $%.4f  |  Messages: %d  |  Model: %s"),
			TotalSessionCost, SessionMessageCount, *FBobBotConfig::Get().ChatModel));
	});

	SlashCommands.Add(TEXT("/model"), [this](const FString& Args)
	{
		FString ModelArg = Args.TrimStartAndEnd().ToLower();
		if (ModelArg == TEXT("sonnet") || ModelArg == TEXT("opus") || ModelArg == TEXT("haiku"))
		{
			FBobBotConfig& Config = FBobBotConfig::Get();
			Config.ChatModel = ModelArg;
			Config.Save();
			FBobBotPythonBridge::Get().ExecCallWithString(TEXT("bob_chat"), TEXT("set_model"), ModelArg);
			AddMessage(FBobBotChatMessage::ESender::System, FString::Printf(TEXT("Model switched to %s."), *ModelArg));
		}
		else
		{
			AddMessage(FBobBotChatMessage::ESender::System, FString::Printf(
				TEXT("Current model: %s.  Usage: /model sonnet|opus|haiku"), *FBobBotConfig::Get().ChatModel));
		}
	});

	SlashCommands.Add(TEXT("/effort"), [this](const FString& Args)
	{
		FString Level = Args.TrimStartAndEnd().ToLower();
		if (Level == TEXT("low") || Level == TEXT("medium") || Level == TEXT("high") || Level == TEXT("max"))
		{
			FBobBotConfig& Cfg = FBobBotConfig::Get();
			Cfg.EffortLevel = Level;
			Cfg.Save();
			Cfg.ApplyEnvironmentVars();
			AddMessage(FBobBotChatMessage::ESender::System,
				FString::Printf(TEXT("Effort level set to %s. Takes effect on next message."), *Level));
		}
		else
		{
			AddMessage(FBobBotChatMessage::ESender::System,
				FString::Printf(TEXT("Current effort: %s.  Usage: /effort low|medium|high|max"),
					*FBobBotConfig::Get().EffortLevel));
		}
	});

	SlashCommands.Add(TEXT("/thinking"), [this](const FString& Args)
	{
		FString Mode = Args.TrimStartAndEnd().ToLower();
		if (Mode == TEXT("on") || Mode == TEXT("enabled"))
		{
			Mode = TEXT("enabled");
		}
		else if (Mode == TEXT("off") || Mode == TEXT("disabled"))
		{
			Mode = TEXT("disabled");
		}

		if (Mode == TEXT("enabled") || Mode == TEXT("disabled") || Mode == TEXT("adaptive"))
		{
			FBobBotConfig& Cfg = FBobBotConfig::Get();
			Cfg.ThinkingMode = Mode;
			Cfg.Save();
			Cfg.ApplyEnvironmentVars();
			AddMessage(FBobBotChatMessage::ESender::System,
				FString::Printf(TEXT("Thinking mode set to %s. Takes effect on next message."), *Mode));
		}
		else
		{
			AddMessage(FBobBotChatMessage::ESender::System,
				FString::Printf(TEXT("Current thinking: %s.  Usage: /thinking on|off|adaptive"),
					*FBobBotConfig::Get().ThinkingMode));
		}
	});

	SlashCommands.Add(TEXT("/rename"), [this](const FString& Args)
	{
		FString Title = Args.TrimStartAndEnd();
		if (Title.IsEmpty())
		{
			AddMessage(FBobBotChatMessage::ESender::System, TEXT("Usage: /rename <title>"));
			return;
		}
		if (ActiveChatId.IsEmpty())
		{
			AddMessage(FBobBotChatMessage::ESender::System, TEXT("No active session to rename."));
			return;
		}
		FBobBotPythonBridge& B = FBobBotPythonBridge::Get();
		B.CallPythonJson(TEXT("bob_chat"), TEXT("rename_saved_session"),
			B.PyStr(ActiveChatId) + TEXT(", ") + B.PyStr(Title));
		AddMessage(FBobBotChatMessage::ESender::System,
			FString::Printf(TEXT("Session renamed to \"%s\"."), *Title));
		OnChatListChanged.Broadcast();
	});

	SlashCommands.Add(TEXT("/tag"), [this](const FString& Args)
	{
		FString Tag = Args.TrimStartAndEnd();
		if (ActiveChatId.IsEmpty())
		{
			AddMessage(FBobBotChatMessage::ESender::System, TEXT("No active session to tag."));
			return;
		}
		FBobBotPythonBridge& B = FBobBotPythonBridge::Get();
		B.CallPythonJson(TEXT("bob_chat"), TEXT("tag_saved_session"),
			B.PyStr(ActiveChatId) + TEXT(", ") + B.PyStr(Tag));
		if (Tag.IsEmpty())
			AddMessage(FBobBotChatMessage::ESender::System, TEXT("Tag cleared."));
		else
			AddMessage(FBobBotChatMessage::ESender::System,
				FString::Printf(TEXT("Session tagged as \"%s\"."), *Tag));
		OnChatListChanged.Broadcast();
	});

	SlashCommands.Add(TEXT("/help"), [this](const FString&)
	{
		// Auto-generate from registered commands so help never gets out of sync
		TArray<FString> Keys;
		SlashCommands.GetKeys(Keys);
		Keys.Sort();
		AddMessage(FBobBotChatMessage::ESender::System,
			TEXT("Commands: ") + FString::Join(Keys, TEXT("  ")));
	});

	SlashCommands.Add(TEXT("/test"), [this](const FString& Args)
	{
		FString Categories = Args.TrimStartAndEnd();
		if (Categories.IsEmpty()) Categories = TEXT("all");

		AddMessage(FBobBotChatMessage::ESender::System,
			FString::Printf(TEXT("Running smoke tests (%s)..."), *Categories));

		FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
		if (!Bridge.IsAvailable())
		{
			AddMessage(FBobBotChatMessage::ESender::Error, TEXT("Python bridge not available."));
			return;
		}

		// Add Scripts/tools to sys.path so _test_runner can be imported
		FString PluginScriptsDir = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectPluginsDir() / TEXT("BobBot") / TEXT("Scripts") / TEXT("tools"));
		PluginScriptsDir = PluginScriptsDir.Replace(TEXT("\\"), TEXT("/"));

		FString Script = FString::Printf(
			TEXT("import sys, json\n")
			TEXT("scripts_dir = '%s'\n")
			TEXT("if scripts_dir not in sys.path: sys.path.insert(0, scripts_dir)\n")
			TEXT("import importlib\n")
			TEXT("try:\n")
			TEXT("    import _test_runner; importlib.reload(_test_runner)\n")
			TEXT("except: import _test_runner\n")
			TEXT("r = _test_runner.run_tests('%s')\n")
			TEXT("open(output_path, 'w').write(json.dumps(r))\n"),
			*PluginScriptsDir, *Categories.Replace(TEXT("'"), TEXT("\\'")));

		TSharedPtr<FJsonObject> Result = Bridge.ExecPythonWithJsonResult(Script, TEXT("_test_results.json"));
		if (!Result.IsValid())
		{
			AddMessage(FBobBotChatMessage::ESender::Error, TEXT("Test runner failed to produce results."));
			return;
		}

		int32 Passed = 0, Failed = 0, Skipped = 0, Total = 0;
		double ElapsedMs = 0;
		Result->TryGetNumberField(TEXT("passed"), (double&)Passed);
		Result->TryGetNumberField(TEXT("failed"), (double&)Failed);
		Result->TryGetNumberField(TEXT("skipped"), (double&)Skipped);
		Result->TryGetNumberField(TEXT("total"), (double&)Total);
		Result->TryGetNumberField(TEXT("elapsed_ms"), ElapsedMs);

		Passed = (int32)Result->GetNumberField(TEXT("passed"));
		Failed = (int32)Result->GetNumberField(TEXT("failed"));
		Skipped = (int32)Result->GetNumberField(TEXT("skipped"));
		Total = (int32)Result->GetNumberField(TEXT("total"));

		FString Summary = FString::Printf(
			TEXT("Tests: %d passed, %d failed, %d skipped (%d total, %dms)"),
			Passed, Failed, Skipped, Total, (int32)ElapsedMs);

		if (Failed > 0)
		{
			// Append failure details
			const TArray<TSharedPtr<FJsonValue>>* ResultsArr = nullptr;
			if (Result->TryGetArrayField(TEXT("results"), ResultsArr))
			{
				for (const auto& Val : *ResultsArr)
				{
					TSharedPtr<FJsonObject> Item = Val->AsObject();
					if (Item.IsValid() && Item->GetStringField(TEXT("status")) == TEXT("FAIL"))
					{
						Summary += FString::Printf(TEXT("\n  FAIL: %s - %s"),
							*Item->GetStringField(TEXT("name")),
							*Item->GetStringField(TEXT("detail")));
					}
				}
			}
			AddMessage(FBobBotChatMessage::ESender::Error, Summary);
		}
		else
		{
			AddMessage(FBobBotChatMessage::ESender::System, Summary);
		}
	});

	SlashCommands.Add(TEXT("/new"), [this](const FString&)
	{
		NewChat();
	});

	SlashCommands.Add(TEXT("/chats"), [this](const FString&)
	{
		// List sessions from SDK
		RefreshChatIndex();
		if (ChatIndex.Num() == 0)
		{
			AddMessage(FBobBotChatMessage::ESender::System, TEXT("No saved sessions."));
			return;
		}
		FString List;
		for (const FBobBotChatEntry& Entry : ChatIndex)
		{
			FString Marker = (Entry.Id == ActiveChatId) ? TEXT("*") : TEXT(" ");
			FString Display = Entry.Title.IsEmpty() ? TEXT("(untitled)") : Entry.Title;
			FString TagStr = Entry.BranchName.IsEmpty() ? TEXT("") :
				FString::Printf(TEXT(" [%s]"), *Entry.BranchName);
			List += FString::Printf(TEXT("%s %s%s  ($%.2f)\n"),
				*Marker, *Display, *TagStr, Entry.Cost);
		}
		AddMessage(FBobBotChatMessage::ESender::System, List.TrimEnd());
	});

	// Wire bridge monitor to chat messaging
	BridgeMonitor.SetMessageCallback([this](bool bError, const FString& Message)
	{
		AddMessage(bError ? FBobBotChatMessage::ESender::Error : FBobBotChatMessage::ESender::System, Message);
	});

	// SDK manages session persistence — no custom file loading needed.
	// ActiveChatId is set from Python's _session_id after first message.
	AddMessage(FBobBotChatMessage::ESender::System, TEXT("BobBot ready. Type a message and press Enter."));
}

FBobBotChatController::~FBobBotChatController()
{
	// Don't touch Python during engine shutdown — the interpreter may be gone
	if (!IsEngineExitRequested())
	{
		KillChatProcess();
	}
}

void FBobBotChatController::KillChatProcess()
{
	if (IsEngineExitRequested())
		return;

	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (Bridge.IsAvailable())
	{
		Bridge.ExecPythonCommand(TEXT("import bob_chat; bob_chat.cleanup()"));
	}
}

// =========================================================================== //
// Slash command dispatch
// =========================================================================== //

void FBobBotChatController::HandleSlashCommand(const FString& Message)
{
	FString Cmd = Message.ToLower();

	// Exact match first (e.g. /clear, /cost, /help)
	if (TFunction<void(const FString&)>* Handler = SlashCommands.Find(Cmd))
	{
		(*Handler)(FString());
		return;
	}

	// Prefix match for commands with arguments (e.g. /model opus)
	for (auto& Pair : SlashCommands)
	{
		if (Cmd.StartsWith(Pair.Key) && Cmd.Len() > Pair.Key.Len())
		{
			FString Args = Message.Mid(Pair.Key.Len());
			Pair.Value(Args);
			return;
		}
	}

	// Unknown slash command — fall through (caller should send as regular message)
}

// =========================================================================== //
// External message (for connect tab notifications)
// =========================================================================== //

void FBobBotChatController::AddExternalMessage(FBobBotChatMessage::ESender Sender, const FString& Content)
{
	AddMessage(Sender, Content);
}

// =========================================================================== //
// Message management
// =========================================================================== //

void FBobBotChatController::AddMessage(FBobBotChatMessage::ESender Sender, const FString& Content, float Cost, int32 DurationMs, int32 NumTurns)
{
	FBobBotChatMessage Msg;
	Msg.Sender = Sender;
	Msg.Content = Content;
	Msg.Timestamp = FDateTime::Now();
	Msg.Cost = Cost;
	Msg.DurationMs = DurationMs;
	Msg.NumTurns = NumTurns;
	ChatHistory.Add(MoveTemp(Msg));

	if (Sender == FBobBotChatMessage::ESender::User || Sender == FBobBotChatMessage::ESender::Bot)
		SessionMessageCount++;
	if (Sender == FBobBotChatMessage::ESender::Bot && Cost > 0.f)
		TotalSessionCost += Cost;

	OnMessageAdded.Broadcast(ChatHistory.Last());
}

// =========================================================================== //
// SendMessage
// =========================================================================== //

void FBobBotChatController::SendMessage(const FString& Message)
{
	FString Trimmed = Message.TrimStartAndEnd();
	if (Trimmed.IsEmpty() || bAiThinking)
		return;

	// Slash commands
	if (Trimmed.StartsWith(TEXT("/")))
	{
		FString Cmd = Trimmed.ToLower();

		// Check exact match
		if (SlashCommands.Contains(Cmd))
		{
			HandleSlashCommand(Trimmed);
			return;
		}

		// Check prefix match (e.g. "/model opus")
		for (auto& Pair : SlashCommands)
		{
			if (Cmd.StartsWith(Pair.Key))
			{
				HandleSlashCommand(Trimmed);
				return;
			}
		}
		// Unknown slash command — fall through to send as regular message
	}

	const FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	if (!Status.bClaudeCodeAvailable)
	{
		AddMessage(FBobBotChatMessage::ESender::Error, TEXT("Claude Code not found. Install it first."));
		return;
	}
	if (!Status.bClaudeAuthenticated)
	{
		AddMessage(FBobBotChatMessage::ESender::Error, TEXT("Not authenticated. Run 'claude login' in a terminal."));
		return;
	}

	AddMessage(FBobBotChatMessage::ESender::User, Trimmed);

	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (!Bridge.IsAvailable())
	{
		AddMessage(FBobBotChatMessage::ESender::Error, TEXT("PythonScriptPlugin not available."));
		return;
	}

	FString MsgFile = Bridge.GetTempDir() / BobBot::TempFiles::ChatMessage;
	FFileHelper::SaveStringToFile(Trimmed, *MsgFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	FString Script = FString::Printf(
		TEXT("import bob_chat; bob_chat.send_message(open(r'%s', encoding='utf-8').read())"),
		*MsgFile.Replace(TEXT("\\"), TEXT("/")));

	if (!Bridge.ExecPythonCommand(Script))
	{
		AddMessage(FBobBotChatMessage::ESender::Error, TEXT("Failed to execute Python chat command."));
		return;
	}

	bAiThinking = true;
	ThinkingDotCount = 0;
	ThinkingAnimTimer = 0.f;
	OnThinkingStateChanged.Broadcast();
}

// =========================================================================== //
// ClearSession / StopChat
// =========================================================================== //

void FBobBotChatController::ClearSession()
{
	ChatHistory.Empty();
	SessionMessageCount = 0;
	TotalSessionCost = 0.f;
	ContextTokensUsed = 0;
	ContextTokensMax = 0;
	StreamingBotMessageIndex = INDEX_NONE;
	ActiveSubagentMessageIndex.Empty();

	OnHistoryCleared.Broadcast();

	FBobBotPythonBridge::Get().ExecPythonCommand(TEXT("import bob_chat; bob_chat.clear_session()"));

	AddMessage(FBobBotChatMessage::ESender::System, TEXT("Chat cleared."));
}

void FBobBotChatController::StopChat()
{
	// Graceful interrupt — keeps persistent client alive, just stops current response
	FBobBotPythonBridge::Get().ExecPythonCommand(TEXT("import bob_chat; bob_chat.interrupt()\n"));

	bAiThinking = false;
	OnThinkingStateChanged.Broadcast();

	AddMessage(FBobBotChatMessage::ESender::System, TEXT("Request interrupted."));
}

void FBobBotChatController::UndoLastMessage()
{
	if (bAiThinking || ChatHistory.Num() == 0) return;

	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (!Bridge.IsAvailable()) return;

	// Call rewind — SDK reverts file changes to the state before the last response
	FString Script =
		TEXT("import bob_chat, json\n")
		TEXT("open(output_path, 'w').write(json.dumps(bob_chat.rewind_to_message('')))\n");

	TSharedPtr<FJsonObject> Result = Bridge.ExecPythonWithJsonResult(Script, TEXT("_undo_result.txt"));

	// Remove last bot + user message pair from display
	while (ChatHistory.Num() > 0)
	{
		auto Sender = ChatHistory.Last().Sender;
		if (Sender == FBobBotChatMessage::ESender::Bot ||
			Sender == FBobBotChatMessage::ESender::ToolCall ||
			Sender == FBobBotChatMessage::ESender::Subagent)
		{
			ChatHistory.Pop();
		}
		else break;
	}
	// Remove the user message that prompted the response
	if (ChatHistory.Num() > 0 && ChatHistory.Last().Sender == FBobBotChatMessage::ESender::User)
	{
		ChatHistory.Pop();
		SessionMessageCount = FMath::Max(0, SessionMessageCount - 1);
	}

	OnHistoryCleared.Broadcast();
	AddMessage(FBobBotChatMessage::ESender::System, TEXT("Last response undone. File changes reverted."));
}

void FBobBotChatController::StopSubagentTask(const FString& TaskId)
{
	if (TaskId.IsEmpty()) return;

	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (!Bridge.IsAvailable()) return;

	FString Script = FString::Printf(
		TEXT("import bob_chat; bob_chat.stop_task('%s')\n"), *TaskId);
	Bridge.ExecPythonCommand(Script);

	// Update status in chat history
	int32* IdxPtr = ActiveSubagentMessageIndex.Find(TaskId);
	if (IdxPtr && ChatHistory.IsValidIndex(*IdxPtr))
	{
		ChatHistory[*IdxPtr].SubagentStatus = TEXT("stopped");
		ChatHistory[*IdxPtr].Content = FString::Printf(
			TEXT("Subagent: %s (stopped)"), *ChatHistory[*IdxPtr].SubagentDescription);
		OnMessageUpdated.Broadcast(*IdxPtr);
	}
	ActiveSubagentMessageIndex.Remove(TaskId);
}

// =========================================================================== //
// Tool Approval ("Ask Me" mode)
// =========================================================================== //

void FBobBotChatController::ApproveExecution()
{
	// Call Python directly via SDK hook — no file I/O
	FBobBotPythonBridge::Get().ExecPythonCommand(
		TEXT("import bob_chat; bob_chat.set_permission_decision('allow')\n"));

	bHasPendingApproval = false;
	OnApprovalStateChanged.Broadcast();

	AddMessage(FBobBotChatMessage::ESender::System, TEXT("Tool execution approved."));
}

void FBobBotChatController::DenyExecution()
{
	// Call Python directly via SDK hook — no file I/O
	FBobBotPythonBridge::Get().ExecPythonCommand(
		TEXT("import bob_chat; bob_chat.set_permission_decision('deny')\n"));

	bHasPendingApproval = false;
	OnApprovalStateChanged.Broadcast();

	AddMessage(FBobBotChatMessage::ESender::System, TEXT("Tool execution denied."));
}

static FString GetCategoryDisplayName(const FString& Category)
{
	if (Category == TEXT("read_only")) return TEXT("Read-only");
	if (Category == TEXT("viewport")) return TEXT("Viewport");
	if (Category == TEXT("create")) return TEXT("Create");
	if (Category == TEXT("modify")) return TEXT("Modify");
	if (Category == TEXT("code_exec")) return TEXT("Code execution");
	return TEXT("Unknown");
}

// =========================================================================== //
// Tick / Polling
// =========================================================================== //

void FBobBotChatController::Tick(float DeltaTime)
{
	// Server status poll
	StatusPollTimer += DeltaTime;
	if (StatusPollTimer >= BobBot::Polling::ServerStatus)
	{
		StatusPollTimer = 0.f;
		PollServerStatus();
	}

	// Chat poll
	ChatPollTimer += DeltaTime;
	float ChatPollInterval = bAiThinking ? BobBot::Polling::ChatActive : BobBot::Polling::ChatIdle;
	if (ChatPollTimer >= ChatPollInterval)
	{
		ChatPollTimer = 0.f;
		PollChatUpdates();
	}

	// Animate thinking dots (cycle every 0.5s)
	if (bAiThinking)
	{
		ThinkingAnimTimer += DeltaTime;
		if (ThinkingAnimTimer >= BobBot::Polling::ThinkingAnim)
		{
			ThinkingAnimTimer = 0.f;
			ThinkingDotCount++;
			OnThinkingStateChanged.Broadcast();
		}
	}

	// Detect thinking state transitions
	if (bAiThinking && !bWasThinking)
	{
		bWasThinking = true;
		OnThinkingStateChanged.Broadcast();
	}
	else if (!bAiThinking && bWasThinking)
	{
		bWasThinking = false;
		OnThinkingStateChanged.Broadcast();
	}
}

void FBobBotChatController::PollServerStatus()
{
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();

	ServerMonitor.Poll(Bridge);
	BridgeMonitor.Poll(Bridge, BobBot::Polling::ServerStatus);
	SdkMonitor.Poll(Bridge);
}

void FBobBotChatController::PollChatUpdates()
{
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (!Bridge.IsAvailable()) return;

	FString Script =
		TEXT("import bob_chat, json\n")
		TEXT("open(output_path, 'w', encoding='utf-8').write(json.dumps(bob_chat.poll()))\n");

	TSharedPtr<FJsonObject> Result = Bridge.ExecPythonWithJsonResult(Script, BobBot::TempFiles::ChatPoll);
	if (!Result.IsValid()) return;

	// Process stream events
	const TArray<TSharedPtr<FJsonValue>>* EventsArr = nullptr;
	if (Result->TryGetArrayField(TEXT("events"), EventsArr))
	{
		for (const TSharedPtr<FJsonValue>& Val : *EventsArr)
		{
			const TSharedPtr<FJsonObject>* EvtObj = nullptr;
			if (!Val.IsValid() || !Val->TryGetObject(EvtObj) || !EvtObj->IsValid())
				continue;

			FString EventType;
			(*EvtObj)->TryGetStringField(TEXT("type"), EventType);

			if (EventType == TEXT("text"))
			{
				FString Text;
				(*EvtObj)->TryGetStringField(TEXT("text"), Text);
				if (!Text.IsEmpty())
				{
					if (StreamingBotMessageIndex != INDEX_NONE && ChatHistory.IsValidIndex(StreamingBotMessageIndex))
					{
						// Update existing streaming message (partial text replaces previous partial)
						ChatHistory[StreamingBotMessageIndex].Content = Text;
						OnMessageUpdated.Broadcast(StreamingBotMessageIndex);
					}
					else
					{
						// First text chunk — add new bot message
						AddMessage(FBobBotChatMessage::ESender::Bot, Text);
						StreamingBotMessageIndex = ChatHistory.Num() - 1;
					}
				}
			}
			else if (EventType == TEXT("tool_use"))
			{
				StreamingBotMessageIndex = INDEX_NONE;  // Tool call interrupts text stream
				FString ToolName, ToolInput;
				(*EvtObj)->TryGetStringField(TEXT("name"), ToolName);
				(*EvtObj)->TryGetStringField(TEXT("input"), ToolInput);

				FBobBotChatMessage ToolMsg;
				ToolMsg.Sender = FBobBotChatMessage::ESender::ToolCall;
				ToolMsg.Content = FString::Printf(TEXT("Tool: %s"), *ToolName);
				ToolMsg.Timestamp = FDateTime::Now();
				ToolMsg.ToolName = ToolName;
				ToolMsg.ToolInput = ToolInput;
				ToolMsg.bToolComplete = false;

				// If a subagent is active, nest the tool call under it
				if (ActiveSubagentMessageIndex.Num() > 0)
				{
					// Add to the most recently started subagent
					int32 SubIdx = INDEX_NONE;
					for (auto& Pair : ActiveSubagentMessageIndex)
						SubIdx = FMath::Max(SubIdx, Pair.Value);
					if (SubIdx != INDEX_NONE && ChatHistory.IsValidIndex(SubIdx))
					{
						ChatHistory[SubIdx].SubagentToolCalls.Add(MoveTemp(ToolMsg));
						OnMessageUpdated.Broadcast(SubIdx);
					}
					else
					{
						ChatHistory.Add(MoveTemp(ToolMsg));
						OnMessageAdded.Broadcast(ChatHistory.Last());
					}
				}
				else
				{
					ChatHistory.Add(MoveTemp(ToolMsg));
					OnMessageAdded.Broadcast(ChatHistory.Last());
				}
			}
			else if (EventType == TEXT("tool_result"))
			{
				FString Output;
				(*EvtObj)->TryGetStringField(TEXT("output"), Output);

				// If subagent is active, mark the tool complete in its nested array
				if (ActiveSubagentMessageIndex.Num() > 0)
				{
					int32 SubIdx = INDEX_NONE;
					for (auto& Pair : ActiveSubagentMessageIndex)
						SubIdx = FMath::Max(SubIdx, Pair.Value);
					if (SubIdx != INDEX_NONE && ChatHistory.IsValidIndex(SubIdx))
					{
						TArray<FBobBotChatMessage>& Tools = ChatHistory[SubIdx].SubagentToolCalls;
						for (int32 i = Tools.Num() - 1; i >= 0; --i)
						{
							if (!Tools[i].bToolComplete)
							{
								Tools[i].bToolComplete = true;
								Tools[i].Content = FString::Printf(TEXT("Tool: %s (done)"), *Tools[i].ToolName);
								OnMessageUpdated.Broadcast(SubIdx);
								break;
							}
						}
					}
				}
				else
				{
					// Find the most recent incomplete tool call and mark it complete
					for (int32 i = ChatHistory.Num() - 1; i >= 0; --i)
					{
						if (ChatHistory[i].Sender == FBobBotChatMessage::ESender::ToolCall && !ChatHistory[i].bToolComplete)
						{
							ChatHistory[i].bToolComplete = true;
							ChatHistory[i].Content = FString::Printf(TEXT("Tool: %s (done)"), *ChatHistory[i].ToolName);
							OnMessageUpdated.Broadcast(i);
							break;
						}
					}
				}
			}
			else if (EventType == TEXT("complete"))
			{
				StreamingBotMessageIndex = INDEX_NONE;  // End of streaming response

				double CostDbl = 0;
				(*EvtObj)->TryGetNumberField(TEXT("cost"), CostDbl);

				double DurDbl = 0;
				(*EvtObj)->TryGetNumberField(TEXT("duration_ms"), DurDbl);

				double TurnsDbl = 1;
				(*EvtObj)->TryGetNumberField(TEXT("num_turns"), TurnsDbl);

				// Update context usage from SDK token data
				double InputTokens = 0, OutputTokens = 0, ContextWindow = 0;
				(*EvtObj)->TryGetNumberField(TEXT("input_tokens"), InputTokens);
				(*EvtObj)->TryGetNumberField(TEXT("output_tokens"), OutputTokens);
				(*EvtObj)->TryGetNumberField(TEXT("context_window"), ContextWindow);
				if (InputTokens > 0)
				{
					ContextTokensUsed = static_cast<int32>(InputTokens);
				}
				if (ContextWindow > 0)
				{
					ContextTokensMax = static_cast<int32>(ContextWindow);
				}

				// Apply cost to the last bot message if there is one
				for (int32 i = ChatHistory.Num() - 1; i >= 0; --i)
				{
					if (ChatHistory[i].Sender == FBobBotChatMessage::ESender::Bot)
					{
						ChatHistory[i].Cost = static_cast<float>(CostDbl);
						ChatHistory[i].DurationMs = static_cast<int32>(DurDbl);
						ChatHistory[i].NumTurns = static_cast<int32>(TurnsDbl);
						TotalSessionCost += static_cast<float>(CostDbl);
						OnMessageUpdated.Broadcast(i);
						break;
					}
				}

				// Update ActiveChatId from SDK session_id (assigned after first message)
				if (ActiveChatId.IsEmpty())
				{
					TSharedPtr<FJsonObject> SidResult = Bridge.ExecPythonWithJsonResult(
						TEXT("import bob_chat, json\nopen(output_path, 'w').write(json.dumps({'sid': bob_chat.get_session_id() or ''}))\n"),
						TEXT("_session_id.txt"));
					if (SidResult.IsValid())
					{
						FString Sid;
						SidResult->TryGetStringField(TEXT("sid"), Sid);
						if (!Sid.IsEmpty())
						{
							ActiveChatId = Sid;
						}
					}
				}
			}
			else if (EventType == TEXT("error"))
			{
				FString ErrorMsg;
				(*EvtObj)->TryGetStringField(TEXT("message"), ErrorMsg);
				if (!ErrorMsg.IsEmpty())
				{
					AddMessage(FBobBotChatMessage::ESender::Error, ErrorMsg);
					// SDK auto-persists sessions
				}
			}
			else if (EventType == TEXT("subagent_start"))
			{
				FString TaskId, Description;
				(*EvtObj)->TryGetStringField(TEXT("task_id"), TaskId);
				(*EvtObj)->TryGetStringField(TEXT("description"), Description);

				FBobBotChatMessage Msg;
				Msg.Sender = FBobBotChatMessage::ESender::Subagent;
				Msg.SubagentTaskId = TaskId;
				Msg.SubagentDescription = Description;
				Msg.SubagentStatus = TEXT("running");
				Msg.Content = FString::Printf(TEXT("Subagent: %s"), *Description);
				Msg.Timestamp = FDateTime::Now();
				ChatHistory.Add(MoveTemp(Msg));
				ActiveSubagentMessageIndex.Add(TaskId, ChatHistory.Num() - 1);
				OnMessageAdded.Broadcast(ChatHistory.Last());
			}
			else if (EventType == TEXT("subagent_progress"))
			{
				FString TaskId;
				(*EvtObj)->TryGetStringField(TEXT("task_id"), TaskId);
				int32* IdxPtr = ActiveSubagentMessageIndex.Find(TaskId);
				if (IdxPtr && ChatHistory.IsValidIndex(*IdxPtr))
				{
					FBobBotChatMessage& Msg = ChatHistory[*IdxPtr];
					double Tokens = 0, ToolUses = 0, DurMs = 0;
					(*EvtObj)->TryGetNumberField(TEXT("total_tokens"), Tokens);
					(*EvtObj)->TryGetNumberField(TEXT("tool_uses"), ToolUses);
					(*EvtObj)->TryGetNumberField(TEXT("duration_ms"), DurMs);
					FString LastTool;
					(*EvtObj)->TryGetStringField(TEXT("last_tool_name"), LastTool);
					Msg.SubagentTokens = static_cast<int32>(Tokens);
					Msg.SubagentToolUses = static_cast<int32>(ToolUses);
					Msg.SubagentDurationMs = static_cast<int32>(DurMs);
					if (!LastTool.IsEmpty())
						Msg.Content = FString::Printf(TEXT("Subagent: %s (%s)"), *Msg.SubagentDescription, *LastTool);
					OnMessageUpdated.Broadcast(*IdxPtr);
				}
			}
			else if (EventType == TEXT("subagent_complete"))
			{
				FString TaskId, Status, Summary;
				(*EvtObj)->TryGetStringField(TEXT("task_id"), TaskId);
				(*EvtObj)->TryGetStringField(TEXT("status"), Status);
				(*EvtObj)->TryGetStringField(TEXT("summary"), Summary);
				int32* IdxPtr = ActiveSubagentMessageIndex.Find(TaskId);
				if (IdxPtr && ChatHistory.IsValidIndex(*IdxPtr))
				{
					FBobBotChatMessage& Msg = ChatHistory[*IdxPtr];
					Msg.SubagentStatus = Status;
					Msg.SubagentSummary = Summary;
					double Tokens = 0, ToolUses = 0, DurMs = 0;
					(*EvtObj)->TryGetNumberField(TEXT("total_tokens"), Tokens);
					(*EvtObj)->TryGetNumberField(TEXT("tool_uses"), ToolUses);
					(*EvtObj)->TryGetNumberField(TEXT("duration_ms"), DurMs);
					Msg.SubagentTokens = static_cast<int32>(Tokens);
					Msg.SubagentToolUses = static_cast<int32>(ToolUses);
					Msg.SubagentDurationMs = static_cast<int32>(DurMs);
					Msg.Content = FString::Printf(TEXT("Subagent: %s (%s)"), *Msg.SubagentDescription, *Status);
					OnMessageUpdated.Broadcast(*IdxPtr);
				}
				ActiveSubagentMessageIndex.Remove(TaskId);
				// SDK auto-persists sessions
			}

			// --- Hook-driven events ---
			else if (EventType == TEXT("approval_request"))
			{
				double IdDbl = 0;
				(*EvtObj)->TryGetNumberField(TEXT("id"), IdDbl);
				PendingApprovalId = static_cast<int32>(IdDbl);
				PendingApprovalTool.Empty();
				PendingApprovalCode.Empty();
				PendingApprovalCategory.Empty();
				(*EvtObj)->TryGetStringField(TEXT("tool"), PendingApprovalTool);
				(*EvtObj)->TryGetStringField(TEXT("input"), PendingApprovalCode);
				(*EvtObj)->TryGetStringField(TEXT("category"), PendingApprovalCategory);
				bHasPendingApproval = true;
				OnApprovalStateChanged.Broadcast();

				FString CategoryDisplay = GetCategoryDisplayName(PendingApprovalCategory);
				FString ApprovalContent = FString::Printf(
					TEXT("Tool: %s\nCategory: %s (requires approval)\n\n%s"),
					*PendingApprovalTool, *CategoryDisplay, *PendingApprovalCode);
				AddMessage(FBobBotChatMessage::ESender::Approval, ApprovalContent);
			}
			else if (EventType == TEXT("notification"))
			{
				FString Msg;
				(*EvtObj)->TryGetStringField(TEXT("message"), Msg);
				if (!Msg.IsEmpty())
				{
					AddMessage(FBobBotChatMessage::ESender::System, Msg);
				}
			}
			else if (EventType == TEXT("hook_tool_error"))
			{
				FString Tool, Error;
				(*EvtObj)->TryGetStringField(TEXT("tool"), Tool);
				(*EvtObj)->TryGetStringField(TEXT("error"), Error);
				if (!Error.IsEmpty())
				{
					AddMessage(FBobBotChatMessage::ESender::Error,
						FString::Printf(TEXT("Tool '%s' failed: %s"), *Tool, *Error));
				}
			}
		}
	}

	// Thinking state
	bool bThinking = false;
	Result->TryGetBoolField(TEXT("thinking"), bThinking);
	if (bThinking != bAiThinking)
	{
		bAiThinking = bThinking;
	}
}

// =========================================================================== //
// Session management — SDK handles persistence
// =========================================================================== //

void FBobBotChatController::RefreshChatIndex()
{
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (!Bridge.IsAvailable()) return;

	FString Script =
		TEXT("import bob_chat, json\n")
		TEXT("open(output_path, 'w').write(json.dumps(bob_chat.list_saved_sessions(50, 0)))\n");

	TSharedPtr<FJsonObject> Wrapper = MakeShareable(new FJsonObject);
	FString ResultStr;
	FString OutputPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / BobBot::SavedSubDir / TEXT("_sessions_list.txt"));

	Bridge.ExecPythonCommand(FString::Printf(
		TEXT("import bob_chat, json\noutput_path = r'%s'\nopen(output_path, 'w', encoding='utf-8').write(json.dumps(bob_chat.list_saved_sessions(50, 0)))\n"),
		*OutputPath.Replace(TEXT("\\"), TEXT("/"))));

	if (!FFileHelper::LoadFileToString(ResultStr, *OutputPath))
		return;

	TArray<TSharedPtr<FJsonValue>> SessionsArr;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResultStr);
	if (!FJsonSerializer::Deserialize(Reader, SessionsArr))
		return;

	ChatIndex.Empty();
	for (const TSharedPtr<FJsonValue>& Val : SessionsArr)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Val.IsValid() || !Val->TryGetObject(Obj) || !Obj->IsValid())
			continue;

		FBobBotChatEntry Entry;
		(*Obj)->TryGetStringField(TEXT("session_id"), Entry.Id);

		// Display: custom_title > summary > first_prompt > "(untitled)"
		FString CustomTitle, Summary, FirstPrompt;
		(*Obj)->TryGetStringField(TEXT("custom_title"), CustomTitle);
		(*Obj)->TryGetStringField(TEXT("summary"), Summary);
		(*Obj)->TryGetStringField(TEXT("first_prompt"), FirstPrompt);

		if (!CustomTitle.IsEmpty())
			Entry.Title = CustomTitle;
		else if (!Summary.IsEmpty())
			Entry.Title = Summary.Left(60);
		else if (!FirstPrompt.IsEmpty())
			Entry.Title = FirstPrompt.Left(40) + (FirstPrompt.Len() > 40 ? TEXT("...") : TEXT(""));
		else
			Entry.Title = TEXT("(untitled)");

		// Use tag field for BranchName display
		(*Obj)->TryGetStringField(TEXT("tag"), Entry.BranchName);

		double LastMod = 0;
		(*Obj)->TryGetNumberField(TEXT("last_modified"), LastMod);
		Entry.Updated = FDateTime::FromUnixTimestamp(static_cast<int64>(LastMod / 1000.0));

		Entry.Model = FBobBotConfig::Get().ChatModel;

		ChatIndex.Add(MoveTemp(Entry));
	}
}

// =========================================================================== //
// Multi-chat: NewChat, SwitchChat, DeleteChat
// =========================================================================== //

void FBobBotChatController::NewChat()
{
	// Clear state — SDK manages session persistence
	ActiveChatId.Empty();
	ChatHistory.Empty();
	SessionMessageCount = 0;
	TotalSessionCost = 0.f;
	ContextTokensUsed = 0;
	ContextTokensMax = 0;
	bAiThinking = false;
	ActiveSubagentMessageIndex.Empty();

	// Clear Python session — next message creates fresh client
	FBobBotPythonBridge::Get().ExecPythonCommand(TEXT("import bob_chat; bob_chat.clear_session()"));

	OnHistoryCleared.Broadcast();
	OnChatListChanged.Broadcast();

	AddMessage(FBobBotChatMessage::ESender::System, TEXT("New conversation started."));
}

void FBobBotChatController::SwitchChat(const FString& ChatId)
{
	if (ChatId == ActiveChatId)
		return;

	// Switch to SDK session — client reconnects on next message
	ActiveChatId = ChatId;
	ChatHistory.Empty();
	SessionMessageCount = 0;
	TotalSessionCost = 0.f;
	ContextTokensUsed = 0;
	ContextTokensMax = 0;
	ActiveSubagentMessageIndex.Empty();
	bAiThinking = false;

	// Tell Python to use this session — _ensure_client() will reconnect with resume
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	Bridge.ExecCallWithString(TEXT("bob_chat"), TEXT("set_session_id"), ChatId);

	// Load conversation transcript from SDK for display
	LoadSessionMessages(ChatId);

	OnHistoryCleared.Broadcast();
	OnChatListChanged.Broadcast();
}

void FBobBotChatController::LoadSessionMessages(const FString& SessionId)
{
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (!Bridge.IsAvailable()) return;

	FString OutputPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / BobBot::SavedSubDir / TEXT("_session_msgs.txt"));

	FString Script = FString::Printf(
		TEXT("import bob_chat, json\nopen(r'%s', 'w', encoding='utf-8').write(json.dumps(bob_chat.get_saved_session_messages('%s', 200, 0)))\n"),
		*OutputPath.Replace(TEXT("\\"), TEXT("/")), *SessionId);

	Bridge.ExecPythonCommand(Script);

	FString ResultStr;
	if (!FFileHelper::LoadFileToString(ResultStr, *OutputPath))
		return;

	TArray<TSharedPtr<FJsonValue>> MessagesArr;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResultStr);
	if (!FJsonSerializer::Deserialize(Reader, MessagesArr))
		return;

	for (const TSharedPtr<FJsonValue>& Val : MessagesArr)
	{
		const TSharedPtr<FJsonObject>* MsgObj = nullptr;
		if (!Val.IsValid() || !Val->TryGetObject(MsgObj) || !MsgObj->IsValid())
			continue;

		FString Type;
		(*MsgObj)->TryGetStringField(TEXT("type"), Type);

		// Extract text from raw API message dict
		FString Text;
		const TSharedPtr<FJsonObject>* RawMsg = nullptr;
		if ((*MsgObj)->TryGetObjectField(TEXT("message"), RawMsg) && RawMsg->IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
			if ((*RawMsg)->TryGetArrayField(TEXT("content"), Content))
			{
				for (const TSharedPtr<FJsonValue>& Block : *Content)
				{
					const TSharedPtr<FJsonObject>* BlockObj = nullptr;
					if (!Block.IsValid() || !Block->TryGetObject(BlockObj) || !BlockObj->IsValid())
						continue;
					FString BlockType;
					(*BlockObj)->TryGetStringField(TEXT("type"), BlockType);
					if (BlockType == TEXT("text"))
					{
						FString BlockText;
						(*BlockObj)->TryGetStringField(TEXT("text"), BlockText);
						if (!BlockText.IsEmpty())
						{
							if (!Text.IsEmpty()) Text += TEXT("\n");
							Text += BlockText;
						}
					}
				}
			}
			// Fallback: try top-level "content" as string
			if (Text.IsEmpty())
			{
				(*RawMsg)->TryGetStringField(TEXT("content"), Text);
			}
		}

		if (Text.IsEmpty())
			continue;

		FBobBotChatMessage::ESender Sender =
			(Type == TEXT("user")) ? FBobBotChatMessage::ESender::User
			                       : FBobBotChatMessage::ESender::Bot;
		FBobBotChatMessage Msg;
		Msg.Sender = Sender;
		Msg.Content = Text;
		Msg.Timestamp = FDateTime::Now();
		ChatHistory.Add(MoveTemp(Msg));
		SessionMessageCount++;
	}
}

void FBobBotChatController::DeleteChat(const FString& ChatId)
{
	// Delete via SDK
	FBobBotPythonBridge::Get().CallPythonJson(TEXT("bob_chat"), TEXT("delete_saved_session"),
		FBobBotPythonBridge::PyStr(ChatId));

	// Remove from cached index
	ChatIndex.RemoveAll([&ChatId](const FBobBotChatEntry& E) { return E.Id == ChatId; });

	// If we deleted the active chat, start fresh
	if (ChatId == ActiveChatId)
	{
		ActiveChatId.Empty();
		bAiThinking = false;
		ChatHistory.Empty();
		SessionMessageCount = 0;
		TotalSessionCost = 0.f;
		ContextTokensUsed = 0;
		ContextTokensMax = 0;
		ActiveSubagentMessageIndex.Empty();

		FBobBotPythonBridge::Get().ExecPythonCommand(TEXT("import bob_chat; bob_chat.clear_session()"));

		AddMessage(FBobBotChatMessage::ESender::System,
			TEXT("BobBot ready. Type a message and press Enter."));

		OnHistoryCleared.Broadcast();
	}

	OnChatListChanged.Broadcast();
}

bool FBobBotChatController::CanFork() const
{
	return !bAiThinking && !ActiveChatId.IsEmpty();
}

void FBobBotChatController::ForkChat()
{
	if (!CanFork()) return;

	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (!Bridge.IsAvailable())
	{
		AddMessage(FBobBotChatMessage::ESender::Error, TEXT("Fork failed: Python bridge not available."));
		return;
	}

	FString Script =
		TEXT("import bob_chat, json\n")
		TEXT("open(output_path, 'w').write(json.dumps(bob_chat.fork_current_session()))\n");

	TSharedPtr<FJsonObject> Result = Bridge.ExecPythonWithJsonResult(Script, TEXT("_fork_result.txt"));
	if (!Result.IsValid())
	{
		AddMessage(FBobBotChatMessage::ESender::Error, TEXT("Fork failed: no response from Python."));
		return;
	}

	bool bOk = false;
	Result->TryGetBoolField(TEXT("ok"), bOk);
	if (!bOk)
	{
		FString Err;
		Result->TryGetStringField(TEXT("error"), Err);
		AddMessage(FBobBotChatMessage::ESender::Error,
			FString::Printf(TEXT("Fork failed: %s"), *Err));
		return;
	}

	FString NewSessionId;
	Result->TryGetStringField(TEXT("session_id"), NewSessionId);

	// Switch to the forked session — SDK manages persistence
	FString ParentTitle = GetActiveChatTitle();
	ActiveChatId = NewSessionId;
	Bridge.ExecCallWithString(TEXT("bob_chat"), TEXT("set_session_id"), NewSessionId);

	AddMessage(FBobBotChatMessage::ESender::System,
		FString::Printf(TEXT("Forked from \"%s\""), *ParentTitle));

	OnChatListChanged.Broadcast();
}

FString FBobBotChatController::GetActiveChatTitle() const
{
	for (const FBobBotChatEntry& Entry : ChatIndex)
	{
		if (Entry.Id == ActiveChatId)
			return Entry.Title;
	}
	return TEXT("New Chat");
}

// =========================================================================== //
// Tool classification (mirrors Python's _classify_tool for UI use)
// =========================================================================== //

FString FBobBotChatController::ClassifyTool(const FString& ToolName)
{
	// Explicit overrides (same as Python's _TOOL_CATEGORY_OVERRIDES)
	static TMap<FString, FString> Overrides = {
		{TEXT("ping_unreal"), TEXT("read_only")},
		{TEXT("get_bobbot_status"), TEXT("read_only")},
		{TEXT("validate_assets"), TEXT("read_only")},
		{TEXT("benchmark_scene"), TEXT("read_only")},
		{TEXT("focus_on_actor"), TEXT("viewport")},
		{TEXT("deselect_all"), TEXT("modify")},
		{TEXT("select_actors"), TEXT("modify")},
		{TEXT("undo"), TEXT("modify")},
		{TEXT("redo"), TEXT("modify")},
		{TEXT("start_pie"), TEXT("modify")},
		{TEXT("stop_pie"), TEXT("modify")},
		{TEXT("play_sequence"), TEXT("modify")},
		{TEXT("save_current_level"), TEXT("modify")},
		{TEXT("open_level"), TEXT("modify")},
		{TEXT("compile_blueprints"), TEXT("modify")},
		{TEXT("build_lighting"), TEXT("modify")},
		{TEXT("render_sequence_to_images"), TEXT("modify")},
		{TEXT("export_asset"), TEXT("modify")},
		{TEXT("execute_pcg_graph"), TEXT("modify")},
		{TEXT("import_asset"), TEXT("create")},
		{TEXT("import_fbx"), TEXT("create")},
	};

	if (const FString* Cat = Overrides.Find(ToolName))
		return *Cat;

	// Prefix matching (order matters — code_exec before read_only to catch execute_*)
	if (ToolName == TEXT("execute_unreal_python") || ToolName == TEXT("run_console_command") || ToolName == TEXT("execute_pie_console_command"))
		return TEXT("code_exec");
	if (ToolName.StartsWith(TEXT("capture_")) || ToolName == TEXT("set_viewport_camera") || ToolName == TEXT("get_active_viewport_camera") || ToolName == TEXT("get_output_log"))
		return TEXT("viewport");
	if (ToolName.StartsWith(TEXT("get_")) || ToolName.StartsWith(TEXT("search_")) || ToolName.StartsWith(TEXT("is_")) || ToolName.StartsWith(TEXT("list_")))
		return TEXT("read_only");
	if (ToolName.StartsWith(TEXT("spawn_")) || ToolName.StartsWith(TEXT("create_")) || ToolName.StartsWith(TEXT("add_")))
		return TEXT("create");
	if (ToolName.StartsWith(TEXT("set_")) || ToolName.StartsWith(TEXT("delete_")) || ToolName.StartsWith(TEXT("remove_"))
		|| ToolName.StartsWith(TEXT("rename_")) || ToolName.StartsWith(TEXT("duplicate_")) || ToolName.StartsWith(TEXT("move_"))
		|| ToolName.StartsWith(TEXT("connect_")) || ToolName.StartsWith(TEXT("attach_"))
		|| ToolName.StartsWith(TEXT("check_out_")) || ToolName.StartsWith(TEXT("check_in_")) || ToolName.StartsWith(TEXT("revert_")))
		return TEXT("modify");

	return TEXT("code_exec"); // Unknown defaults to most restrictive
}

bool FBobBotChatController::IsToolAutoApproved(const FString& ToolName)
{
	const FBobBotConfig& Config = FBobBotConfig::Get();
	if (Config.PermissionMode != EBobBotPermissionMode::AskBeforeEdits)
		return true; // EditAutomatically or Plan — SDK handles permissions

	FString Cat = ClassifyTool(ToolName);
	if (Cat == TEXT("read_only")) return Config.bAutoApproveReadOnly;
	if (Cat == TEXT("viewport")) return Config.bAutoApproveViewport;
	if (Cat == TEXT("create")) return Config.bAutoApproveCreate;
	if (Cat == TEXT("modify")) return Config.bAutoApproveModify;
	if (Cat == TEXT("code_exec")) return Config.bAutoApproveCodeExec;
	return false;
}

#undef LOCTEXT_NAMESPACE
