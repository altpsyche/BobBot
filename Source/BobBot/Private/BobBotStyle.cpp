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

	// Icons
	Style->Set("BobBot.Icon.Send", new IMAGE_BRUSH_SVG(TEXT("SendIcon"), Icon16x16));
	Style->Set("BobBot.Icon.Stop", new IMAGE_BRUSH_SVG(TEXT("StopIcon"), Icon16x16));
	Style->Set("BobBot.Icon.Undo", new IMAGE_BRUSH_SVG(TEXT("UndoIcon"), Icon16x16));
	Style->Set("BobBot.Icon.Gear", new IMAGE_BRUSH_SVG(TEXT("GearIcon"), Icon16x16));
	Style->Set("BobBot.Icon.Status", new IMAGE_BRUSH_SVG(TEXT("StatusDot"), Icon16x16));
	Style->Set("BobBot.Icon.Check", new IMAGE_BRUSH_SVG(TEXT("CheckIcon"), Icon16x16));
	Style->Set("BobBot.Icon.Fail", new IMAGE_BRUSH_SVG(TEXT("FailIcon"), Icon16x16));
	Style->Set("BobBot.Icon.Spinner", new IMAGE_BRUSH_SVG(TEXT("SpinnerIcon"), Icon16x16));
	Style->Set("BobBot.Icon.ChevronRight", new IMAGE_BRUSH_SVG(TEXT("ChevronRight"), Icon16x16));
	Style->Set("BobBot.Icon.ChevronDown", new IMAGE_BRUSH_SVG(TEXT("ChevronDown"), Icon16x16));
	Style->Set("BobBot.Icon.Plus", new IMAGE_BRUSH_SVG(TEXT("PlusIcon"), Icon16x16));
	Style->Set("BobBot.Icon.Fork", new IMAGE_BRUSH_SVG(TEXT("ForkIcon"), Icon16x16));
	Style->Set("BobBot.Icon.Edit", new IMAGE_BRUSH_SVG(TEXT("EditIcon"), Icon16x16));
	Style->Set("BobBot.Icon.Trash", new IMAGE_BRUSH_SVG(TEXT("TrashIcon"), Icon16x16));

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
