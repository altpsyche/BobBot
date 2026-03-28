// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "BobBotPythonBridge.h"
#include "BobBot.h"
#include "BobBotConstants.h"
#include "IPythonScriptPlugin.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FBobBotPythonBridge& FBobBotPythonBridge::Get()
{
	static FBobBotPythonBridge Instance;
	return Instance;
}

bool FBobBotPythonBridge::IsAvailable() const
{
	return IPythonScriptPlugin::Get() != nullptr;
}

FString FBobBotPythonBridge::GetTempDir() const
{
	FString Dir = FPaths::ProjectSavedDir() / BobBot::SavedSubDir;
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	PF.CreateDirectoryTree(*Dir);
	return Dir;
}

bool FBobBotPythonBridge::ExecPythonCommand(const FString& Script)
{
	IPythonScriptPlugin* Plugin = IPythonScriptPlugin::Get();
	if (!Plugin)
	{
		UE_LOG(LogBobBot, Warning, TEXT("BobBotPythonBridge: PythonScriptPlugin not available"));
		return false;
	}

	return Plugin->ExecPythonCommand(*Script);
}

bool FBobBotPythonBridge::ExecCallWithString(const FString& Module, const FString& Function, const FString& Arg)
{
	// Validate arg contains only safe characters to prevent injection
	for (TCHAR Ch : Arg)
	{
		if (!FChar::IsAlnum(Ch) && Ch != TEXT('_') && Ch != TEXT('-') && Ch != TEXT('.'))
		{
			UE_LOG(LogBobBot, Warning, TEXT("BobBotPythonBridge: Unsafe character '%c' in arg '%s'"), Ch, *Arg);
			return false;
		}
	}

	FString Script = FString::Printf(TEXT("import %s; %s.%s('%s')"), *Module, *Module, *Function, *Arg);
	return ExecPythonCommand(Script);
}

TSharedPtr<FJsonObject> FBobBotPythonBridge::ExecPythonWithJsonResult(const FString& ScriptBody, const FString& TempFileName)
{
	IPythonScriptPlugin* Plugin = IPythonScriptPlugin::Get();
	if (!Plugin)
	{
		return nullptr;
	}

	FString TempDir = GetTempDir();
	FString OutputFile = TempDir / TempFileName;
	FString OutputFileFwd = OutputFile.Replace(TEXT("\\"), TEXT("/"));

	// Inject output_path variable and execute
	FString FullScript = FString::Printf(TEXT("output_path = r'%s'\n%s"), *OutputFileFwd, *ScriptBody);
	Plugin->ExecPythonCommand(*FullScript);

	// Read and parse result
	TSharedPtr<FJsonObject> Result;
	FString JsonStr;
	if (FFileHelper::LoadFileToString(JsonStr, *OutputFile))
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
		FJsonSerializer::Deserialize(Reader, Result);
	}

	// Cleanup
	FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*OutputFile);

	return Result;
}
