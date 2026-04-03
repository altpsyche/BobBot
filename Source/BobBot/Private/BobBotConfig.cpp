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

	GConfig->GetString(ConfigSection, TEXT("ThinkingMode"), ThinkingMode, FilePath);
	GConfig->GetInt(ConfigSection, TEXT("ThinkingBudget"), ThinkingBudget, FilePath);
	ThinkingBudget = FMath::Clamp(ThinkingBudget, 1000, 100000);
	GConfig->GetString(ConfigSection, TEXT("EffortLevel"), EffortLevel, FilePath);

	GConfig->GetBool(ConfigSection, TEXT("bAutoApproveReadOnly"), bAutoApproveReadOnly, FilePath);
	GConfig->GetBool(ConfigSection, TEXT("bAutoApproveViewport"), bAutoApproveViewport, FilePath);
	GConfig->GetBool(ConfigSection, TEXT("bAutoApproveCreate"), bAutoApproveCreate, FilePath);
	GConfig->GetBool(ConfigSection, TEXT("bAutoApproveModify"), bAutoApproveModify, FilePath);
	GConfig->GetBool(ConfigSection, TEXT("bAutoApproveCodeExec"), bAutoApproveCodeExec, FilePath);
	GConfig->GetBool(ConfigSection, TEXT("bSetupComplete"), bSetupComplete, FilePath);

	// API key: read from OS keychain (Windows Credential Manager) first
	if (!FPlatformMisc::GetStoredValue(TEXT("BobBot"), TEXT("Auth"), TEXT("ApiKey"), ApiKey) || ApiKey.IsEmpty())
	{
		// Fall back to INI for migration from pre-1.5 versions
		FString IniKey;
		GConfig->GetString(ConfigSection, TEXT("ApiKey"), IniKey, FilePath);
		if (!IniKey.IsEmpty())
		{
			ApiKey = IniKey;
			// Migrate: store in keychain and remove from INI
			FPlatformMisc::SetStoredValue(TEXT("BobBot"), TEXT("Auth"), TEXT("ApiKey"), *ApiKey);
			GConfig->RemoveKey(ConfigSection, TEXT("ApiKey"), FilePath);
			GConfig->Flush(false, FilePath);
			UE_LOG(LogTemp, Log, TEXT("BobBot: Migrated API key from INI to OS keychain"));
		}
	}

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
	GConfig->SetString(ConfigSection, TEXT("ChatModel"), *ChatModel, FilePath);
	GConfig->SetString(ConfigSection, TEXT("SystemPrompt"), *SystemPrompt, FilePath);
	GConfig->SetInt(ConfigSection, TEXT("ChatTimeoutSeconds"), ChatTimeoutSeconds, FilePath);
	GConfig->SetInt(ConfigSection, TEXT("PermissionMode"), static_cast<int32>(PermissionMode), FilePath);
	GConfig->SetFloat(ConfigSection, TEXT("MaxBudgetUsd"), MaxBudgetUsd, FilePath);
	GConfig->SetString(ConfigSection, TEXT("ThinkingMode"), *ThinkingMode, FilePath);
	GConfig->SetInt(ConfigSection, TEXT("ThinkingBudget"), ThinkingBudget, FilePath);
	GConfig->SetString(ConfigSection, TEXT("EffortLevel"), *EffortLevel, FilePath);
	GConfig->SetBool(ConfigSection, TEXT("bAutoApproveReadOnly"), bAutoApproveReadOnly, FilePath);
	GConfig->SetBool(ConfigSection, TEXT("bAutoApproveViewport"), bAutoApproveViewport, FilePath);
	GConfig->SetBool(ConfigSection, TEXT("bAutoApproveCreate"), bAutoApproveCreate, FilePath);
	GConfig->SetBool(ConfigSection, TEXT("bAutoApproveModify"), bAutoApproveModify, FilePath);
	GConfig->SetBool(ConfigSection, TEXT("bAutoApproveCodeExec"), bAutoApproveCodeExec, FilePath);
	GConfig->SetBool(ConfigSection, TEXT("bSetupComplete"), bSetupComplete, FilePath);
	GConfig->SetInt(ConfigSection, TEXT("AuthMode"), static_cast<int32>(AuthMode), FilePath);

	// API key: store in OS keychain (Windows Credential Manager), not in plaintext INI
	if (!ApiKey.IsEmpty())
	{
		FPlatformMisc::SetStoredValue(TEXT("BobBot"), TEXT("Auth"), TEXT("ApiKey"), *ApiKey);
	}
	else
	{
		FPlatformMisc::DeleteStoredValue(TEXT("BobBot"), TEXT("Auth"), TEXT("ApiKey"));
	}
	// Ensure no plaintext key in INI (cleanup from migration or old versions)
	GConfig->RemoveKey(ConfigSection, TEXT("ApiKey"), FilePath);

	GConfig->SetString(ConfigSection, TEXT("ApiProvider"), *ApiProvider, FilePath);
	GConfig->SetString(ConfigSection, TEXT("ApiRegion"), *ApiRegion, FilePath);
	GConfig->SetString(ConfigSection, TEXT("ApiProjectId"), *ApiProjectId, FilePath);

	GConfig->Flush(false, FilePath);
}

const TCHAR* FBobBotConfig::PermissionModeToString(EBobBotPermissionMode Mode)
{
	switch (Mode)
	{
	case EBobBotPermissionMode::Plan:              return TEXT("plan");
	case EBobBotPermissionMode::AskBeforeEdits:    return TEXT("ask_before_edits");
	default:                                       return TEXT("edit_automatically");
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
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_PERMISSION_MODE"), PermissionModeToString(PermissionMode));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_CHAT_TIMEOUT"), *FString::FromInt(ChatTimeoutSeconds));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_MAX_BUDGET"), *FString::Printf(TEXT("%.2f"), MaxBudgetUsd));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_AUTO_APPROVE_READ_ONLY"), bAutoApproveReadOnly ? TEXT("1") : TEXT("0"));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_AUTO_APPROVE_VIEWPORT"), bAutoApproveViewport ? TEXT("1") : TEXT("0"));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_AUTO_APPROVE_CREATE"), bAutoApproveCreate ? TEXT("1") : TEXT("0"));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_AUTO_APPROVE_MODIFY"), bAutoApproveModify ? TEXT("1") : TEXT("0"));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_AUTO_APPROVE_CODE_EXEC"), bAutoApproveCodeExec ? TEXT("1") : TEXT("0"));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_THINKING_MODE"), *ThinkingMode);
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_THINKING_BUDGET"), *FString::FromInt(ThinkingBudget));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_EFFORT"), *EffortLevel);
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_AUTH_MODE"), AuthMode == EBobBotAuthMode::ApiKey ? TEXT("api_key") : TEXT("subscription"));
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_API_KEY"), *ApiKey);
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_API_PROVIDER"), *ApiProvider);
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_API_REGION"), *ApiRegion);
	FPlatformMisc::SetEnvironmentVar(TEXT("BOB_API_PROJECT_ID"), *ApiProjectId);
}
