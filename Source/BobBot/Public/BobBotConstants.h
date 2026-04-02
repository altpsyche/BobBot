// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

/**
 * Centralized constants for the BobBot plugin.
 * Colors, temp file names, poll intervals, UI helpers, and Python script fragments.
 */
namespace BobBot
{

// =========================================================================== //
// Colors
// =========================================================================== //
namespace Colors
{
	inline const FLinearColor Green(0.2f, 0.8f, 0.2f);
	inline const FLinearColor Yellow(0.9f, 0.8f, 0.1f);
	inline const FLinearColor Red(0.9f, 0.2f, 0.2f);
	inline const FLinearColor DimGray(0.5f, 0.5f, 0.5f);
	inline const FLinearColor LightGray(0.6f, 0.6f, 0.6f);
	inline const FLinearColor Blue(0.3f, 0.6f, 1.0f);
	inline const FLinearColor BotGreen(0.3f, 0.9f, 0.3f);
	inline const FLinearColor ErrorOrange(1.0f, 0.5f, 0.1f);
	inline const FLinearColor ActiveBlue(0.1f, 0.5f, 0.9f, 1.f);
	inline const FLinearColor InactiveDark(0.15f, 0.15f, 0.15f, 1.f);
	inline const FLinearColor SectionBg(0.02f, 0.02f, 0.02f, 0.5f);
	inline const FLinearColor CodeBlockBg(0.08f, 0.08f, 0.08f, 1.f);
	inline const FLinearColor CodeText(0.85f, 0.9f, 0.85f);
}

// =========================================================================== //
// Temp file names (all live under Saved/BobBot/)
// =========================================================================== //
namespace TempFiles
{
	inline const TCHAR* ClaudeDetect     = TEXT("_claude_detect.txt");
	inline const TCHAR* StatusOutput     = TEXT("_status_output.txt");
	inline const TCHAR* ChatPoll         = TEXT("_chat_poll.txt");
	inline const TCHAR* ChatMessage      = TEXT("_chat_msg.txt");
	inline const TCHAR* ChatHistory      = TEXT("_chat_history.json");
	inline const TCHAR* SystemPrompt     = TEXT("_system_prompt.txt");
	inline const TCHAR* BobBotMcpConfig  = TEXT("_bobbot_mcp.json");
	inline const TCHAR* WelcomeStep      = TEXT("_welcome_step.json");
}

// =========================================================================== //
// Subdirectory name under ProjectSavedDir
// =========================================================================== //
inline const TCHAR* SavedSubDir = TEXT("BobBot");

// =========================================================================== //
// Poll intervals (seconds)
// =========================================================================== //
namespace Polling
{
	constexpr float ServerStatus = 2.0f;
	constexpr float ChatActive   = 0.05f;
	constexpr float ChatIdle     = 1.0f;
	constexpr float ThinkingAnim = 0.5f;
}

// =========================================================================== //
// Python script fragments (used by C++ ExecPythonCommand)
// =========================================================================== //
namespace Scripts
{
	/** Sync OS environment variables into Python's os.environ dict.
	 *  Uses ctypes to read from the OS env block directly, avoiding
	 *  string interpolation of config values into Python source. */
	inline const TCHAR* EnvSync =
		TEXT("import os, ctypes, ctypes.wintypes\n")
		TEXT("_gev = ctypes.windll.kernel32.GetEnvironmentVariableW\n")
		TEXT("_gev.argtypes = [ctypes.wintypes.LPCWSTR, ctypes.wintypes.LPWSTR, ctypes.wintypes.DWORD]\n")
		TEXT("_gev.restype = ctypes.wintypes.DWORD\n")
		TEXT("def _env(k):\n")
		TEXT("    buf = ctypes.create_unicode_buffer(1024)\n")
		TEXT("    n = _gev(k, buf, 1024)\n")
		TEXT("    return buf.value if n else os.environ.get(k, '')\n")
		TEXT("for _k in ['BOB_MCP_PORT','BOB_MCP_HOST','BOB_MCP_MAX_CLIENTS','BOB_MCP_RATE_LIMIT','BOB_PROJECT_ROOT','BOB_PERMISSION_MODE','BOB_CHAT_TIMEOUT','BOB_MCP_BRIDGE_PORT','BOB_USE_SDK','BOB_MAX_BUDGET','BOB_AUTH_MODE','BOB_API_KEY','BOB_API_PROVIDER','BOB_API_REGION','BOB_API_PROJECT_ID','BOB_AUTO_APPROVE_READ_ONLY','BOB_AUTO_APPROVE_VIEWPORT','BOB_AUTO_APPROVE_CREATE','BOB_AUTO_APPROVE_MODIFY','BOB_AUTO_APPROVE_CODE_EXEC']:\n")
		TEXT("    os.environ[_k] = _env(_k)\n")
		TEXT("del _gev, _env, _k\n");
}

// =========================================================================== //
// UI Helpers
// =========================================================================== //
namespace UI
{
	/** Make a section heading label. */
	inline TSharedRef<SWidget> SectionHeading(const FText& Label)
	{
		return SNew(STextBlock).Text(Label)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			.ColorAndOpacity(FSlateColor(Colors::LightGray));
	}

	/** Make a key-value row: "Label    Value" with optional color. */
	inline TSharedRef<SWidget> KeyValueRow(const FText& Key, TAttribute<FText> Value, TAttribute<FSlateColor> ValueColor = FSlateColor(FLinearColor::White))
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 12, 0)
			[
				SNew(STextBlock).Text(Key)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.ColorAndOpacity(FSlateColor(Colors::DimGray))
				.MinDesiredWidth(110.f)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(STextBlock).Text(Value)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.ColorAndOpacity(ValueColor)
			];
	}
}

} // namespace BobBot
