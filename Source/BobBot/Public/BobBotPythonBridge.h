// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"

class FJsonObject;

/**
 * Centralizes all C++ → Python communication via IPythonScriptPlugin.
 * Provides reusable patterns for the file-based IPC (write script → exec → read JSON → cleanup).
 */
class FBobBotPythonBridge
{
public:
	static FBobBotPythonBridge& Get();

	/**
	 * Execute a Python script that writes JSON to a temp file, then parse and return the result.
	 * The script receives an `output_path` Python variable pointing to the temp file.
	 * Returns nullptr on failure (plugin unavailable, exec failed, or parse error).
	 */
	TSharedPtr<FJsonObject> ExecPythonWithJsonResult(const FString& ScriptBody, const FString& TempFileName);

	/** Execute a Python command with no result needed. Returns false on failure. */
	bool ExecPythonCommand(const FString& Script);

	/**
	 * Convenience: import a module and call a function with a single string argument.
	 * Generates: import {Module}; {Module}.{Function}('{Arg}')
	 * The Arg is validated to contain only safe characters (alphanumeric, underscore, dash, dot).
	 */
	bool ExecCallWithString(const FString& Module, const FString& Function, const FString& Arg);

	/** Get the temp directory path (Saved/BobBot/), creating it if needed. */
	FString GetTempDir() const;

	/** Check if PythonScriptPlugin is available. */
	bool IsAvailable() const;

private:
	FBobBotPythonBridge() = default;
};
