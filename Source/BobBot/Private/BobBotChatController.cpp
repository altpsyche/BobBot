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

	SlashCommands.Add(TEXT("/help"), [this](const FString&)
	{
		AddMessage(FBobBotChatMessage::ESender::System,
			TEXT("Commands: /clear  /cost  /model [sonnet|opus|haiku]  /new  /chats  /help"));
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

	OnHistoryCleared.Broadcast();

	// Delete persisted history
	FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*GetChatHistoryPath());

	FBobBotPythonBridge::Get().ExecPythonCommand(TEXT("import bob_chat; bob_chat.clear_session()"));

	AddMessage(FBobBotChatMessage::ESender::System, TEXT("Chat cleared."));
}

void FBobBotChatController::StopChat()
{
	FBobBotPythonBridge::Get().ExecPythonCommand(BobBot::Scripts::StopChatProcess);

	bAiThinking = false;
	OnThinkingStateChanged.Broadcast();

	AddMessage(FBobBotChatMessage::ESender::System, TEXT("Request stopped."));
}

// =========================================================================== //
// Tool Approval ("Ask Me" mode)
// =========================================================================== //

void FBobBotChatController::ApproveExecution()
{
	FString ResponseFile = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / BobBot::SavedSubDir / BobBot::TempFiles::ApprovalResponse);
	FString Json = FString::Printf(TEXT("{\"id\":%d,\"decision\":\"approved\"}"), PendingApprovalId);
	FFileHelper::SaveStringToFile(Json, *ResponseFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	bHasPendingApproval = false;
	OnApprovalStateChanged.Broadcast();

	AddMessage(FBobBotChatMessage::ESender::System, TEXT("Tool execution approved."));
}

void FBobBotChatController::DenyExecution()
{
	FString ResponseFile = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / BobBot::SavedSubDir / BobBot::TempFiles::ApprovalResponse);
	FString Json = FString::Printf(TEXT("{\"id\":%d,\"decision\":\"denied\"}"), PendingApprovalId);
	FFileHelper::SaveStringToFile(Json, *ResponseFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	bHasPendingApproval = false;
	OnApprovalStateChanged.Broadcast();

	AddMessage(FBobBotChatMessage::ESender::System, TEXT("Tool execution denied."));
}

void FBobBotChatController::PollApprovalRequests()
{
	if (FBobBotConfig::Get().PermissionMode != EBobBotPermissionMode::AskMe)
		return;

	FString ApprovalFile = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / BobBot::SavedSubDir / BobBot::TempFiles::ApprovalPending);
	FString ApprovalJson;
	if (!FFileHelper::LoadFileToString(ApprovalJson, *ApprovalFile))
	{
		if (bHasPendingApproval)
		{
			// File was removed by server (approval resolved or timed out)
			bHasPendingApproval = false;
			OnApprovalStateChanged.Broadcast();
		}
		return;
	}

	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ApprovalJson);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) return;

	double IdDbl = 0;
	Obj->TryGetNumberField(TEXT("id"), IdDbl);
	int32 Id = static_cast<int32>(IdDbl);

	if (Id == PendingApprovalId && bHasPendingApproval)
		return; // Already showing this one

	PendingApprovalId = Id;
	PendingApprovalTool.Empty();
	PendingApprovalCode.Empty();
	Obj->TryGetStringField(TEXT("tool"), PendingApprovalTool);
	Obj->TryGetStringField(TEXT("code"), PendingApprovalCode);
	bHasPendingApproval = true;

	OnApprovalStateChanged.Broadcast();

	// Show the approval request in chat
	FString ApprovalContent = FString::Printf(
		TEXT("Tool: %s\n\n%s"),
		*PendingApprovalTool, *PendingApprovalCode);
	AddMessage(FBobBotChatMessage::ESender::Approval, ApprovalContent);
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
		PollApprovalRequests();
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

	// Poll SDK availability (calls bob_chat_sdk.check_sdk_available())
	FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	if (!Status.bAgentSDKAvailable)
	{
		FString SdkScript =
			TEXT("import json\n")
			TEXT("try:\n")
			TEXT("    import bob_chat_sdk\n")
			TEXT("    open(output_path, 'w').write(json.dumps(bob_chat_sdk.check_sdk_available()))\n")
			TEXT("except: open(output_path, 'w').write('{\"ok\":false,\"ver\":\"\"}')\n");

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

				FBobBotChatMessage Msg;
				Msg.Sender = FBobBotChatMessage::ESender::ToolCall;
				Msg.Content = FString::Printf(TEXT("Tool: %s"), *ToolName);
				Msg.Timestamp = FDateTime::Now();
				Msg.ToolName = ToolName;
				Msg.ToolInput = ToolInput;
				Msg.bToolComplete = false;
				ChatHistory.Add(MoveTemp(Msg));
				OnMessageAdded.Broadcast(ChatHistory.Last());
			}
			else if (EventType == TEXT("tool_result"))
			{
				FString Output;
				(*EvtObj)->TryGetStringField(TEXT("output"), Output);

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
			else if (EventType == TEXT("complete"))
			{
				double CostDbl = 0;
				(*EvtObj)->TryGetNumberField(TEXT("cost"), CostDbl);

				double DurDbl = 0;
				(*EvtObj)->TryGetNumberField(TEXT("duration_ms"), DurDbl);

				double TurnsDbl = 1;
				(*EvtObj)->TryGetNumberField(TEXT("num_turns"), TurnsDbl);

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
		Msg.Sender = static_cast<FBobBotChatMessage::ESender>(FMath::Clamp(static_cast<int32>(SenderDbl), 0, 5));

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
	bAiThinking = false;

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

FString FBobBotChatController::GetActiveChatTitle() const
{
	for (const FBobBotChatEntry& Entry : ChatIndex)
	{
		if (Entry.Id == ActiveChatId)
			return Entry.Title;
	}
	return TEXT("New Chat");
}

#undef LOCTEXT_NAMESPACE
