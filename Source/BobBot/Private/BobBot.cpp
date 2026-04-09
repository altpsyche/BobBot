// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "BobBot.h"
#include "BobBotStyle.h"
#include "BobBotCommands.h"
#include "BobBotConstants.h"
#include "BobBotConfig.h"
#include "BobBotRuntimeStatus.h"
#include "BobBotPythonBridge.h"
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
	// 1. Load config + generate per-launch bridge auth token
	FBobBotConfig::Get().Load();
	FBobBotConfig::Get().RegenerateBridgeAuthToken();

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

	// 4. Create project tools directory if it doesn't exist
	EnsureProjectToolsDir();

	// 5. Auto-start Python server
	if (FBobBotConfig::Get().bAutoStartServer)
	{
		AutoStartPythonServer();
	}

	// 6. Auto-start HTTP bridge + SDK import (only if setup is complete — otherwise Welcome tab handles it)
	if (FBobBotConfig::Get().bSetupComplete)
	{
		if (FBobBotConfig::Get().bAutoStartBridge)
		{
			AutoStartHttpBridge();
		}

		// Import bob_chat_sdk in background (loads SDK from existing venv)
		FBobBotPythonBridge& Bridge2 = FBobBotPythonBridge::Get();
		if (Bridge2.IsAvailable())
		{
			Bridge2.ExecPythonCommand(
				TEXT("import threading\n")
				TEXT("def _bobbot_sdk_setup():\n")
				TEXT("    try: import bob_chat_sdk\n")
				TEXT("    except Exception as e:\n")
				TEXT("        try:\n")
				TEXT("            import unreal; unreal.log_warning('BobBot SDK setup failed: {}'.format(e))\n")
				TEXT("        except: pass\n")
				TEXT("threading.Thread(target=_bobbot_sdk_setup, daemon=True).start()\n")
			);
			UE_LOG(LogBobBot, Log, TEXT("claude-agent-sdk setup started (background)"));
		}
	}

	// 8. Generate .mcp.json (after bridge is up, so we can use HTTP transport if available)
	if (FBobBotConfig::Get().bAutoGenerateMcpJson)
	{
		EnsureMcpJson();
	}

	UE_LOG(LogBobBot, Log, TEXT("BobBot ready: server on :%d, bridge on :%d, .mcp.json %s"),
		FBobBotConfig::Get().Port,
		FBobBotConfig::Get().BridgePort,
		FBobBotConfig::Get().bAutoGenerateMcpJson ? TEXT("up to date") : TEXT("skipped"));
}

void FBobBotModule::ShutdownModule()
{
	// Stop HTTP bridge before unregistering UI
	if (!IsEngineExitRequested())
	{
		FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
		if (Bridge.IsAvailable())
		{
			Bridge.ExecPythonCommand(TEXT("import bob_bridge_launcher; bob_bridge_launcher.stop()"));
		}
	}

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FBobBotStyle::Shutdown();
	FBobBotCommands::Unregister();
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BobBotTabName);
}

TSharedRef<SDockTab> FBobBotModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<SBobBotPanel> Panel = SNew(SBobBotPanel);
	TSharedRef<SDockTab> Tab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			Panel
		];

	Tab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda(
		[Panel](TSharedRef<SDockTab>)
		{
			Panel->Shutdown();
		}
	));

	return Tab;
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
		Section.AddMenuEntryWithCommandList(FBobBotCommands::Get().OpenPluginWindow, PluginCommands,
			TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), NAME_None, FName("BobBot_OpenWindow"));
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
	FBobBotRuntimeStatus& Status = FBobBotRuntimeStatus::Get();

	// Check PythonScriptPlugin (the only real prerequisite — everything else uses UE's bundled Python)
	{
		Status.bPythonPluginAvailable = IPythonScriptPlugin::Get() != nullptr;
		if (Status.bPythonPluginAvailable)
		{
			UE_LOG(LogBobBot, Log, TEXT("PythonScriptPlugin available"));
		}
		else
		{
			UE_LOG(LogBobBot, Warning, TEXT("PythonScriptPlugin not available. Enable it in Edit > Plugins."));
		}
	}

	// claude-agent-sdk check is deferred — runs after UE Python is available (step 7)
	// so we can verify the package is actually importable, not just installed on system pip.
	Status.bAgentSDKAvailable = false;
	Status.AgentSDKVersion = TEXT("");
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

FString FBobBotModule::GenerateClientConfig(const FString& ClientName, bool bForceStdio) const
{
	const FBobBotConfig& Config = FBobBotConfig::Get();

	TSharedPtr<FJsonObject> ServerEntry = MakeShareable(new FJsonObject);

	// Use HTTP transport when bridge is configured to auto-start (it will be up shortly)
	// or when it's already running. VS Code Copilot uses its own MCP, so exclude it.
	bool bUseHttp = !bForceStdio
		&& (FBobBotRuntimeStatus::Get().bBridgeRunning || Config.bAutoStartBridge)
		&& ClientName != TEXT("vscode");

	if (bUseHttp)
	{
		// HTTP transport: point to the already-running persistent bridge.
		// Token authentication: the bridge's MCP endpoint lives at
		// /mcp/<token> instead of /mcp. Anyone who doesn't know the path
		// gets Starlette's default 404. We encode the token in the URL path
		// rather than a custom header because the MCP client library
		// deprecated the `headers` field (silently ignored since mcp 1.x).
		FString PathSuffix = Config.BridgeAuthToken.IsEmpty()
			? TEXT("/mcp")
			: FString::Printf(TEXT("/mcp/%s"), *Config.BridgeAuthToken);
		FString BridgeUrl = FString::Printf(TEXT("http://127.0.0.1:%d%s"),
			Config.BridgePort, *PathSuffix);
		ServerEntry->SetStringField(TEXT("type"), TEXT("http"));
		ServerEntry->SetStringField(TEXT("url"), BridgeUrl);
	}
	else
	{
		// Stdio transport: spawn per-invocation using venv Python (fallback, or for VS Code).
		// The bridge subprocess inherits BOB_BRIDGE_TOKEN via env so it can auth to the TCP server.
		FString BridgePath = GetBridgeScriptPath();
		BridgePath.ReplaceInline(TEXT("\\"), TEXT("/"));

#if PLATFORM_WINDOWS
		FString VenvPython = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectSavedDir() / TEXT("BobBot") / TEXT(".venv") / TEXT("Scripts") / TEXT("python.exe"));
#else
		FString VenvPython = FPaths::ConvertRelativePathToFull(
			FPaths::ProjectSavedDir() / TEXT("BobBot") / TEXT(".venv") / TEXT("bin") / TEXT("python"));
#endif
		VenvPython.ReplaceInline(TEXT("\\"), TEXT("/"));

		ServerEntry->SetStringField(TEXT("command"), VenvPython);

		TArray<TSharedPtr<FJsonValue>> Args;
		Args.Add(MakeShareable(new FJsonValueString(BridgePath)));
		ServerEntry->SetArrayField(TEXT("args"), Args);

		TSharedPtr<FJsonObject> EnvObj = MakeShareable(new FJsonObject);
		EnvObj->SetStringField(TEXT("BOB_MCP_PORT"), FString::FromInt(Config.Port));
		if (!Config.BridgeAuthToken.IsEmpty())
		{
			EnvObj->SetStringField(TEXT("BOB_BRIDGE_TOKEN"), Config.BridgeAuthToken);
		}
		ServerEntry->SetObjectField(TEXT("env"), EnvObj);
	}

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

	// Write BobBot-specific HTTP config for session isolation from VS Code
	FString BobBotMcpPath = FPaths::ProjectSavedDir() / BobBot::SavedSubDir / TEXT("_bobbot_mcp.json");
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	PF.CreateDirectoryTree(*FPaths::GetPath(BobBotMcpPath));
	FFileHelper::SaveStringToFile(DesiredContent, *BobBotMcpPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	// Always write a stdio fallback config (used when HTTP bridge is down)
	FString FallbackContent = GenerateClientConfig(TEXT("claude"), /*bForceStdio=*/ true);

	FString FallbackPath = FPaths::ProjectSavedDir() / BobBot::SavedSubDir / TEXT("_bobbot_mcp_fallback.json");
	FFileHelper::SaveStringToFile(FallbackContent, *FallbackPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

// --------------------------------------------------------------------------- //
// Python Server Auto-Start
// --------------------------------------------------------------------------- //

void FBobBotModule::AutoStartPythonServer()
{
	if (!FBobBotRuntimeStatus::Get().bPythonPluginAvailable)
	{
		UE_LOG(LogBobBot, Warning, TEXT("Cannot auto-start server: PythonScriptPlugin not available. Start manually from the BobBot panel."));
		return;
	}

	// Set env vars before importing the server module
	FBobBotConfig::Get().ApplyEnvironmentVars();

	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (Bridge.IsAvailable())
	{
		// Sync env vars into Python's os.environ (FPlatformMisc::SetEnvironmentVar
		// updates the OS env but not Python's cached os.environ dict).
		Bridge.ExecPythonCommand(BobBot::Scripts::EnvSync);
		Bridge.ExecPythonCommand(TEXT("import bob_mcp_server"));
		UE_LOG(LogBobBot, Log, TEXT("Python server auto-started"));
	}
}

// --------------------------------------------------------------------------- //
// HTTP Bridge Auto-Start
// --------------------------------------------------------------------------- //

void FBobBotModule::AutoStartHttpBridge()
{
	if (!FBobBotRuntimeStatus::Get().bPythonPluginAvailable)
	{
		UE_LOG(LogBobBot, Warning, TEXT("Cannot auto-start HTTP bridge: PythonScriptPlugin not available."));
		return;
	}

	FBobBotPythonBridge& Bridge = FBobBotPythonBridge::Get();
	if (Bridge.IsAvailable())
	{
		Bridge.ExecPythonCommand(BobBot::Scripts::EnvSync);
		// Start bridge in background thread to avoid blocking editor startup.
		Bridge.ExecPythonCommand(TEXT("import bob_bridge_launcher, threading\n")
			TEXT("threading.Thread(target=bob_bridge_launcher.start, daemon=True).start()\n"));
		UE_LOG(LogBobBot, Log, TEXT("HTTP bridge auto-start requested (background)"));
	}
	else
	{
		UE_LOG(LogBobBot, Warning, TEXT("Cannot auto-start HTTP bridge: Python bridge not available."));
	}
}

// --------------------------------------------------------------------------- //
// Project Tools Directory
// --------------------------------------------------------------------------- //

void FBobBotModule::EnsureProjectToolsDir()
{
	FString ToolsDir = FPaths::ProjectDir() / TEXT("BobBot") / TEXT("tools");
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();

	if (PF.DirectoryExists(*ToolsDir))
	{
		return;
	}

	PF.CreateDirectoryTree(*ToolsDir);

	// Write __init__.py so the directory is importable as a Python package
	FString InitPath = ToolsDir / TEXT("__init__.py");
	FFileHelper::SaveStringToFile(TEXT(""), *InitPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	// Write example template (starts with _ so the bridge skips it)
	FString ExampleContent = FString(
		TEXT("\"\"\"")		LINE_TERMINATOR
		TEXT("Example BobBot MCP tool.")		LINE_TERMINATOR
		TEXT("Rename this file (remove the leading underscore) and modify register()")		LINE_TERMINATOR
		TEXT("to add your own tools.")		LINE_TERMINATOR
		TEXT("")		LINE_TERMINATOR
		TEXT("Tools placed in <ProjectRoot>/BobBot/tools/ are auto-discovered by BobBot.")		LINE_TERMINATOR
		TEXT("Each file needs a register(mcp, send_fn) function.")		LINE_TERMINATOR
		TEXT("\"\"\"")		LINE_TERMINATOR
		TEXT("")		LINE_TERMINATOR
		TEXT("")		LINE_TERMINATOR
		TEXT("def register(mcp, send_fn):")		LINE_TERMINATOR
		TEXT("")		LINE_TERMINATOR
		TEXT("    @mcp.tool()")		LINE_TERMINATOR
		TEXT("    def my_custom_tool(param: str) -> str:")		LINE_TERMINATOR
		TEXT("        \"\"\"Description that Claude sees when choosing tools.\"\"\"")		LINE_TERMINATOR
		TEXT("        result = send_fn({\"type\": \"execute\", \"code\": f\"print('Hello from {param}')\"})")		LINE_TERMINATOR
		TEXT("        if result.get(\"success\"):")		LINE_TERMINATOR
		TEXT("            return result.get(\"output\", \"(no output)\")")		LINE_TERMINATOR
		TEXT("        return \"Error: \" + result.get(\"error\", \"unknown\")")		LINE_TERMINATOR
	);

	FString ExamplePath = ToolsDir / TEXT("_example.py");
	FFileHelper::SaveStringToFile(ExampleContent, *ExamplePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	UE_LOG(LogBobBot, Log, TEXT("Created project tools directory: %s"), *ToolsDir);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBobBotModule, BobBot)
