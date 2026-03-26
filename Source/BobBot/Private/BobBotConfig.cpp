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
	return FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("BobBot.ini");
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

	Port = FMath::Clamp(Port, 1, 65535);
	MaxClients = FMath::Clamp(MaxClients, 1, 16);
	RateLimitPerSecond = FMath::Clamp(RateLimitPerSecond, 1, 1000);
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

	GConfig->Flush(false, FilePath);
}

void FBobBotConfig::ApplyEnvironmentVars() const
{
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_MCP_PORT"), *FString::FromInt(Port));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_MCP_HOST"), *Host);
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_MCP_MAX_CLIENTS"), *FString::FromInt(MaxClients));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_MCP_RATE_LIMIT"), *FString::FromInt(RateLimitPerSecond));

	// Set project root for bob_chat.py
	FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_PROJECT_ROOT"), *ProjectRoot);
}
