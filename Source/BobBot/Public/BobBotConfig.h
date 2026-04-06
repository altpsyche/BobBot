// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"

/** Tool permission modes. */
enum class EBobBotPermissionMode : uint8
{
	Plan,               // plan — read-only, BobBot suggests but doesn't execute
	AskBeforeEdits,     // acceptEdits — allows reads, asks before writes/creates
	EditAutomatically,  // bypassPermissions — BobBot does everything without asking
};

/** Authentication mode: subscription (OAuth) or user-provided API key. */
enum class EBobBotAuthMode : uint8
{
	Subscription,  // Claude Code subscription via OAuth
	ApiKey,        // User's own Anthropic / Bedrock / Vertex API key
};

/**
 * Centralized configuration for BobBot.
 * Single source of truth for port, host, and all settings.
 * Persisted to Saved/Config/BobBot.ini.
 */
class FBobBotConfig
{
public:
	static FBobBotConfig& Get();

	void Load();
	void Save();

	/** Apply config values as environment variables for Python scripts. */
	void ApplyEnvironmentVars() const;

	/** Convert a permission mode enum to its Python string representation. */
	static const TCHAR* PermissionModeToString(EBobBotPermissionMode Mode);

	// --- Server Settings ---
	int32 Port = 13579;
	FString Host = TEXT("127.0.0.1");
	bool bAutoStartServer = true;
	bool bAutoGenerateMcpJson = true;
	int32 MaxClients = 4;
	int32 RateLimitPerSecond = 30;

	// --- HTTP Bridge Settings ---
	int32 BridgePort = 13580;
	bool bAutoStartBridge = true;

	// Per-launch random token gating the HTTP bridge and TCP server.
	// External MCP clients (Cursor, VS Code) must include this in their
	// `headers: { "X-Bobbot-Token": ... }` config or be rejected. The
	// token is regenerated on every editor launch and never written to
	// the on-disk INI — only to the in-memory env var and the
	// auto-generated _bobbot_mcp.json/_bobbot_mcp_fallback.json files.
	FString BridgeAuthToken;

	/** Generate a fresh random token. Called once on module startup. */
	void RegenerateBridgeAuthToken();

	// --- AI Chat Settings ---
	FString ChatModel = TEXT("sonnet");  // BobBot::ModelNames::Sonnet / Opus / Haiku
	EBobBotPermissionMode PermissionMode = EBobBotPermissionMode::AskBeforeEdits;
	FString SystemPrompt;  // empty = use default from bob_chat.py
	int32 ChatTimeoutSeconds = 300;
	float MaxBudgetUsd = 5.0f;  // Per-message cost budget (0 = unlimited)

	// --- Extended Thinking & Effort ---
	FString ThinkingMode = TEXT("disabled");  // "disabled", "enabled", "adaptive"
	int32 ThinkingBudget = 10000;             // Token budget for thinking (when enabled)
	FString EffortLevel = TEXT("high");        // "low", "medium", "high", "max"

	// --- Auto-approve categories (AskBeforeEdits mode) ---
	bool bAutoApproveReadOnly = true;    // get_*, search_*, is_*, list_*
	bool bAutoApproveViewport = true;    // capture_*, viewport camera tools
	bool bAutoApproveCreate = false;     // spawn_*, create_*, add_*
	bool bAutoApproveModify = false;     // set_*, delete_*, remove_*
	bool bAutoApproveCodeExec = false;   // execute_unreal_python, run_console_command

	// --- FTUE ---
	bool bSetupComplete = false;  // True after Welcome tab completes; prevents it from showing again

	// --- Authentication ---
	EBobBotAuthMode AuthMode = EBobBotAuthMode::Subscription;
	FString ApiKey;           // Stored in Windows Credential Manager (falls back to INI for migration)
	FString ApiProvider = TEXT("anthropic");  // "anthropic", "bedrock", "vertex"
	FString ApiRegion;        // AWS region for Bedrock, GCP region for Vertex
	FString ApiProjectId;     // GCP project ID for Vertex

private:
	FBobBotConfig() = default;

	static constexpr const TCHAR* ConfigSection = TEXT("BobBot");
	static constexpr const TCHAR* ConfigFileName = TEXT("BobBot");

	FString GetConfigFilePath() const;
};
