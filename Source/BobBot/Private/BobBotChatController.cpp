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

	SlashCommands.Add(TEXT("/help"), [this](const FString&)
	{
		AddMessage(FBobBotChatMessage::ESender::System,
			TEXT("Commands: /clear  /cost  /model [sonnet|opus|haiku]  /effort [low|medium|high|max]  /thinking [on|off|adaptive]  /new  /chats  /help"));
	});

	SlashCommands.Add(TEXT("/new"), [this](const FString&)
	{
		NewChat();
	});

	SlashCommands.Add(TEXT("/chats"), [this](const FString&)
	{
		if (ChatIndex.Num() == 0)
		{
			AddMessage(FBobBotChatMessage::ESender::System, TEXT("No conversations yet."));
			return;
		}
		FString List;
		for (const FBobBotChatEntry& Entry : ChatIndex)
		{
			FString Marker = (Entry.Id == ActiveChatId) ? TEXT("*") : TEXT(" ");
			List += FString::Printf(TEXT("%s %s  ($%.2f)\n"), *Marker, *Entry.Title, Entry.Cost);
		}
		AddMessage(FBobBotChatMessage::ESender::System, List.TrimEnd());
	});

	// Load chat index and migrate legacy single-file history if needed
	MigrateLegacyHistory();
	LoadChatIndex();

	if (ActiveChatId.IsEmpty())
	{
		// No chats exist yet, create the first one
		ActiveChatId = FGuid::NewGuid().ToString();
	}

	LoadChatHistory();

	if (ChatHistory.Num() == 0)
	{
		AddMessage(FBobBotChatMessage::ESender::System, TEXT("BobBot ready. Type a message and press Enter to chat with Claude."));
	}
}

FBobBotChatController::~FBobBotChatController()
{
	// Don't touch Python during engine shutdown — the interpreter may be gone
	if (!IsEngineExitRequested())
	{
		SaveChatHistory();
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

	// Persist after user, system, and error messages (not during streaming bot responses)
	if (Sender != FBobBotChatMessage::ESender::Bot)
	{
		SaveChatHistory();
	}
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
	ActiveSubagentMessageIndex.Empty();

	OnHistoryCleared.Broadcast();

	// Delete persisted history
	FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*GetChatHistoryPath());

	FBobBotPythonBridge::Get().ExecPythonCommand(TEXT("import bob_chat; bob_chat.clear_session()"));

	AddMessage(FBobBotChatMessage::ESender::System, TEXT("Chat cleared."));
}

void FBobBotChatController::StopChat()
{
	// Graceful interrupt — keeps persistent client alive, just stops current response
	FBobBotPythonBridge::Get().ExecPythonCommand(TEXT("import bob_chat; bob_chat.interrupt()\n"));

	bAiThinking = false;
	OnThinkingStateChanged.Broadcast();

	AddMessage(FBobBotChatMessage::ESender::System, TEXT("Request stopped."));
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
	if (!Bridge.IsAvailable()) { bServerRunning = false; ConnectedClientCount = 0; return; }

	FString Script =
		TEXT("import json\n")
		TEXT("try:\n")
		TEXT("    import bob_mcp_server\n")
		TEXT("    open(output_path, 'w').write(json.dumps(bob_mcp_server.get_status()))\n")
		TEXT("except Exception:\n")
		TEXT("    open(output_path, 'w').write('{\"running\":false,\"client_count\":0}')\n");

	TSharedPtr<FJsonObject> StatusObj = Bridge.ExecPythonWithJsonResult(Script, BobBot::TempFiles::StatusOutput);
	if (StatusObj.IsValid())
	{
		StatusObj->TryGetBoolField(TEXT("running"), bServerRunning);
		double ClientCountDbl = 0;
		if (StatusObj->TryGetNumberField(TEXT("client_count"), ClientCountDbl))
			ConnectedClientCount = static_cast<int32>(ClientCountDbl);
	}

	// Poll HTTP bridge health
	{
		FBobBotRuntimeStatus& BridgeStatus = FBobBotRuntimeStatus::Get();

		FString BridgeScript =
			TEXT("import bob_bridge_launcher, json\n")
			TEXT("open(output_path, 'w').write(json.dumps(bob_bridge_launcher.check_health()))\n");

		TSharedPtr<FJsonObject> BridgeObj = Bridge.ExecPythonWithJsonResult(BridgeScript, TEXT("_bridge_health.txt"));
		if (BridgeObj.IsValid())
		{
			bool bOk = false;
			BridgeObj->TryGetBoolField(TEXT("ok"), bOk);
			BridgeStatus.bBridgeRunning = bOk;
			double PortDbl = 0;
			if (BridgeObj->TryGetNumberField(TEXT("port"), PortDbl))
				BridgeStatus.BridgePort = static_cast<int32>(PortDbl);
			FString StatusStr;
			if (BridgeObj->TryGetStringField(TEXT("status"), StatusStr))
				BridgeStatus.BridgeHealth = StatusStr;

		}
	}

	// Poll SDK availability — stop after 60 polls (~2 min) to avoid polling forever
	static int32 SdkPollCount = 0;
	static constexpr int32 MaxSdkPolls = 60;
	FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	if (!Status.bAgentSDKAvailable && SdkPollCount < MaxSdkPolls)
	{
		++SdkPollCount;
		FString SdkScript =
			TEXT("import json\n")
			TEXT("try:\n")
			TEXT("    import bob_chat_sdk\n")
			TEXT("    open(output_path, 'w').write(json.dumps(bob_chat_sdk.check_sdk_available()))\n")
			TEXT("except Exception: open(output_path, 'w').write('{\"ok\":false,\"ver\":\"\"}')\n");

		TSharedPtr<FJsonObject> SdkObj = Bridge.ExecPythonWithJsonResult(SdkScript, TEXT("_sdk_check.txt"));
		if (SdkObj.IsValid())
		{
			bool bOk = false;
			if (SdkObj->TryGetBoolField(TEXT("ok"), bOk) && bOk)
			{
				Status.bAgentSDKAvailable = true;
				FString Ver;
				if (SdkObj->TryGetStringField(TEXT("ver"), Ver) && !Ver.IsEmpty())
					Status.AgentSDKVersion = Ver;
				UE_LOG(LogBobBot, Log, TEXT("claude-agent-sdk now available: %s"), *Status.AgentSDKVersion);
			}
		}
	}
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
					AddMessage(FBobBotChatMessage::ESender::Bot, Text);
				}
			}
			else if (EventType == TEXT("tool_use"))
			{
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

				SaveChatHistory();
			}
			else if (EventType == TEXT("error"))
			{
				FString ErrorMsg;
				(*EvtObj)->TryGetStringField(TEXT("message"), ErrorMsg);
				if (!ErrorMsg.IsEmpty())
				{
					AddMessage(FBobBotChatMessage::ESender::Error, ErrorMsg);
					SaveChatHistory();
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
				SaveChatHistory();
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
// Chat history persistence
// =========================================================================== //

FString FBobBotChatController::GetChatsDir()
{
	return FPaths::ProjectSavedDir() / BobBot::SavedSubDir / TEXT("chats");
}

FString FBobBotChatController::GetChatIndexPath()
{
	return GetChatsDir() / TEXT("_index.json");
}

FString FBobBotChatController::GetChatHistoryPath() const
{
	return GetChatsDir() / (ActiveChatId + TEXT(".json"));
}

void FBobBotChatController::SaveChatHistory()
{
	// Update index entry with current title/cost/timestamp
	UpdateIndexEntry();

	TArray<TSharedPtr<FJsonValue>> MessagesArray;
	for (const FBobBotChatMessage& Msg : ChatHistory)
	{
		TSharedPtr<FJsonObject> MsgObj = MakeShareable(new FJsonObject);
		MsgObj->SetNumberField(TEXT("sender"), static_cast<int32>(Msg.Sender));
		MsgObj->SetStringField(TEXT("content"), Msg.Content);
		MsgObj->SetStringField(TEXT("timestamp"), Msg.Timestamp.ToIso8601());
		MsgObj->SetNumberField(TEXT("cost"), Msg.Cost);
		MsgObj->SetNumberField(TEXT("duration_ms"), Msg.DurationMs);
		MsgObj->SetNumberField(TEXT("num_turns"), Msg.NumTurns);
		if (Msg.Sender == FBobBotChatMessage::ESender::ToolCall)
		{
			MsgObj->SetStringField(TEXT("tool_name"), Msg.ToolName);
			MsgObj->SetStringField(TEXT("tool_input"), Msg.ToolInput);
			MsgObj->SetBoolField(TEXT("tool_complete"), Msg.bToolComplete);
		}
		MessagesArray.Add(MakeShareable(new FJsonValueObject(MsgObj)));
	}

	// Get session ID from Python to persist
	FString SessionId;
	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	TSharedPtr<FJsonObject> SidResult = Bridge.ExecPythonWithJsonResult(
		TEXT("import bob_chat, json\nopen(output_path, 'w').write(json.dumps({'sid': bob_chat.get_session_id() or ''}))\n"),
		TEXT("_session_id.txt"));
	if (SidResult.IsValid())
		SidResult->TryGetStringField(TEXT("sid"), SessionId);

	TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject);
	Root->SetArrayField(TEXT("messages"), MessagesArray);
	Root->SetNumberField(TEXT("session_cost"), TotalSessionCost);
	Root->SetNumberField(TEXT("message_count"), SessionMessageCount);
	Root->SetStringField(TEXT("session_id"), SessionId);
	Root->SetNumberField(TEXT("context_tokens_used"), ContextTokensUsed);
	Root->SetNumberField(TEXT("context_tokens_max"), ContextTokensMax);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	FString Path = GetChatHistoryPath();
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	PF.CreateDirectoryTree(*FPaths::GetPath(Path));
	FFileHelper::SaveStringToFile(Output, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	SaveChatIndex();
}

void FBobBotChatController::LoadChatHistory()
{
	FString Path = GetChatHistoryPath();
	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *Path))
		return;

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return;

	const TArray<TSharedPtr<FJsonValue>>* MessagesArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("messages"), MessagesArray))
		return;

	for (const TSharedPtr<FJsonValue>& Val : *MessagesArray)
	{
		const TSharedPtr<FJsonObject>* MsgObj = nullptr;
		if (!Val.IsValid() || !Val->TryGetObject(MsgObj) || !MsgObj->IsValid())
			continue;

		FBobBotChatMessage Msg;
		double SenderDbl = 0;
		(*MsgObj)->TryGetNumberField(TEXT("sender"), SenderDbl);
		Msg.Sender = static_cast<FBobBotChatMessage::ESender>(FMath::Clamp(static_cast<int32>(SenderDbl), 0, 6));

		(*MsgObj)->TryGetStringField(TEXT("content"), Msg.Content);

		FString TimestampStr;
		if ((*MsgObj)->TryGetStringField(TEXT("timestamp"), TimestampStr))
			FDateTime::ParseIso8601(*TimestampStr, Msg.Timestamp);

		double CostDbl = 0;
		(*MsgObj)->TryGetNumberField(TEXT("cost"), CostDbl);
		Msg.Cost = static_cast<float>(CostDbl);

		double DurDbl = 0;
		(*MsgObj)->TryGetNumberField(TEXT("duration_ms"), DurDbl);
		Msg.DurationMs = static_cast<int32>(DurDbl);

		double TurnsDbl = 0;
		(*MsgObj)->TryGetNumberField(TEXT("num_turns"), TurnsDbl);
		Msg.NumTurns = static_cast<int32>(TurnsDbl);

		if (Msg.Sender == FBobBotChatMessage::ESender::ToolCall)
		{
			(*MsgObj)->TryGetStringField(TEXT("tool_name"), Msg.ToolName);
			(*MsgObj)->TryGetStringField(TEXT("tool_input"), Msg.ToolInput);
			(*MsgObj)->TryGetBoolField(TEXT("tool_complete"), Msg.bToolComplete);
		}

		ChatHistory.Add(MoveTemp(Msg));
	}

	double SessionCostDbl = 0;
	if (Root->TryGetNumberField(TEXT("session_cost"), SessionCostDbl))
		TotalSessionCost = static_cast<float>(SessionCostDbl);

	double MsgCountDbl = 0;
	if (Root->TryGetNumberField(TEXT("message_count"), MsgCountDbl))
		SessionMessageCount = static_cast<int32>(MsgCountDbl);

	double CtxUsedDbl = 0, CtxMaxDbl = 0;
	if (Root->TryGetNumberField(TEXT("context_tokens_used"), CtxUsedDbl))
		ContextTokensUsed = static_cast<int32>(CtxUsedDbl);
	if (Root->TryGetNumberField(TEXT("context_tokens_max"), CtxMaxDbl))
		ContextTokensMax = static_cast<int32>(CtxMaxDbl);

	// Restore session ID to Python for multi-turn continuity
	FString SessionId;
	if (Root->TryGetStringField(TEXT("session_id"), SessionId) && !SessionId.IsEmpty())
	{
		FBobBotPythonBridge::Get().ExecCallWithString(TEXT("bob_chat"), TEXT("set_session_id"), SessionId);
	}

	// Note: No delegate broadcast here — the chat tab subscribes after construction
	// and will do an initial rebuild from GetHistory().
}

// =========================================================================== //
// Multi-chat: index persistence
// =========================================================================== //

void FBobBotChatController::SaveChatIndex()
{
	TArray<TSharedPtr<FJsonValue>> ChatsArr;
	for (const FBobBotChatEntry& Entry : ChatIndex)
	{
		TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
		Obj->SetStringField(TEXT("id"), Entry.Id);
		Obj->SetStringField(TEXT("title"), Entry.Title);
		Obj->SetStringField(TEXT("model"), Entry.Model);
		Obj->SetNumberField(TEXT("cost"), Entry.Cost);
		Obj->SetStringField(TEXT("updated"), Entry.Updated.ToIso8601());
		if (!Entry.ParentId.IsEmpty())
			Obj->SetStringField(TEXT("parent_id"), Entry.ParentId);
		if (!Entry.BranchName.IsEmpty())
			Obj->SetStringField(TEXT("branch_name"), Entry.BranchName);
		ChatsArr.Add(MakeShareable(new FJsonValueObject(Obj)));
	}

	TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject);
	Root->SetStringField(TEXT("active"), ActiveChatId);
	Root->SetArrayField(TEXT("chats"), ChatsArr);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	FString Path = GetChatIndexPath();
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	PF.CreateDirectoryTree(*FPaths::GetPath(Path));
	FFileHelper::SaveStringToFile(Output, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void FBobBotChatController::LoadChatIndex()
{
	FString Path = GetChatIndexPath();
	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *Path))
		return;

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return;

	Root->TryGetStringField(TEXT("active"), ActiveChatId);

	const TArray<TSharedPtr<FJsonValue>>* ChatsArr = nullptr;
	if (!Root->TryGetArrayField(TEXT("chats"), ChatsArr))
		return;

	ChatIndex.Empty();
	for (const TSharedPtr<FJsonValue>& Val : *ChatsArr)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Val.IsValid() || !Val->TryGetObject(Obj) || !Obj->IsValid())
			continue;

		FBobBotChatEntry Entry;
		(*Obj)->TryGetStringField(TEXT("id"), Entry.Id);
		(*Obj)->TryGetStringField(TEXT("title"), Entry.Title);
		(*Obj)->TryGetStringField(TEXT("model"), Entry.Model);

		double CostDbl = 0;
		(*Obj)->TryGetNumberField(TEXT("cost"), CostDbl);
		Entry.Cost = static_cast<float>(CostDbl);

		FString UpdatedStr;
		if ((*Obj)->TryGetStringField(TEXT("updated"), UpdatedStr))
			FDateTime::ParseIso8601(*UpdatedStr, Entry.Updated);
		(*Obj)->TryGetStringField(TEXT("parent_id"), Entry.ParentId);
		(*Obj)->TryGetStringField(TEXT("branch_name"), Entry.BranchName);

		ChatIndex.Add(MoveTemp(Entry));
	}
}

void FBobBotChatController::UpdateIndexEntry()
{
	// Find or create the entry for the active chat
	FBobBotChatEntry* Found = nullptr;
	for (FBobBotChatEntry& Entry : ChatIndex)
	{
		if (Entry.Id == ActiveChatId)
		{
			Found = &Entry;
			break;
		}
	}

	if (!Found)
	{
		FBobBotChatEntry NewEntry;
		NewEntry.Id = ActiveChatId;
		ChatIndex.Add(MoveTemp(NewEntry));
		Found = &ChatIndex.Last();
	}

	// Auto-title from first user message
	if (Found->Title.IsEmpty())
	{
		for (const FBobBotChatMessage& Msg : ChatHistory)
		{
			if (Msg.Sender == FBobBotChatMessage::ESender::User)
			{
				Found->Title = Msg.Content.Left(40).TrimEnd();
				if (Msg.Content.Len() > 40)
					Found->Title += TEXT("...");
				break;
			}
		}
		if (Found->Title.IsEmpty())
			Found->Title = TEXT("New Chat");
	}

	Found->Model = FBobBotConfig::Get().ChatModel;
	Found->Cost = TotalSessionCost;
	Found->Updated = FDateTime::Now();
}

void FBobBotChatController::MigrateLegacyHistory()
{
	// Migrate from the old single-file format to per-chat files
	FString LegacyPath = FPaths::ProjectSavedDir() / BobBot::SavedSubDir / BobBot::TempFiles::ChatHistory;
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*LegacyPath))
		return;

	// Read legacy file, write to a new chat file, delete legacy
	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *LegacyPath))
		return;

	FString NewId = FGuid::NewGuid().ToString();
	FString ChatsDir = GetChatsDir();
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	PF.CreateDirectoryTree(*ChatsDir);

	FString NewPath = ChatsDir / (NewId + TEXT(".json"));
	FFileHelper::SaveStringToFile(JsonStr, *NewPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	// Create an index entry for the migrated chat
	ActiveChatId = NewId;

	// Write a minimal index
	FBobBotChatEntry Entry;
	Entry.Id = NewId;
	Entry.Title = TEXT("Migrated Chat");
	Entry.Model = FBobBotConfig::Get().ChatModel;
	Entry.Updated = FDateTime::Now();
	ChatIndex.Add(MoveTemp(Entry));
	SaveChatIndex();

	// Delete legacy file
	PF.DeleteFile(*LegacyPath);

	UE_LOG(LogBobBot, Log, TEXT("BobBot: Migrated legacy chat history to %s"), *NewPath);
}

// =========================================================================== //
// Multi-chat: NewChat, SwitchChat, DeleteChat
// =========================================================================== //

void FBobBotChatController::NewChat()
{
	// Save current chat first
	UpdateIndexEntry();
	SaveChatHistory();
	SaveChatIndex();

	// Generate new ID and clear state
	ActiveChatId = FGuid::NewGuid().ToString();
	ChatHistory.Empty();
	SessionMessageCount = 0;
	TotalSessionCost = 0.f;
	ContextTokensUsed = 0;
	ContextTokensMax = 0;
	bAiThinking = false;
	ActiveSubagentMessageIndex.Empty();

	// Kill subprocess + clear Python session
	FBobBotPythonBridge::Get().ExecPythonCommand(TEXT("import bob_chat; bob_chat.cleanup()"));

	OnHistoryCleared.Broadcast();
	OnChatListChanged.Broadcast();

	AddMessage(FBobBotChatMessage::ESender::System, TEXT("New conversation started."));
}

void FBobBotChatController::SwitchChat(const FString& ChatId)
{
	if (ChatId == ActiveChatId)
		return;

	// Save current chat
	UpdateIndexEntry();
	SaveChatHistory();
	SaveChatIndex();

	// Switch
	ActiveChatId = ChatId;
	ChatHistory.Empty();
	SessionMessageCount = 0;
	TotalSessionCost = 0.f;
	ContextTokensUsed = 0;
	ContextTokensMax = 0;
	ActiveSubagentMessageIndex.Empty();
	bAiThinking = false;

	LoadChatHistory();

	OnHistoryCleared.Broadcast();
	OnChatListChanged.Broadcast();
}

void FBobBotChatController::DeleteChat(const FString& ChatId)
{
	// Remove from index
	ChatIndex.RemoveAll([&ChatId](const FBobBotChatEntry& E) { return E.Id == ChatId; });

	// Delete file
	FString FilePath = GetChatsDir() / (ChatId + TEXT(".json"));
	FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*FilePath);

	// If we deleted the active chat, clean up and switch without re-saving the deleted chat
	if (ChatId == ActiveChatId)
	{
		// Kill subprocess + clear session (cleanup() handles both after the Python fix)
		KillChatProcess();
		bAiThinking = false;
		ChatHistory.Empty();
		SessionMessageCount = 0;
		TotalSessionCost = 0.f;
		ContextTokensUsed = 0;
		ContextTokensMax = 0;
		ActiveSubagentMessageIndex.Empty();

		if (ChatIndex.Num() > 0)
		{
			ActiveChatId = ChatIndex[0].Id;
			LoadChatHistory();
		}
		else
		{
			ActiveChatId = FGuid::NewGuid().ToString();
			AddMessage(FBobBotChatMessage::ESender::System,
				TEXT("BobBot ready. Type a message and press Enter to chat with Claude."));
		}

		OnHistoryCleared.Broadcast();
	}

	SaveChatIndex();
	OnChatListChanged.Broadcast();
}

bool FBobBotChatController::CanFork() const
{
	return !bAiThinking && FBobBotConfig::Get().bUseAgentSDK && !ActiveChatId.IsEmpty();
}

void FBobBotChatController::ForkChat()
{
	if (!CanFork()) return;

	// 1. Save current state
	UpdateIndexEntry();
	SaveChatHistory();
	SaveChatIndex();

	// 2. Call Python fork_session via SDK
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

	// 3. Create new chat entry with parent reference
	FString ParentChatId = ActiveChatId;
	FString ParentTitle = GetActiveChatTitle();
	FString NewChatId = FGuid::NewGuid().ToString();

	FBobBotChatEntry NewEntry;
	NewEntry.Id = NewChatId;
	NewEntry.Title = ParentTitle + TEXT(" (fork)");
	NewEntry.Model = FBobBotConfig::Get().ChatModel;
	NewEntry.Cost = TotalSessionCost;
	NewEntry.Updated = FDateTime::Now();
	NewEntry.ParentId = ParentChatId;
	NewEntry.BranchName = TEXT("fork");
	ChatIndex.Add(MoveTemp(NewEntry));

	// 4. Switch to the forked chat
	ActiveChatId = NewChatId;

	// Set the forked session ID in Python
	Bridge.ExecCallWithString(TEXT("bob_chat"), TEXT("set_session_id"), NewSessionId);

	// Insert fork marker
	AddMessage(FBobBotChatMessage::ESender::System,
		FString::Printf(TEXT("\x2500\x2500 forked from \"%s\" \x2500\x2500"), *ParentTitle));

	SaveChatHistory();
	SaveChatIndex();
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
	if (Config.PermissionMode != EBobBotPermissionMode::AskMe)
		return true; // AllowAlways mode — everything is approved

	FString Cat = ClassifyTool(ToolName);
	if (Cat == TEXT("read_only")) return Config.bAutoApproveReadOnly;
	if (Cat == TEXT("viewport")) return Config.bAutoApproveViewport;
	if (Cat == TEXT("create")) return Config.bAutoApproveCreate;
	if (Cat == TEXT("modify")) return Config.bAutoApproveModify;
	if (Cat == TEXT("code_exec")) return Config.bAutoApproveCodeExec;
	return false;
}

#undef LOCTEXT_NAMESPACE
