// Copyright Epic Games, Inc. All Rights Reserved.

#include "BobBotStyle.h"
#include "BobBotConstants.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FBobBotStyle::StyleInstance = nullptr;

void FBobBotStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FBobBotStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FBobBotStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("BobBotStyle"));
	return StyleSetName;
}

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FBobBotStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("BobBotStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("BobBot")->GetBaseDir() / TEXT("Resources"));

	Style->Set("BobBot.OpenPluginWindow", new IMAGE_BRUSH_SVG(TEXT("PlaceholderButtonIcon"), Icon20x20));

	// Rich text styles for chat message rendering (using Theme)
	using namespace BobBot::Theme;

	Style->Set("BobBot.Normal", FTextBlockStyle()
		.SetFont(FontBody())
		.SetColorAndOpacity(FSlateColor(TextPrimary)));

	Style->Set("BobBot.Bold", FTextBlockStyle()
		.SetFont(FontHeading())
		.SetColorAndOpacity(FSlateColor(TextPrimary)));

	Style->Set("BobBot.Italic", FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle("Italic", 10))
		.SetColorAndOpacity(FSlateColor(TextPrimary)));

	Style->Set("BobBot.Code", FTextBlockStyle()
		.SetFont(FontCode())
		.SetColorAndOpacity(FSlateColor(TextCode)));

	Style->Set("BobBot.System", FTextBlockStyle()
		.SetFont(FontSmall())
		.SetColorAndOpacity(FSlateColor(TextSecondary)));

	Style->Set("BobBot.Error", FTextBlockStyle()
		.SetFont(FontBody())
		.SetColorAndOpacity(FSlateColor(ErrorAccent)));

	Style->Set("BobBot.Caption", FTextBlockStyle()
		.SetFont(FontCaption())
		.SetColorAndOpacity(FSlateColor(TextSecondary)));

	return Style;
}

void FBobBotStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FBobBotStyle::Get()
{
	return *StyleInstance;
}
