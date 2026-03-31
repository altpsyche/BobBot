// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"

/** Tool permission modes. */
enum class EBobBotPermissionMode : uint8
{
	AllowAlways,  // Claude can run code without asking
	AskMe,        // User must approve each tool execution in the UI
	ChatOnly,     // Claude answers only, no tool access
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

	// --- AI Chat Settings ---
	bool bUseAgentSDK = true;  // Use claude-agent-sdk for persistent process (default)
	FString ChatModel = TEXT("sonnet");  // "sonnet", "opus", or "haiku"
	EBobBotPermissionMode PermissionMode = EBobBotPermissionMode::AskMe;
	FString SystemPrompt;  // empty = use default from bob_chat.py
	int32 ChatTimeoutSeconds = 300;
	float MaxBudgetUsd = 5.0f;  // Per-message cost budget (0 = unlimited)

	// --- Authentication ---
	EBobBotAuthMode AuthMode = EBobBotAuthMode::Subscription;
	FString ApiKey;           // Stored in INI (TODO: OS keychain for security)
	FString ApiProvider = TEXT("anthropic");  // "anthropic", "bedrock", "vertex"
	FString ApiRegion;        // AWS region for Bedrock, GCP region for Vertex
	FString ApiProjectId;     // GCP project ID for Vertex

private:
	FBobBotConfig() = default;

	static constexpr const TCHAR* ConfigSection = TEXT("BobBot");
	static constexpr const TCHAR* ConfigFileName = TEXT("BobBot");

	FString GetConfigFilePath() const;
};
