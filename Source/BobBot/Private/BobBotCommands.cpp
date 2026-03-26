// Copyright Epic Games, Inc. All Rights Reserved.

#include "BobBotCommands.h"

#define LOCTEXT_NAMESPACE "FBobBotModule"

void FBobBotCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "BobBot", "Bring up BobBot window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
