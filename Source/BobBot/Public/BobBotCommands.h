// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "BobBotStyle.h"

class FBobBotCommands : public TCommands<FBobBotCommands>
{
public:

	FBobBotCommands()
		: TCommands<FBobBotCommands>(TEXT("BobBot"), NSLOCTEXT("Contexts", "BobBot", "BobBot Plugin"), NAME_None, FBobBotStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};