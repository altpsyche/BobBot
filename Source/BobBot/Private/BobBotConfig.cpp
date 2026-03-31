// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "BobBotConfig.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"

FBobBotConfig& FBobBotConfig::Get()
{
	static FBobBotConfig Instance;
	return Instance;
}

FString FBobBotConfig::GetConfigFilePath() const
{
	FString Path = FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("BobBot.ini");
	FPaths::NormalizeFilename(Path);
	FConfigCacheIni::NormalizeConfigIniPath(Path);
	return Path;
}

void FBobBotConfig::Load()
{
	const FString FilePath = GetConfigFilePath();

	if (!GConfig)
	{
		return;
	}

	FConfigFile* ConfigFile = GConfig->FindConfigFile(FilePath);
	if (!ConfigFile)
	{
		return;
	}

	GConfig->GetInt(ConfigSection, TEXT("Port"), Port, FilePath);
	GConfig->GetString(ConfigSection, TEXT("Host"), Host, FilePath);
	GConfig->GetBool(ConfigSection, TEXT("bAutoStartServer"), bAutoStartServer, FilePath);
	GConfig->GetBool(ConfigSection, TEXT("bAutoGenerateMcpJson"), bAutoGenerateMcpJson, FilePath);
	GConfig->GetInt(ConfigSection, TEXT("MaxClients"), MaxClients, FilePath);
	GConfig->GetInt(ConfigSection, TEXT("RateLimitPerSecond"), RateLimitPerSecond, FilePath);
	GConfig->GetInt(ConfigSection, TEXT("BridgePort"), BridgePort, FilePath);
	GConfig->GetBool(ConfigSection, TEXT("bAutoStartBridge"), bAutoStartBridge, FilePath);
	GConfig->GetBool(ConfigSection, TEXT("bUseAgentSDK"), bUseAgentSDK, FilePath);
	GConfig->GetString(ConfigSection, TEXT("ChatModel"), ChatModel, FilePath);
	GConfig->GetString(ConfigSection, TEXT("SystemPrompt"), SystemPrompt, FilePath);
	GConfig->GetInt(ConfigSection, TEXT("ChatTimeoutSeconds"), ChatTimeoutSeconds, FilePath);

	int32 PermModeInt = static_cast<int32>(PermissionMode);
	GConfig->GetInt(ConfigSection, TEXT("PermissionMode"), PermModeInt, FilePath);
	PermissionMode = static_cast<EBobBotPermissionMode>(FMath::Clamp(PermModeInt, 0, 2));

	float BudgetFloat = MaxBudgetUsd;
	GConfig->GetFloat(ConfigSection, TEXT("MaxBudgetUsd"), BudgetFloat, FilePath);
	MaxBudgetUsd = FMath::Max(0.f, BudgetFloat);

	int32 AuthModeInt = static_cast<int32>(AuthMode);
	GConfig->GetInt(ConfigSection, TEXT("AuthMode"), AuthModeInt, FilePath);
	AuthMode = static_cast<EBobBotAuthMode>(FMath::Clamp(AuthModeInt, 0, 1));

	GConfig->GetBool(ConfigSection, TEXT("bSetupComplete"), bSetupComplete, FilePath);
	GConfig->GetString(ConfigSection, TEXT("ApiKey"), ApiKey, FilePath);
	GConfig->GetString(ConfigSection, TEXT("ApiProvider"), ApiProvider, FilePath);
	GConfig->GetString(ConfigSection, TEXT("ApiRegion"), ApiRegion, FilePath);
	GConfig->GetString(ConfigSection, TEXT("ApiProjectId"), ApiProjectId, FilePath);

	Port = FMath::Clamp(Port, 1, 65535);
	BridgePort = FMath::Clamp(BridgePort, 1, 65535);
	MaxClients = FMath::Clamp(MaxClients, 1, 16);
	RateLimitPerSecond = FMath::Clamp(RateLimitPerSecond, 1, 1000);
	ChatTimeoutSeconds = FMath::Clamp(ChatTimeoutSeconds, 10, 3600);
}

void FBobBotConfig::Save()
{
	const FString FilePath = GetConfigFilePath();

	GConfig->SetInt(ConfigSection, TEXT("Port"), Port, FilePath);
	GConfig->SetString(ConfigSection, TEXT("Host"), *Host, FilePath);
	GConfig->SetBool(ConfigSection, TEXT("bAutoStartServer"), bAutoStartServer, FilePath);
	GConfig->SetBool(ConfigSection, TEXT("bAutoGenerateMcpJson"), bAutoGenerateMcpJson, FilePath);
	GConfig->SetInt(ConfigSection, TEXT("MaxClients"), MaxClients, FilePath);
	GConfig->SetInt(ConfigSection, TEXT("RateLimitPerSecond"), RateLimitPerSecond, FilePath);
	GConfig->SetInt(ConfigSection, TEXT("BridgePort"), BridgePort, FilePath);
	GConfig->SetBool(ConfigSection, TEXT("bAutoStartBridge"), bAutoStartBridge, FilePath);
	GConfig->SetBool(ConfigSection, TEXT("bUseAgentSDK"), bUseAgentSDK, FilePath);
	GConfig->SetString(ConfigSection, TEXT("ChatModel"), *ChatModel, FilePath);
	GConfig->SetString(ConfigSection, TEXT("SystemPrompt"), *SystemPrompt, FilePath);
	GConfig->SetInt(ConfigSection, TEXT("ChatTimeoutSeconds"), ChatTimeoutSeconds, FilePath);
	GConfig->SetInt(ConfigSection, TEXT("PermissionMode"), static_cast<int32>(PermissionMode), FilePath);
	GConfig->SetFloat(ConfigSection, TEXT("MaxBudgetUsd"), MaxBudgetUsd, FilePath);
	GConfig->SetBool(ConfigSection, TEXT("bSetupComplete"), bSetupComplete, FilePath);
	GConfig->SetInt(ConfigSection, TEXT("AuthMode"), static_cast<int32>(AuthMode), FilePath);
	GConfig->SetString(ConfigSection, TEXT("ApiKey"), *ApiKey, FilePath);
	GConfig->SetString(ConfigSection, TEXT("ApiProvider"), *ApiProvider, FilePath);
	GConfig->SetString(ConfigSection, TEXT("ApiRegion"), *ApiRegion, FilePath);
	GConfig->SetString(ConfigSection, TEXT("ApiProjectId"), *ApiProjectId, FilePath);

	GConfig->Flush(false, FilePath);
}

const TCHAR* FBobBotConfig::PermissionModeToString(EBobBotPermissionMode Mode)
{
	switch (Mode)
	{
	case EBobBotPermissionMode::AskMe:    return TEXT("ask_me");
	case EBobBotPermissionMode::ChatOnly: return TEXT("chat_only");
	default:                              return TEXT("allow_always");
	}
}

void FBobBotConfig::ApplyEnvironmentVars() const
{
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_MCP_PORT"), *FString::FromInt(Port));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_MCP_HOST"), *Host);
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_MCP_MAX_CLIENTS"), *FString::FromInt(MaxClients));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_MCP_RATE_LIMIT"), *FString::FromInt(RateLimitPerSecond));

	// Set project root for bob_chat.py — use the .uproject file's directory for absolute path
	FString ProjectRoot = FPaths::GetPath(FPaths::GetProjectFilePath());
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_PROJECT_ROOT"), *ProjectRoot);

	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_MCP_BRIDGE_PORT"), *FString::FromInt(BridgePort));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_USE_SDK"), bUseAgentSDK ? TEXT("1") : TEXT("0"));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_PERMISSION_MODE"), PermissionModeToString(PermissionMode));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_CHAT_TIMEOUT"), *FString::FromInt(ChatTimeoutSeconds));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_MAX_BUDGET"), *FString::Printf(TEXT("%.2f"), MaxBudgetUsd));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_AUTH_MODE"), AuthMode == EBobBotAuthMode::ApiKey ? TEXT("api_key") : TEXT("subscription"));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_API_KEY"), *ApiKey);
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_API_PROVIDER"), *ApiProvider);
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_API_REGION"), *ApiRegion);
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_API_PROJECT_ID"), *ApiProjectId);
}
