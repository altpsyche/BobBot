// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"

/**
 * Transient runtime status populated at startup and by DetectClaudeCli.
 * NOT persisted to INI. Separated from FBobBotConfig to keep config
 * concerns (what the user chose) distinct from runtime state (what was detected).
 */
struct FBobBotRuntimeStatus
{
	static FBobBotRuntimeStatus& Get()
	{
		static FBobBotRuntimeStatus Instance;
		return Instance;
	}

	// --- Prerequisites (populated by FBobBotModule::CheckPrerequisites) ---
	bool bPythonAvailable = false;
	FString PythonVersion;
	bool bUvAvailable = false;
	FString UvVersion;
	bool bPythonPluginAvailable = false;
	bool bAgentSDKAvailable = false;
	FString AgentSDKVersion;

	// --- Claude CLI (populated by DetectClaudeCli) ---
	bool bClaudeCodeAvailable = false;
	FString ClaudeCodeVersion;
	bool bClaudeAuthenticated = false;
	FString ClaudeAuthStatus;

	// --- HTTP Bridge (populated by PollServerStatus) ---
	bool bBridgeRunning = false;
	int32 BridgePort = 0;
	FString BridgeHealth;  // "Connected", "Starting", "Stopped", "Unreachable"

	// --- Backend mode ---
	bool bSDKActive = false;  // True when Agent SDK process is alive
	FString LastBackendError;

	// --- API key validation ---
	bool bApiKeyValid = false;
	FString ApiKeyStatus;  // "Valid", "Invalid", "No key set"

	/** True when Claude Code is installed and authenticated — ready to chat. */
	bool IsReadyToChat() const
	{
		return bClaudeCodeAvailable && bClaudeAuthenticated;
	}

private:
	FBobBotRuntimeStatus() = default;
};
