// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "BobBotStatusMonitors.h"
#include "BobBot.h"
#include "BobBotConstants.h"
#include "BobBotPythonBridge.h"
#include "BobBotRuntimeStatus.h"
#include "Dom/JsonObject.h"

// =========================================================================== //
// FBobBotServerMonitor
// =========================================================================== //

void FBobBotServerMonitor::Poll(FBobBotPythonBridge& Bridge)
{
	if (!Bridge.IsAvailable())
	{
		bRunning = false;
		ClientCount = 0;
		return;
	}

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
		StatusObj->TryGetBoolField(TEXT("running"), bRunning);
		double ClientCountDbl = 0;
		if (StatusObj->TryGetNumberField(TEXT("client_count"), ClientCountDbl))
			ClientCount = static_cast<int32>(ClientCountDbl);
	}
}

// =========================================================================== //
// FBobBotBridgeMonitor
// =========================================================================== //

void FBobBotBridgeMonitor::Poll(FBobBotPythonBridge& Bridge, float PollInterval)
{
	if (!Bridge.IsAvailable())
	{
		bWasRunning = false;
		return;
	}

	FBobBotRuntimeStatus& BridgeStatus = FBobBotRuntimeStatus::Get();

	FString BridgeScript =
		TEXT("import bob_bridge_launcher, json\n")
		TEXT("open(output_path, 'w').write(json.dumps(bob_bridge_launcher.check_health()))\n");

	TSharedPtr<FJsonObject> BridgeObj = Bridge.ExecPythonWithJsonResult(BridgeScript, TEXT("_bridge_health.txt"));
	if (!BridgeObj.IsValid())
		return;

	bool bOk = false;
	BridgeObj->TryGetBoolField(TEXT("ok"), bOk);
	BridgeStatus.bBridgeRunning = bOk;

	double PortDbl = 0;
	if (BridgeObj->TryGetNumberField(TEXT("port"), PortDbl))
		BridgeStatus.BridgePort = static_cast<int32>(PortDbl);

	FString StatusStr;
	if (BridgeObj->TryGetStringField(TEXT("status"), StatusStr))
		BridgeStatus.BridgeHealth = StatusStr;

	// Reconnection: detect bridge death and attempt restart
	if (bWasRunning && !bOk && ReconnectAttempts < MaxReconnectAttempts)
	{
		ReconnectCooldown -= PollInterval;
		if (ReconnectCooldown <= 0.f)
		{
			++ReconnectAttempts;
			if (MessageCallback)
			{
				MessageCallback(false,
					FString::Printf(TEXT("Bridge lost. Reconnecting... (attempt %d/%d)"),
						ReconnectAttempts, MaxReconnectAttempts));
			}

			Bridge.ExecPythonCommand(TEXT("import bob_bridge_launcher; bob_bridge_launcher.start()"));
			ReconnectCooldown = 5.f;  // Wait 5s between attempts
		}
	}
	else if (bOk)
	{
		if (!bWasRunning && ReconnectAttempts > 0)
		{
			if (MessageCallback)
			{
				MessageCallback(false, TEXT("Bridge reconnected."));
			}
		}
		ReconnectAttempts = 0;
		ReconnectCooldown = 0.f;
	}
	else if (!bOk && ReconnectAttempts >= MaxReconnectAttempts && bWasRunning)
	{
		// Exhausted retries — only notify once
		if (ReconnectAttempts == MaxReconnectAttempts)
		{
			++ReconnectAttempts;  // Prevent repeat message
			if (MessageCallback)
			{
				MessageCallback(true,
					TEXT("Bridge restart failed. Use the Connect tab to restart manually."));
			}
		}
	}
	bWasRunning = bOk;
}

// =========================================================================== //
// FBobBotSdkMonitor
// =========================================================================== //

void FBobBotSdkMonitor::Poll(FBobBotPythonBridge& Bridge)
{
	if (bDone)
		return;

	FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();
	if (Status.bAgentSDKAvailable)
	{
		bDone = true;
		return;
	}

	if (PollCount >= MaxPolls)
	{
		bDone = true;
		return;
	}

	++PollCount;
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
			bDone = true;
		}
	}
}
