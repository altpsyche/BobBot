// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SCheckBox.h"
#include "BobBotTheme.h"
#include "BobBotConfig.h"

/**
 * Centralized constants for the BobBot plugin.
 * Colors, temp file names, poll intervals, UI helpers, and Python script fragments.
 */
namespace BobBot
{

// =========================================================================== //
// Colors — aliases to Theme (backward compatible)
// =========================================================================== //
namespace Colors
{
	inline const auto& Green       = Theme::Success;
	inline const auto& Yellow      = Theme::Warning;
	inline const auto& Red         = Theme::Error;
	inline const auto& DimGray     = Theme::TextSecondary;
	inline const auto& LightGray   = Theme::TextSecondary;
	inline const auto& Blue        = Theme::Info;
	inline const auto& BotGreen    = Theme::BotAccent;
	inline const auto& ErrorOrange = Theme::ErrorAccent;
	inline const auto& ActiveBlue  = Theme::ActiveTab;
	inline const auto& InactiveDark= Theme::InactiveTab;
	inline const auto& SectionBg   = Theme::Base;
	inline const auto& CodeBlockBg = Theme::Overlay;
	inline const auto& CodeText    = Theme::TextCode;
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
	 *  FPlatformMisc::SetEnvironmentVar updates the C runtime environment, but
	 *  Python's os.environ is a snapshot taken at interpreter startup and does
	 *  NOT reflect setenv() calls made afterward. So on every platform we must
	 *  read the *live* C environment, not os.environ:
	 *    - Windows: GetEnvironmentVariableW (ctypes).
	 *    - macOS/Linux: libc getenv (ctypes). Reading os.environ here would copy
	 *      stale/empty values, which is exactly what left the bridge subprocess
	 *      with an empty BOB_BRIDGE_TOKEN (open /mcp vs token-gated config -> 404). */
	inline const TCHAR* EnvSync =
		TEXT("import os, sys\n")
		TEXT("if sys.platform == 'win32':\n")
		TEXT("    import ctypes, ctypes.wintypes\n")
		TEXT("    _gev = ctypes.windll.kernel32.GetEnvironmentVariableW\n")
		TEXT("    _gev.argtypes = [ctypes.wintypes.LPCWSTR, ctypes.wintypes.LPWSTR, ctypes.wintypes.DWORD]\n")
		TEXT("    _gev.restype = ctypes.wintypes.DWORD\n")
		TEXT("    def _env(k):\n")
		TEXT("        buf = ctypes.create_unicode_buffer(1024)\n")
		TEXT("        n = _gev(k, buf, 1024)\n")
		TEXT("        return buf.value if n else os.environ.get(k, '')\n")
		TEXT("else:\n")
		TEXT("    import ctypes\n")
		TEXT("    try:\n")
		TEXT("        _libc = ctypes.CDLL(None)\n")
		TEXT("        _libc.getenv.restype = ctypes.c_char_p\n")
		TEXT("        _libc.getenv.argtypes = [ctypes.c_char_p]\n")
		TEXT("        def _env(k):\n")
		TEXT("            _v = _libc.getenv(k.encode('utf-8'))\n")
		TEXT("            return _v.decode('utf-8', 'replace') if _v is not None else os.environ.get(k, '')\n")
		TEXT("    except Exception:\n")
		TEXT("        def _env(k): return os.environ.get(k, '')\n")
		TEXT("for _k in ['BOB_MCP_PORT','BOB_MCP_HOST','BOB_MCP_MAX_CLIENTS','BOB_MCP_RATE_LIMIT','BOB_PROJECT_ROOT','BOB_PERMISSION_MODE','BOB_CHAT_TIMEOUT','BOB_MCP_BRIDGE_PORT','BOB_BRIDGE_TOKEN','BOB_MAX_BUDGET','BOB_THINKING_MODE','BOB_THINKING_BUDGET','BOB_EFFORT','BOB_AUTH_MODE','BOB_API_KEY','BOB_API_PROVIDER','BOB_API_REGION','BOB_API_PROJECT_ID','BOB_AUTO_APPROVE_READ_ONLY','BOB_AUTO_APPROVE_VIEWPORT','BOB_AUTO_APPROVE_CREATE','BOB_AUTO_APPROVE_MODIFY','BOB_AUTO_APPROVE_CODE_EXEC','BOB_AUTO_CAPTURE_AFTER_EDITS']:\n")
		TEXT("    os.environ[_k] = _env(_k)\n")
		TEXT("del _env, _k\n");
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
			.Font(Theme::FontHeading())
			.ColorAndOpacity(FSlateColor(Theme::TextSecondary));
	}

	/** Make a key-value row: "Label    Value" with optional color. */
	inline TSharedRef<SWidget> KeyValueRow(const FText& Key, TAttribute<FText> Value, TAttribute<FSlateColor> ValueColor = FSlateColor(FLinearColor::White))
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, Theme::SpaceLG, 0)
			[
				SNew(STextBlock).Text(Key)
				.Font(Theme::FontBody())
				.ColorAndOpacity(FSlateColor(Theme::TextSecondary))
				.MinDesiredWidth(110.f)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(STextBlock).Text(Value)
				.Font(Theme::FontBody())
				.ColorAndOpacity(ValueColor)
			];
	}

	/** Dark card — code blocks, data panels, reference sections. */
	inline TSharedRef<SBorder> Card(TSharedRef<SWidget> Content, FMargin Pad = FMargin(10, 6))
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(Theme::Overlay)
			.Padding(Pad)
			[ Content ];
	}

	/** Surface container — headers, input areas, main sections. */
	inline TSharedRef<SBorder> Container(TSharedRef<SWidget> Content, FMargin Pad = FMargin(6, 5))
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(Theme::Surface)
			.Padding(Pad)
			[ Content ];
	}

	/** Elevated toolbar — control bars, action rows. */
	inline TSharedRef<SBorder> Toolbar(TSharedRef<SWidget> Content, FMargin Pad = FMargin(6, 4))
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			.BorderBackgroundColor(Theme::Elevated)
			.Padding(Pad)
			[ Content ];
	}

	/** Config checkbox — bound to a bool field on FBobBotConfig. Auto-saves and applies env vars. */
	inline TSharedRef<SWidget> ConfigCheckbox(bool FBobBotConfig::* Field, const FText& Label)
	{
		return SNew(SCheckBox)
			.IsChecked_Lambda([Field]()
			{
				return (FBobBotConfig::Get().*Field) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([Field](ECheckBoxState State)
			{
				FBobBotConfig::Get().*Field = (State == ECheckBoxState::Checked);
				FBobBotConfig::Get().Save();
				FBobBotConfig::Get().ApplyEnvironmentVars();
			})
			[
				SNew(STextBlock).Text(Label).Font(Theme::FontSmall())
			];
	}

	/** Config checkbox (no env var sync) — for settings that don't need ApplyEnvironmentVars. */
	inline TSharedRef<SWidget> ConfigCheckboxSimple(bool FBobBotConfig::* Field, const FText& Label)
	{
		return SNew(SCheckBox)
			.IsChecked_Lambda([Field]()
			{
				return (FBobBotConfig::Get().*Field) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([Field](ECheckBoxState State)
			{
				FBobBotConfig::Get().*Field = (State == ECheckBoxState::Checked);
				FBobBotConfig::Get().Save();
			})
			[
				SNew(STextBlock).Text(Label).Font(Theme::FontSmall())
			];
	}

	/** Accent bar + card — status indicators, errors, categories. */
	inline TSharedRef<SWidget> AccentCard(TSharedRef<SWidget> Content, FLinearColor Accent, FMargin Pad = FMargin(10, 6))
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
				.BorderBackgroundColor(Accent)
				.Padding(FMargin(1.5f, 0))
				[ SNullWidget::NullWidget ]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(Theme::SpaceSM, 0, 0, 0)
			[ Card(Content, Pad) ];
	}
}

// =========================================================================== //
// Model name constants
// =========================================================================== //
namespace ModelNames
{
	inline const TCHAR* Sonnet = TEXT("sonnet");
	inline const TCHAR* Opus   = TEXT("opus");
	inline const TCHAR* Haiku  = TEXT("haiku");
}

} // namespace BobBot
