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
	GConfig->GetString(ConfigSection, TEXT("ChatModel"), ChatModel, FilePath);
	GConfig->GetString(ConfigSection, TEXT("SystemPrompt"), SystemPrompt, FilePath);
	GConfig->GetInt(ConfigSection, TEXT("ChatTimeoutSeconds"), ChatTimeoutSeconds, FilePath);

	int32 PermModeInt = static_cast<int32>(PermissionMode);
	GConfig->GetInt(ConfigSection, TEXT("PermissionMode"), PermModeInt, FilePath);
	PermissionMode = static_cast<EBobBotPermissionMode>(FMath::Clamp(PermModeInt, 0, 2));

	Port = FMath::Clamp(Port, 1, 65535);
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
	GConfig->SetString(ConfigSection, TEXT("ChatModel"), *ChatModel, FilePath);
	GConfig->SetString(ConfigSection, TEXT("SystemPrompt"), *SystemPrompt, FilePath);
	GConfig->SetInt(ConfigSection, TEXT("ChatTimeoutSeconds"), ChatTimeoutSeconds, FilePath);
	GConfig->SetInt(ConfigSection, TEXT("PermissionMode"), static_cast<int32>(PermissionMode), FilePath);

	GConfig->Flush(false, FilePath);
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

	// Permission mode: "allow_always", "ask_me", or "chat_only"
	const TCHAR* ModeStr = TEXT("allow_always");
	if (PermissionMode == EBobBotPermissionMode::AskMe)
		ModeStr = TEXT("ask_me");
	else if (PermissionMode == EBobBotPermissionMode::ChatOnly)
		ModeStr = TEXT("chat_only");
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_PERMISSION_MODE"), ModeStr);
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_CHAT_TIMEOUT"), *FString::FromInt(ChatTimeoutSeconds));
}
