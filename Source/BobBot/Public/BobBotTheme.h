// BobBot Theme System — unified colors, fonts, spacing
// Header-only. No .cpp needed.

#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

namespace BobBot
{
namespace Theme
{
	// ---- Surface hierarchy (dark to light) ----
	inline const FLinearColor Base(0.02f, 0.02f, 0.02f);
	inline const FLinearColor Surface(0.04f, 0.04f, 0.04f);
	inline const FLinearColor Elevated(0.06f, 0.06f, 0.06f);
	inline const FLinearColor Overlay(0.08f, 0.08f, 0.08f);

	// ---- Semantic colors ----
	inline const FLinearColor Primary(0.12f, 0.44f, 0.84f);     // send button, links
	inline const FLinearColor Success(0.3f, 0.9f, 0.3f);        // completed, auto mode
	inline const FLinearColor Warning(0.9f, 0.8f, 0.1f);        // plan mode, running
	inline const FLinearColor Error(0.9f, 0.2f, 0.2f);          // errors, stop
	inline const FLinearColor Info(0.3f, 0.6f, 1.0f);           // ask mode, user msgs

	// ---- Text colors ----
	inline const FLinearColor TextPrimary = FLinearColor::White;
	inline const FLinearColor TextSecondary(0.5f, 0.5f, 0.5f);
	inline const FLinearColor TextDisabled(0.35f, 0.35f, 0.35f);
	inline const FLinearColor TextCode(0.85f, 0.9f, 0.85f);

	// ---- Brand / accents ----
	inline const FLinearColor BotAccent(0.3f, 0.9f, 0.3f);      // BobBot green
	inline const FLinearColor ErrorAccent(1.0f, 0.5f, 0.1f);    // orange for errors
	inline const FLinearColor ContextBar(0.15f, 0.45f, 0.75f);  // context progress

	// ---- Interactive states ----
	inline const FLinearColor ActiveTab(0.1f, 0.5f, 0.9f, 1.f);
	inline const FLinearColor InactiveTab(0.15f, 0.15f, 0.15f, 1.f);
	inline const FLinearColor ApproveGreen(0.2f, 0.7f, 0.2f);
	inline const FLinearColor DenyRed(0.7f, 0.2f, 0.2f);
	inline const FLinearColor StopRed(0.8f, 0.2f, 0.2f);

	// ---- Spacing scale ----
	constexpr float SpaceXS = 2.f;
	constexpr float SpaceSM = 4.f;
	constexpr float SpaceMD = 8.f;
	constexpr float SpaceLG = 12.f;
	constexpr float SpaceXL = 16.f;

	// ---- Padding presets ----
	inline FMargin PadContainer()    { return FMargin(6.f, 5.f); }
	inline FMargin PadToolbar()      { return FMargin(6.f, 4.f); }
	inline FMargin PadButton()       { return FMargin(6.f, 3.f); }
	inline FMargin PadDropdownItem() { return FMargin(12.f, 6.f); }
	inline FMargin PadInput()        { return FMargin(10.f, 8.f, 10.f, 4.f); }
	inline FMargin PadMessage()      { return FMargin(SpaceMD, SpaceSM); }
	inline FMargin PadCodeBlock()    { return FMargin(SpaceMD, SpaceSM); }

	// ---- Font helpers ----
	inline FSlateFontInfo FontTitle()         { return FCoreStyle::GetDefaultFontStyle("Bold", 18); }
	inline FSlateFontInfo FontHeading()       { return FCoreStyle::GetDefaultFontStyle("Bold", 10); }
	inline FSlateFontInfo FontBody()          { return FCoreStyle::GetDefaultFontStyle("Regular", 10); }
	inline FSlateFontInfo FontSmall()         { return FCoreStyle::GetDefaultFontStyle("Regular", 9); }
	inline FSlateFontInfo FontCaption()       { return FCoreStyle::GetDefaultFontStyle("Regular", 8); }
	inline FSlateFontInfo FontCode()          { return FCoreStyle::GetDefaultFontStyle("Mono", 9); }
	inline FSlateFontInfo FontCodeSmall()     { return FCoreStyle::GetDefaultFontStyle("Mono", 8); }
	inline FSlateFontInfo FontItalic()         { return FCoreStyle::GetDefaultFontStyle("Italic", 10); }
	inline FSlateFontInfo FontItalicSmall()    { return FCoreStyle::GetDefaultFontStyle("Italic", 9); }
	inline FSlateFontInfo FontIcon()          { return FCoreStyle::GetDefaultFontStyle("Regular", 12); }
	inline FSlateFontInfo FontDropdownTitle() { return FCoreStyle::GetDefaultFontStyle("Bold", 9); }
	inline FSlateFontInfo FontStat()          { return FCoreStyle::GetDefaultFontStyle("Bold", 16); }

} // namespace Theme
} // namespace BobBot
