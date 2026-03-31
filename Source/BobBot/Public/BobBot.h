// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBobBot, Log, All);

class FToolBarBuilder;
class FMenuBuilder;

class FBobBotModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void PluginButtonClicked();

	/** Generate MCP config JSON for a specific AI client. */
	FString GenerateClientConfig(const FString& ClientName, bool bForceStdio = false) const;

	/** Write MCP config to the appropriate location for a client. Returns true on success. */
	bool WriteClientConfig(const FString& ClientName, const FString& TargetPath) const;

private:
	void RegisterMenus();
	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

	/** Check for uv, Python, PythonScriptPlugin availability. */
	void CheckPrerequisites();

	/** Generate or update .mcp.json at project root. */
	void EnsureMcpJson();

	/** Start the Python TCP server inside UE. */
	void AutoStartPythonServer();

	/** Start the persistent HTTP MCP bridge. */
	void AutoStartHttpBridge();

	/** Create <ProjectRoot>/BobBot/tools/ with template if it doesn't exist. */
	void EnsureProjectToolsDir();

	/** Get the path to the bridge script. */
	FString GetBridgeScriptPath() const;

private:
	TSharedPtr<class FUICommandList> PluginCommands;
};
