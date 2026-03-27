// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"

/** Tool permission modes. */
enum class EBobBotPermissionMode : uint8
{
	AllowAlways,  // Claude can run code without asking
	ChatOnly,     // Claude answers only, no tool access
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

	// --- Server Settings ---
	int32 Port = 13579;
	FString Host = TEXT("127.0.0.1");
	bool bAutoStartServer = true;
	bool bAutoGenerateMcpJson = true;
	int32 MaxClients = 2;
	int32 RateLimitPerSecond = 30;

	// --- AI Chat Settings ---
	FString ChatModel = TEXT("sonnet");  // "sonnet", "opus", or "haiku"
	EBobBotPermissionMode PermissionMode = EBobBotPermissionMode::AllowAlways;
	FString SystemPrompt;  // empty = use default from bob_chat.py
	int32 ChatTimeoutSeconds = 300;

	// --- Prerequisite Status (populated at startup, not persisted) ---
	bool bPythonAvailable = false;
	FString PythonVersion;
	bool bUvAvailable = false;
	FString UvVersion;
	bool bPythonPluginAvailable = false;
	bool bClaudeCodeAvailable = false;
	FString ClaudeCodeVersion;
	bool bClaudeAuthenticated = false;
	FString ClaudeAuthStatus;

private:
	FBobBotConfig() = default;

	static constexpr const TCHAR* ConfigSection = TEXT("BobBot");
	static constexpr const TCHAR* ConfigFileName = TEXT("BobBot");

	FString GetConfigFilePath() const;
};
