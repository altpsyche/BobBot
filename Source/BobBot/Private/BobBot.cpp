// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "BobBot.h"
#include "BobBotStyle.h"
#include "BobBotCommands.h"
#include "BobBotConfig.h"
#include "Widgets/SBobBotPanel.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "Interfaces/IPluginManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "IPythonScriptPlugin.h"

DEFINE_LOG_CATEGORY(LogBobBot);

static const FName BobBotTabName("BobBot");

#define LOCTEXT_NAMESPACE "FBobBotModule"

void FBobBotModule::StartupModule()
{
	// 1. Load config
	FBobBotConfig::Get().Load();

	// 2. Register UI
	FBobBotStyle::Initialize();
	FBobBotStyle::ReloadTextures();
	FBobBotCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FBobBotCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FBobBotModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBobBotModule::RegisterMenus));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(BobBotTabName, FOnSpawnTab::CreateRaw(this, &FBobBotModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FBobBotTabTitle", "BobBot"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	// 3. Check prerequisites
	CheckPrerequisites();

	// 4. Generate .mcp.json
	if (FBobBotConfig::Get().bAutoGenerateMcpJson)
	{
		EnsureMcpJson();
	}

	// 5. Auto-start Python server
	if (FBobBotConfig::Get().bAutoStartServer)
	{
		AutoStartPythonServer();
	}

	UE_LOG(LogBobBot, Log, TEXT("BobBot ready: server on :%d, .mcp.json %s"),
		FBobBotConfig::Get().Port,
		FBobBotConfig::Get().bAutoGenerateMcpJson ? TEXT("up to date") : TEXT("skipped"));
}

void FBobBotModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FBobBotStyle::Shutdown();
	FBobBotCommands::Unregister();
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BobBotTabName);
}

TSharedRef<SDockTab> FBobBotModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SBobBotPanel)
		];
}

void FBobBotModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(BobBotTabName);
}

void FBobBotModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
		Section.AddMenuEntryWithCommandList(FBobBotCommands::Get().OpenPluginWindow, PluginCommands);
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
		FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FBobBotCommands::Get().OpenPluginWindow));
		Entry.SetCommandList(PluginCommands);
	}
}

// --------------------------------------------------------------------------- //
// Prerequisites
// --------------------------------------------------------------------------- //

void FBobBotModule::CheckPrerequisites()
{
	FBobBotConfig& Config = FBobBotConfig::Get();

	// Check uv
	{
		FString StdOut, StdErr;
		int32 ReturnCode = -1;
		FPlatformProcess::ExecProcess(TEXT("uv"), TEXT("--version"), &ReturnCode, &StdOut, &StdErr);
		Config.bUvAvailable = (ReturnCode == 0);
		Config.UvVersion = StdOut.TrimStartAndEnd();
		if (Config.bUvAvailable)
		{
			UE_LOG(LogBobBot, Log, TEXT("uv found: %s"), *Config.UvVersion);
		}
		else
		{
			UE_LOG(LogBobBot, Warning, TEXT("uv not found. Install from https://docs.astral.sh/uv/"));
		}
	}

	// Check Python
	{
		FString StdOut, StdErr;
		int32 ReturnCode = -1;
		FPlatformProcess::ExecProcess(TEXT("python"), TEXT("--version"), &ReturnCode, &StdOut, &StdErr);
		Config.bPythonAvailable = (ReturnCode == 0);
		Config.PythonVersion = StdOut.TrimStartAndEnd();
		if (Config.bPythonAvailable)
		{
			UE_LOG(LogBobBot, Log, TEXT("Python found: %s"), *Config.PythonVersion);
		}
		else
		{
			UE_LOG(LogBobBot, Warning, TEXT("Python not found on PATH"));
		}
	}

	// Check PythonScriptPlugin
	{
		Config.bPythonPluginAvailable = IPythonScriptPlugin::Get() != nullptr;
		if (Config.bPythonPluginAvailable)
		{
			UE_LOG(LogBobBot, Log, TEXT("PythonScriptPlugin available"));
		}
		else
		{
			UE_LOG(LogBobBot, Warning, TEXT("PythonScriptPlugin not available. Enable it in Edit > Plugins."));
		}
	}
}

// --------------------------------------------------------------------------- //
// MCP JSON Generation
// --------------------------------------------------------------------------- //

FString FBobBotModule::GetBridgeScriptPath() const
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BobBot"));
	if (!Plugin.IsValid())
	{
		return FString();
	}

	FString BridgePath = Plugin->GetBaseDir() / TEXT("Scripts") / TEXT("bob_mcp_bridge.py");
	return FPaths::ConvertRelativePathToFull(BridgePath);
}

FString FBobBotModule::GenerateClientConfig(const FString& ClientName) const
{
	const FBobBotConfig& Config = FBobBotConfig::Get();
	FString BridgePath = GetBridgeScriptPath();
	// Use forward slashes for JSON
	BridgePath.ReplaceInline(TEXT("\\"), TEXT("/"));

	// Build the server entry
	TSharedPtr<FJsonObject> ServerEntry = MakeShareable(new FJsonObject);
	ServerEntry->SetStringField(TEXT("command"), TEXT("uv"));

	TArray<TSharedPtr<FJsonValue>> Args;
	Args.Add(MakeShareable(new FJsonValueString(TEXT("run"))));
	Args.Add(MakeShareable(new FJsonValueString(TEXT("--with"))));
	Args.Add(MakeShareable(new FJsonValueString(TEXT("mcp[cli]"))));
	Args.Add(MakeShareable(new FJsonValueString(BridgePath)));
	ServerEntry->SetArrayField(TEXT("args"), Args);

	TSharedPtr<FJsonObject> EnvObj = MakeShareable(new FJsonObject);
	EnvObj->SetStringField(TEXT("BOB_MCP_PORT"), FString::FromInt(Config.Port));
	ServerEntry->SetObjectField(TEXT("env"), EnvObj);

	// Build the root object (format differs per client)
	TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject);

	if (ClientName == TEXT("vscode"))
	{
		// VS Code Copilot uses "servers" key
		TSharedPtr<FJsonObject> Servers = MakeShareable(new FJsonObject);
		Servers->SetObjectField(TEXT("unreal"), ServerEntry);
		Root->SetObjectField(TEXT("servers"), Servers);
	}
	else
	{
		// Claude Code, Cursor, Windsurf all use "mcpServers"
		TSharedPtr<FJsonObject> McpServers = MakeShareable(new FJsonObject);
		McpServers->SetObjectField(TEXT("unreal"), ServerEntry);
		Root->SetObjectField(TEXT("mcpServers"), McpServers);
	}

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	return Output;
}

bool FBobBotModule::WriteClientConfig(const FString& ClientName, const FString& TargetPath) const
{
	FString JsonContent = GenerateClientConfig(ClientName);
	if (JsonContent.IsEmpty())
	{
		return false;
	}

	// If writing to an existing file that may have other entries, merge
	FString ExistingContent;
	if (FFileHelper::LoadFileToString(ExistingContent, *TargetPath) && !ExistingContent.IsEmpty())
	{
		TSharedPtr<FJsonObject> ExistingRoot;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ExistingContent);
		if (FJsonSerializer::Deserialize(Reader, ExistingRoot) && ExistingRoot.IsValid())
		{
			// Parse our new content
			TSharedPtr<FJsonObject> NewRoot;
			TSharedRef<TJsonReader<>> NewReader = TJsonReaderFactory<>::Create(JsonContent);
			if (FJsonSerializer::Deserialize(NewReader, NewRoot) && NewRoot.IsValid())
			{
				// Merge: add/overwrite the "unreal" server entry in existing file
				FString ServersKey = (ClientName == TEXT("vscode")) ? TEXT("servers") : TEXT("mcpServers");

				// Get or create the servers container in existing file
				TSharedPtr<FJsonObject> ExistingServers = ExistingRoot->GetObjectField(ServersKey);
				if (!ExistingServers.IsValid())
				{
					ExistingServers = MakeShareable(new FJsonObject);
				}

				// Get our "unreal" entry from the new config
				TSharedPtr<FJsonObject> NewServers = NewRoot->GetObjectField(ServersKey);
				if (NewServers.IsValid())
				{
					TSharedPtr<FJsonObject> UnrealEntry = NewServers->GetObjectField(TEXT("unreal"));
					if (UnrealEntry.IsValid())
					{
						ExistingServers->SetObjectField(TEXT("unreal"), UnrealEntry);
					}
				}

				ExistingRoot->SetObjectField(ServersKey, ExistingServers);

				// Re-serialize
				JsonContent.Empty();
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonContent);
				FJsonSerializer::Serialize(ExistingRoot.ToSharedRef(), Writer);
			}
		}
	}

	return FFileHelper::SaveStringToFile(JsonContent, *TargetPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void FBobBotModule::EnsureMcpJson()
{
	FString McpJsonPath = FPaths::ProjectDir() / TEXT(".mcp.json");
	FString DesiredContent = GenerateClientConfig(TEXT("claude"));

	// Only write if content differs
	FString ExistingContent;
	if (FFileHelper::LoadFileToString(ExistingContent, *McpJsonPath))
	{
		if (ExistingContent.TrimStartAndEnd() == DesiredContent.TrimStartAndEnd())
		{
			return;
		}
	}

	if (FFileHelper::SaveStringToFile(DesiredContent, *McpJsonPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogBobBot, Log, TEXT(".mcp.json written to %s"), *McpJsonPath);
	}
	else
	{
		UE_LOG(LogBobBot, Warning, TEXT("Failed to write .mcp.json to %s"), *McpJsonPath);
	}
}

// --------------------------------------------------------------------------- //
// Python Server Auto-Start
// --------------------------------------------------------------------------- //

void FBobBotModule::AutoStartPythonServer()
{
	if (!FBobBotConfig::Get().bPythonPluginAvailable)
	{
		UE_LOG(LogBobBot, Warning, TEXT("Cannot auto-start server: PythonScriptPlugin not available. Start manually from the BobBot panel."));
		return;
	}

	// Set env vars before importing the server module
	FBobBotConfig::Get().ApplyEnvironmentVars();

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (PythonPlugin)
	{
		PythonPlugin->ExecPythonCommand(TEXT("import bob_mcp_server"));
		UE_LOG(LogBobBot, Log, TEXT("Python server auto-started"));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBobBotModule, BobBot)
