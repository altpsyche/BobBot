// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "BobBotLib.h"
#include "BobBot.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_CallFunction.h"
#include "EdGraphSchema_K2.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"

TMap<FString, FEdGraphPinType> UBobBotLib::TypeCache;

// -- Type Resolution (with cache) --

bool UBobBotLib::ResolveVarType(const FString& VarType, FEdGraphPinType& OutPinType)
{
	FString Key = VarType.ToLower();

	// Check cache first
	if (const FEdGraphPinType* Cached = TypeCache.Find(Key))
	{
		OutPinType = *Cached;
		return true;
	}

	OutPinType.PinCategory = NAME_None;
	OutPinType.PinSubCategory = NAME_None;
	OutPinType.PinSubCategoryObject = nullptr;

	if (Key == TEXT("float") || Key == TEXT("double"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (Key == TEXT("int") || Key == TEXT("int32"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (Key == TEXT("int64"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
	}
	else if (Key == TEXT("bool"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (Key == TEXT("fstring") || Key == TEXT("string"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (Key == TEXT("fname") || Key == TEXT("name"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (Key == TEXT("ftext") || Key == TEXT("text"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
	}
	else if (Key == TEXT("byte") || Key == TEXT("uint8"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
	}
	else if (Key == TEXT("fvector") || Key == TEXT("vector"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (Key == TEXT("frotator") || Key == TEXT("rotator"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (Key == TEXT("ftransform") || Key == TEXT("transform"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	}
	else if (Key == TEXT("flinearcolor") || Key == TEXT("linearcolor") || Key == TEXT("color"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
	}
	else if (Key == TEXT("fvector2d") || Key == TEXT("vector2d"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		OutPinType.PinSubCategoryObject = TBaseStructure<FVector2D>::Get();
	}
	else
	{
		// Try as object reference (e.g., "UTexture2D*", "UStaticMesh*")
		FString CleanType = VarType;
		CleanType.RemoveFromEnd(TEXT("*"));
		CleanType.RemoveFromStart(TEXT("U"));
		CleanType.RemoveFromStart(TEXT("A"));

		UClass* FoundClass = FindFirstObject<UClass>(*CleanType, EFindFirstObjectOptions::ExactClass);
		if (!FoundClass)
		{
			FoundClass = FindFirstObject<UClass>(*FString::Printf(TEXT("U%s"), *CleanType), EFindFirstObjectOptions::ExactClass);
		}
		if (!FoundClass)
		{
			FoundClass = FindFirstObject<UClass>(*FString::Printf(TEXT("A%s"), *CleanType), EFindFirstObjectOptions::ExactClass);
		}

		if (FoundClass)
		{
			OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			OutPinType.PinSubCategoryObject = FoundClass;
		}
		else
		{
			UE_LOG(LogBobBot, Warning, TEXT("Could not resolve type: %s"), *VarType);
			return false;
		}
	}

	// Cache on success
	TypeCache.Add(Key, OutPinType);
	return true;
}

void UBobBotLib::ClearTypeCache()
{
	TypeCache.Empty();
	UE_LOG(LogBobBot, Log, TEXT("Type cache cleared"));
}

// -- Blueprint Variables --

static FBPVariableDescription* FindBlueprintVariable(UBlueprint* Blueprint, FName VarName)
{
	for (FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		if (Var.VarName == VarName)
		{
			return &Var;
		}
	}
	return nullptr;
}

bool UBobBotLib::AddBlueprintVariable(UBlueprint* Blueprint, FName VarName, const FString& VarType, bool bInstanceEditable)
{
	if (!Blueprint)
	{
		UE_LOG(LogBobBot, Error, TEXT("AddBlueprintVariable: null Blueprint"));
		return false;
	}

	FEdGraphPinType PinType;
	if (!ResolveVarType(VarType, PinType))
	{
		return false;
	}

	const bool bResult = FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, PinType);
	if (bResult && bInstanceEditable)
	{
		FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(Blueprint, VarName, false);
		FBlueprintEditorUtils::SetInterpFlag(Blueprint, VarName, false);
	}

	if (bResult)
	{
		UE_LOG(LogBobBot, Log, TEXT("Added variable '%s' (%s) to %s"), *VarName.ToString(), *VarType, *Blueprint->GetName());
	}

	return bResult;
}

bool UBobBotLib::RemoveBlueprintVariable(UBlueprint* Blueprint, FName VarName)
{
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VarName);
	return true;
}

bool UBobBotLib::SetBlueprintVariableDefault(UBlueprint* Blueprint, FName VarName, const FString& DefaultValue)
{
	if (!Blueprint)
	{
		return false;
	}

	FBPVariableDescription* Var = FindBlueprintVariable(Blueprint, VarName);
	if (!Var)
	{
		UE_LOG(LogBobBot, Warning, TEXT("Variable '%s' not found in %s"), *VarName.ToString(), *Blueprint->GetName());
		return false;
	}

	Var->DefaultValue = DefaultValue;
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return true;
}

bool UBobBotLib::SetBlueprintVariableCategory(UBlueprint* Blueprint, FName VarName, const FString& Category)
{
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, VarName, nullptr, FText::FromString(Category));
	return true;
}

bool UBobBotLib::SetBlueprintVariableTooltip(UBlueprint* Blueprint, FName VarName, const FString& Tooltip)
{
	if (!Blueprint)
	{
		return false;
	}

	FBPVariableDescription* Var = FindBlueprintVariable(Blueprint, VarName);
	if (!Var)
	{
		return false;
	}

	Var->SetMetaData(FBlueprintMetadata::MD_Tooltip, *Tooltip);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return true;
}

bool UBobBotLib::SetBlueprintVariableSliderRange(UBlueprint* Blueprint, FName VarName, float Min, float Max)
{
	if (!Blueprint)
	{
		return false;
	}

	FBPVariableDescription* Var = FindBlueprintVariable(Blueprint, VarName);
	if (!Var)
	{
		return false;
	}

	Var->SetMetaData(FName(TEXT("ClampMin")), *FString::SanitizeFloat(Min));
	Var->SetMetaData(FName(TEXT("ClampMax")), *FString::SanitizeFloat(Max));
	Var->SetMetaData(FName(TEXT("UIMin")), *FString::SanitizeFloat(Min));
	Var->SetMetaData(FName(TEXT("UIMax")), *FString::SanitizeFloat(Max));
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return true;
}

TArray<FName> UBobBotLib::GetBlueprintVariableNames(UBlueprint* Blueprint)
{
	TArray<FName> Names;
	if (Blueprint)
	{
		for (const FBPVariableDescription& Var : Blueprint->NewVariables)
		{
			Names.Add(Var.VarName);
		}
	}
	return Names;
}

// -- Blueprint Components --

FString UBobBotLib::AddComponentToBlueprint(UBlueprint* Blueprint, UClass* ComponentClass, const FString& ComponentName)
{
	if (!Blueprint || !ComponentClass)
	{
		return TEXT("ERROR: null Blueprint or ComponentClass");
	}

	if (!ComponentClass->IsChildOf(UActorComponent::StaticClass()))
	{
		return TEXT("ERROR: class is not a UActorComponent subclass");
	}

	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
	if (!SCS)
	{
		return TEXT("ERROR: Blueprint has no SimpleConstructionScript");
	}

	USCS_Node* NewNode = SCS->CreateNode(ComponentClass, *ComponentName);
	if (!NewNode)
	{
		return TEXT("ERROR: failed to create SCS node");
	}

	SCS->AddNode(NewNode);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogBobBot, Log, TEXT("Added component '%s' (%s) to %s"),
		*ComponentName, *ComponentClass->GetName(), *Blueprint->GetName());

	return NewNode->GetVariableName().ToString();
}

// -- Blueprint Graph --

FString UBobBotLib::AddFunctionCallNode(UBlueprint* Blueprint, const FString& FunctionName, UClass* TargetClass, int32 NodeX, int32 NodeY)
{
	if (!Blueprint || !TargetClass)
	{
		return TEXT("ERROR: null Blueprint or TargetClass");
	}

	UFunction* Function = TargetClass->FindFunctionByName(*FunctionName);
	if (!Function)
	{
		return FString::Printf(TEXT("ERROR: function '%s' not found on %s"), *FunctionName, *TargetClass->GetName());
	}

	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	if (!EventGraph)
	{
		return TEXT("ERROR: no event graph found");
	}

	UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(EventGraph);
	CallNode->FunctionReference.SetExternalMember(*FunctionName, TargetClass);
	CallNode->NodePosX = NodeX;
	CallNode->NodePosY = NodeY;

	EventGraph->AddNode(CallNode, false, false);
	CallNode->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	return FString::Printf(TEXT("OK: %s::%s at (%d,%d)"), *TargetClass->GetName(), *FunctionName, NodeX, NodeY);
}

FString UBobBotLib::AddSetMPCScalarNode(UBlueprint* Blueprint, const FString& MPCPath, const FString& ParamName, int32 NodeX, int32 NodeY)
{
	if (!Blueprint)
	{
		return TEXT("ERROR: null Blueprint");
	}

	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	if (!EventGraph)
	{
		return TEXT("ERROR: no event graph found");
	}

	UFunction* SetScalarFunc = UKismetMaterialLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UKismetMaterialLibrary, SetScalarParameterValue));
	if (!SetScalarFunc)
	{
		return TEXT("ERROR: SetScalarParameterValue function not found");
	}

	UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(EventGraph);
	CallNode->FunctionReference.SetExternalMember(SetScalarFunc->GetFName(), UKismetMaterialLibrary::StaticClass());
	CallNode->NodePosX = NodeX;
	CallNode->NodePosY = NodeY;

	EventGraph->AddNode(CallNode, false, false);
	CallNode->AllocateDefaultPins();

	if (UEdGraphPin* CollectionPin = CallNode->FindPin(TEXT("Collection")))
	{
		CollectionPin->DefaultValue = MPCPath;
	}
	if (UEdGraphPin* ParamNamePin = CallNode->FindPin(TEXT("ParameterName")))
	{
		ParamNamePin->DefaultValue = ParamName;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return FString::Printf(TEXT("OK: SetScalarParameterValue(%s, %s)"), *MPCPath, *ParamName);
}

FString UBobBotLib::AddSetMPCVectorNode(UBlueprint* Blueprint, const FString& MPCPath, const FString& ParamName, int32 NodeX, int32 NodeY)
{
	if (!Blueprint)
	{
		return TEXT("ERROR: null Blueprint");
	}

	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	if (!EventGraph)
	{
		return TEXT("ERROR: no event graph found");
	}

	UFunction* SetVectorFunc = UKismetMaterialLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UKismetMaterialLibrary, SetVectorParameterValue));
	if (!SetVectorFunc)
	{
		return TEXT("ERROR: SetVectorParameterValue function not found");
	}

	UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(EventGraph);
	CallNode->FunctionReference.SetExternalMember(SetVectorFunc->GetFName(), UKismetMaterialLibrary::StaticClass());
	CallNode->NodePosX = NodeX;
	CallNode->NodePosY = NodeY;

	EventGraph->AddNode(CallNode, false, false);
	CallNode->AllocateDefaultPins();

	if (UEdGraphPin* CollectionPin = CallNode->FindPin(TEXT("Collection")))
	{
		CollectionPin->DefaultValue = MPCPath;
	}
	if (UEdGraphPin* ParamNamePin = CallNode->FindPin(TEXT("ParameterName")))
	{
		ParamNamePin->DefaultValue = ParamName;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return FString::Printf(TEXT("OK: SetVectorParameterValue(%s, %s)"), *MPCPath, *ParamName);
}

// -- Blueprint Compilation --

bool UBobBotLib::CompileBlueprint(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return false;
	}

	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
	return Blueprint->Status == BS_UpToDate;
}

FString UBobBotLib::CompileBlueprintWithStatus(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return TEXT("ERROR: null Blueprint");
	}

	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);

	switch (Blueprint->Status)
	{
	case BS_UpToDate:
		return TEXT("OK: compiled successfully");
	case BS_Error:
		return TEXT("ERROR: compilation failed");
	case BS_Unknown:
		return TEXT("UNKNOWN: compilation status unknown");
	default:
		return TEXT("WARNING: unexpected status");
	}
}

// -- Utility --

UObject* UBobBotLib::GetBlueprintCDO(UBlueprint* Blueprint)
{
	if (!Blueprint || !Blueprint->GeneratedClass)
	{
		return nullptr;
	}

	return Blueprint->GeneratedClass->GetDefaultObject();
}

bool UBobBotLib::SetCDOProperty(UBlueprint* Blueprint, const FString& PropertyName, const FString& Value)
{
	UObject* CDO = GetBlueprintCDO(Blueprint);
	if (!CDO)
	{
		UE_LOG(LogBobBot, Warning, TEXT("SetCDOProperty: could not get CDO for %s"),
			Blueprint ? *Blueprint->GetName() : TEXT("null"));
		return false;
	}

	FProperty* Prop = CDO->GetClass()->FindPropertyByName(*PropertyName);
	if (!Prop)
	{
		UE_LOG(LogBobBot, Warning, TEXT("SetCDOProperty: property '%s' not found"), *PropertyName);
		return false;
	}

	void* PropAddr = Prop->ContainerPtrToValuePtr<void>(CDO);
	if (!Prop->ImportText_Direct(*Value, PropAddr, CDO, PPF_None))
	{
		UE_LOG(LogBobBot, Warning, TEXT("SetCDOProperty: failed to import value '%s' for '%s'"), *Value, *PropertyName);
		return false;
	}

	CDO->MarkPackageDirty();
	return true;
}
