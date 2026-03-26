// Copyright Epic Games, Inc. All Rights Reserved.

#include "BobBot.h"
#include "BobBotStyle.h"
#include "BobBotCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"

static const FName BobBotTabName("BobBot");

#define LOCTEXT_NAMESPACE "FBobBotModule"

void FBobBotModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
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
}

void FBobBotModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FBobBotStyle::Shutdown();

	FBobBotCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BobBotTabName);
}

TSharedRef<SDockTab> FBobBotModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	FText WidgetText = FText::Format(
		LOCTEXT("WindowWidgetText", "Add code to {0} in {1} to override this window's contents"),
		FText::FromString(TEXT("FBobBotModule::OnSpawnPluginTab")),
		FText::FromString(TEXT("BobBot.cpp"))
		);

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			// Put your tab content here!
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(WidgetText)
			]
		];
}

void FBobBotModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(BobBotTabName);
}

void FBobBotModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FBobBotCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FBobBotCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBobBotModule, BobBot)