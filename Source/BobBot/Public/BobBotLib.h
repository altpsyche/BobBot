// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/Blueprint.h"
#include "BobBotLib.generated.h"

/**
 * Exposes Blueprint and asset editing functions to Python.
 * All functions are static and callable from Python via unreal.BobBotLib.
 *
 * Covers the gaps in UE 5.4's Python API:
 * - Blueprint variable management
 * - Blueprint component management
 * - Blueprint graph node creation
 * - Blueprint compilation
 */
UCLASS()
class BOBBOT_API UBobBotLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// -- Blueprint Variables --

	/** Add a member variable to a Blueprint. VarType examples: "float", "int32", "bool", "FVector", "FLinearColor", "FString", "UTexture2D*" */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static bool AddBlueprintVariable(UBlueprint* Blueprint, FName VarName, const FString& VarType, bool bInstanceEditable = true);

	/** Remove a member variable from a Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static bool RemoveBlueprintVariable(UBlueprint* Blueprint, FName VarName);

	/** Set the default value of a Blueprint variable (as string representation). */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static bool SetBlueprintVariableDefault(UBlueprint* Blueprint, FName VarName, const FString& DefaultValue);

	/** Set a Blueprint variable's category for organization in the Details panel. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static bool SetBlueprintVariableCategory(UBlueprint* Blueprint, FName VarName, const FString& Category);

	/** Set a Blueprint variable's tooltip. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static bool SetBlueprintVariableTooltip(UBlueprint* Blueprint, FName VarName, const FString& Tooltip);

	/** Set slider range for a numeric Blueprint variable. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static bool SetBlueprintVariableSliderRange(UBlueprint* Blueprint, FName VarName, float Min, float Max);

	/** Get all variable names in a Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static TArray<FName> GetBlueprintVariableNames(UBlueprint* Blueprint);

	// -- Blueprint Components --

	/** Add a component to a Blueprint's Simple Construction Script. Returns the component template name. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString AddComponentToBlueprint(UBlueprint* Blueprint, UClass* ComponentClass, const FString& ComponentName);

	// -- Blueprint Graph --

	/** Add a function call node to a Blueprint graph. Returns node description. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString AddFunctionCallNode(UBlueprint* Blueprint, const FString& FunctionName, UClass* TargetClass, int32 NodeX = 0, int32 NodeY = 0);

	/** Add a "Set MPC Scalar Parameter" node. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString AddSetMPCScalarNode(UBlueprint* Blueprint, const FString& MPCPath, const FString& ParamName, int32 NodeX = 0, int32 NodeY = 0);

	/** Add a "Set MPC Vector Parameter" node. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString AddSetMPCVectorNode(UBlueprint* Blueprint, const FString& MPCPath, const FString& ParamName, int32 NodeX = 0, int32 NodeY = 0);

	// -- Blueprint Compilation --

	/** Compile a Blueprint and return success. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static bool CompileBlueprint(UBlueprint* Blueprint);

	/** Compile a Blueprint and return the status message. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString CompileBlueprintWithStatus(UBlueprint* Blueprint);

	// -- Utility --

	/** Get the Class Default Object of a Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Utility")
	static UObject* GetBlueprintCDO(UBlueprint* Blueprint);

	/** Set a property on a Blueprint's CDO by name and string value. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Utility")
	static bool SetCDOProperty(UBlueprint* Blueprint, const FString& PropertyName, const FString& Value);

	/** Clear the type resolution cache. Call after hot reload if types change. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Utility")
	static void ClearTypeCache();

private:
	static bool ResolveVarType(const FString& VarType, struct FEdGraphPinType& OutPinType);
	static TMap<FString, FEdGraphPinType> TypeCache;
};
