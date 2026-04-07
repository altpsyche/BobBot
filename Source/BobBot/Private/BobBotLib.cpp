// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "BobBotLib.h"
#include "BobBot.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "EdGraphSchema_K2.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_DynamicCast.h"

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

FString UBobBotLib::AddFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName)
{
	if (!Blueprint)
	{
		return TEXT("ERROR: null Blueprint");
	}

	// Check if function already exists
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == FName(*FunctionName))
		{
			return FString::Printf(TEXT("ERROR: function '%s' already exists"), *FunctionName);
		}
	}

	// Create new function graph
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (!NewGraph)
	{
		return FString::Printf(TEXT("ERROR: failed to create graph for '%s'"), *FunctionName);
	}

	// AddFunctionGraph is a template: AddFunctionGraph<UClass>(bp, graph, bUserCreated, SignatureClass*)
	// Passing nullptr for SignatureFromObject creates a basic function with no predefined signature
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, NewGraph, /*bIsUserCreated=*/ true, nullptr);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogBobBot, Log, TEXT("Created function '%s' on %s"), *FunctionName, *Blueprint->GetName());
	return FunctionName;
}

FString UBobBotLib::AddCustomEvent(UBlueprint* Blueprint, const FString& EventName)
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

	// Create the custom event node
	UK2Node_CustomEvent* EventNode = NewObject<UK2Node_CustomEvent>(EventGraph);
	EventNode->CustomFunctionName = FName(*EventName);
	EventNode->NodePosX = 0;
	EventNode->NodePosY = 0;

	EventGraph->AddNode(EventNode, /*bFromUI=*/ false, /*bSelectNewNode=*/ false);
	EventNode->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogBobBot, Log, TEXT("Created custom event '%s' on %s"), *EventName, *Blueprint->GetName());
	return EventName;
}

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

// -- Blueprint Node Wiring --

// Helper for ConnectPins: collect all nodes whose title (case-insensitive)
// or unique GetName() (case-sensitive) matches the supplied label.
// GetName() is the synthesized unique node name (e.g. "K2Node_CallFunction_3")
// — useful for disambiguation. Title is the user-visible label, which may be
// duplicated across multiple nodes (two "Print String" calls, etc).
static void FindMatchingNodes(UBlueprint* Blueprint, const FString& Label, TArray<UEdGraphNode*>& OutMatches)
{
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	for (UEdGraph* Graph : AllGraphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			const FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::EditableTitle).ToString();
			const FString NodeName = Node->GetName();
			if (NodeTitle.Equals(Label, ESearchCase::IgnoreCase) || NodeName == Label)
			{
				OutMatches.Add(Node);
			}
		}
	}
}

// Build the ambiguous-match diagnostic listing every candidate by its
// unique GetName(), so the caller can re-invoke with the disambiguator.
static FString FormatAmbiguousNodeError(const TCHAR* Role, const FString& Label, const TArray<UEdGraphNode*>& Matches)
{
	FString Out = FString::Printf(TEXT("ERROR: %s node '%s' is ambiguous (%d matches: "),
		Role, *Label, Matches.Num());
	for (int32 i = 0; i < Matches.Num(); ++i)
	{
		if (i > 0) Out += TEXT(", ");
		Out += Matches[i]->GetName();
	}
	Out += TEXT("). Re-call with the unique node name from this list.");
	return Out;
}

FString UBobBotLib::ConnectPins(UBlueprint* Blueprint, const FString& SourceNodeName, const FString& SourcePinName,
	const FString& TargetNodeName, const FString& TargetPinName)
{
	if (!Blueprint) return TEXT("ERROR: null Blueprint");

	TArray<UEdGraphNode*> SourceMatches;
	TArray<UEdGraphNode*> TargetMatches;
	FindMatchingNodes(Blueprint, SourceNodeName, SourceMatches);
	FindMatchingNodes(Blueprint, TargetNodeName, TargetMatches);

	if (SourceMatches.Num() == 0)
		return FString::Printf(TEXT("ERROR: source node '%s' not found"), *SourceNodeName);
	if (SourceMatches.Num() > 1)
		return FormatAmbiguousNodeError(TEXT("source"), SourceNodeName, SourceMatches);
	if (TargetMatches.Num() == 0)
		return FString::Printf(TEXT("ERROR: target node '%s' not found"), *TargetNodeName);
	if (TargetMatches.Num() > 1)
		return FormatAmbiguousNodeError(TEXT("target"), TargetNodeName, TargetMatches);

	UEdGraphNode* SourceNode = SourceMatches[0];
	UEdGraphNode* TargetNode = TargetMatches[0];

	UEdGraphPin* SourcePin = SourceNode->FindPin(FName(*SourcePinName));
	if (!SourcePin)
		return FString::Printf(TEXT("ERROR: source pin '%s' not found on '%s'"), *SourcePinName, *SourceNodeName);

	UEdGraphPin* TargetPin = TargetNode->FindPin(FName(*TargetPinName));
	if (!TargetPin)
		return FString::Printf(TEXT("ERROR: target pin '%s' not found on '%s'"), *TargetPinName, *TargetNodeName);

	// Make the connection
	if (!SourcePin->GetSchema()->TryCreateConnection(SourcePin, TargetPin))
		return FString::Printf(TEXT("ERROR: could not connect %s.%s -> %s.%s (type mismatch?)"),
			*SourceNodeName, *SourcePinName, *TargetNodeName, *TargetPinName);

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return FString::Printf(TEXT("OK: connected %s.%s -> %s.%s"),
		*SourceNodeName, *SourcePinName, *TargetNodeName, *TargetPinName);
}

// -- Blueprint Graph Reading --

// Format a single pin in compact form: name(category) [-> target.pin, ...]
static FString FormatPin(const UEdGraphPin* Pin)
{
	if (!Pin) return FString();

	FString Out = Pin->PinName.ToString();
	if (!Pin->PinType.PinCategory.IsNone())
	{
		Out += TEXT("(") + Pin->PinType.PinCategory.ToString();
		if (!Pin->PinType.PinSubCategory.IsNone())
		{
			Out += TEXT("/") + Pin->PinType.PinSubCategory.ToString();
		}
		Out += TEXT(")");
	}

	if (Pin->LinkedTo.Num() > 0)
	{
		Out += TEXT(" -> [");
		for (int32 i = 0; i < Pin->LinkedTo.Num(); ++i)
		{
			if (i > 0) Out += TEXT(", ");
			const UEdGraphPin* Linked = Pin->LinkedTo[i];
			if (Linked && Linked->GetOwningNode())
			{
				Out += FString::Printf(TEXT("%s.%s"),
					*Linked->GetOwningNode()->GetName(),
					*Linked->PinName.ToString());
			}
			else
			{
				Out += TEXT("?");
			}
		}
		Out += TEXT("]");
	}
	return Out;
}

// Append a one-line summary of a node to OutBuilder.
//   Name | Title | (x,y) | in: ... | out: ...
static void DescribeNodeOneLine(const UEdGraphNode* Node, FString& OutBuilder)
{
	if (!Node) return;

	const FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
	OutBuilder += FString::Printf(TEXT("%s | %s | (%d,%d)"),
		*Node->GetName(), *Title, Node->NodePosX, Node->NodePosY);

	FString InputList;
	FString OutputList;
	int32 InputCount = 0;
	int32 OutputCount = 0;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin || Pin->bHidden) continue;
		const FString PinStr = FormatPin(Pin);
		if (Pin->Direction == EGPD_Input)
		{
			if (InputCount > 0) InputList += TEXT(", ");
			InputList += PinStr;
			++InputCount;
		}
		else
		{
			if (OutputCount > 0) OutputList += TEXT(", ");
			OutputList += PinStr;
			++OutputCount;
		}
	}
	if (InputCount > 0)
	{
		OutBuilder += TEXT(" | in: ") + InputList;
	}
	if (OutputCount > 0)
	{
		OutBuilder += TEXT(" | out: ") + OutputList;
	}
	OutBuilder += TEXT("\n");
}

FString UBobBotLib::DescribeBlueprintGraph(UBlueprint* Blueprint, FName GraphName)
{
	if (!Blueprint) return TEXT("ERROR: null Blueprint");

	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	// Filter to a specific graph if a name was provided
	TArray<UEdGraph*> Selected;
	if (GraphName.IsNone() || GraphName.ToString().IsEmpty())
	{
		Selected = AllGraphs;
	}
	else
	{
		for (UEdGraph* Graph : AllGraphs)
		{
			if (Graph && Graph->GetFName() == GraphName)
			{
				Selected.Add(Graph);
			}
		}
		if (Selected.Num() == 0)
		{
			FString Available;
			for (UEdGraph* Graph : AllGraphs)
			{
				if (Graph)
				{
					if (!Available.IsEmpty()) Available += TEXT(", ");
					Available += Graph->GetName();
				}
			}
			return FString::Printf(TEXT("ERROR: graph '%s' not found. Available: %s"),
				*GraphName.ToString(), *Available);
		}
	}

	FString Out;
	Out += FString::Printf(TEXT("Blueprint: %s\n"), *Blueprint->GetPathName());

	const int32 NodeBudget = 80;
	int32 EmittedNodes = 0;
	int32 SkippedNodes = 0;

	for (UEdGraph* Graph : Selected)
	{
		if (!Graph) continue;
		Out += FString::Printf(TEXT("Graph: %s (%d nodes)\n"), *Graph->GetName(), Graph->Nodes.Num());

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			if (EmittedNodes >= NodeBudget)
			{
				++SkippedNodes;
				continue;
			}
			Out += TEXT("  ");
			DescribeNodeOneLine(Node, Out);
			++EmittedNodes;
		}
	}

	if (SkippedNodes > 0)
	{
		Out += FString::Printf(TEXT("(... %d more nodes truncated; call get_node_details for specific nodes)\n"),
			SkippedNodes);
	}

	return Out;
}

FString UBobBotLib::DescribeBlueprintNode(UBlueprint* Blueprint, const FString& NodeName)
{
	if (!Blueprint) return TEXT("ERROR: null Blueprint");
	if (NodeName.IsEmpty()) return TEXT("ERROR: NodeName is empty");

	// Reuse FindMatchingNodes for ambiguity-aware lookup
	TArray<UEdGraphNode*> Matches;
	FindMatchingNodes(Blueprint, NodeName, Matches);

	if (Matches.Num() == 0)
		return FString::Printf(TEXT("ERROR: node '%s' not found"), *NodeName);
	if (Matches.Num() > 1)
		return FormatAmbiguousNodeError(TEXT("node"), NodeName, Matches);

	UEdGraphNode* Node = Matches[0];
	FString Out;
	Out += FString::Printf(TEXT("Blueprint: %s\n"), *Blueprint->GetPathName());
	if (Node->GetGraph())
	{
		Out += FString::Printf(TEXT("Graph: %s\n"), *Node->GetGraph()->GetName());
	}
	Out += FString::Printf(TEXT("Node: %s\n"), *Node->GetName());
	Out += FString::Printf(TEXT("Title: %s\n"), *Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
	Out += FString::Printf(TEXT("Class: %s\n"), *Node->GetClass()->GetName());
	Out += FString::Printf(TEXT("Position: (%d,%d)\n"), Node->NodePosX, Node->NodePosY);
	Out += FString::Printf(TEXT("Pins (%d):\n"), Node->Pins.Num());

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin) continue;
		const TCHAR* DirChar = (Pin->Direction == EGPD_Input) ? TEXT("in ") : TEXT("out");
		const TCHAR* HiddenMark = Pin->bHidden ? TEXT(" [hidden]") : TEXT("");

		FString TypeStr = Pin->PinType.PinCategory.ToString();
		if (!Pin->PinType.PinSubCategory.IsNone())
		{
			TypeStr += TEXT("/") + Pin->PinType.PinSubCategory.ToString();
		}
		if (Pin->PinType.PinSubCategoryObject.IsValid())
		{
			TypeStr += TEXT("(") + Pin->PinType.PinSubCategoryObject->GetName() + TEXT(")");
		}

		Out += FString::Printf(TEXT("  %s %s : %s%s"),
			DirChar, *Pin->PinName.ToString(), *TypeStr, HiddenMark);

		if (!Pin->DefaultValue.IsEmpty())
		{
			Out += FString::Printf(TEXT(" = %s"), *Pin->DefaultValue);
		}

		if (Pin->LinkedTo.Num() > 0)
		{
			Out += TEXT("\n    linked to:");
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked && Linked->GetOwningNode())
				{
					Out += FString::Printf(TEXT("\n      %s.%s"),
						*Linked->GetOwningNode()->GetName(),
						*Linked->PinName.ToString());
				}
			}
		}
		Out += TEXT("\n");
	}

	return Out;
}

FString UBobBotLib::AddBranchNode(UBlueprint* Blueprint, int32 NodeX, int32 NodeY)
{
	if (!Blueprint) return TEXT("ERROR: null Blueprint");

	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	if (!EventGraph) return TEXT("ERROR: no event graph found");

	UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(EventGraph);
	BranchNode->NodePosX = NodeX;
	BranchNode->NodePosY = NodeY;

	EventGraph->AddNode(BranchNode, false, false);
	BranchNode->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return FString::Printf(TEXT("OK: Branch at (%d,%d) [pins: Execute, Condition, Then, Else]"), NodeX, NodeY);
}

FString UBobBotLib::AddVariableGetNode(UBlueprint* Blueprint, const FString& VarName, int32 NodeX, int32 NodeY)
{
	if (!Blueprint) return TEXT("ERROR: null Blueprint");

	// Verify the variable exists
	FProperty* Prop = FindFProperty<FProperty>(Blueprint->SkeletonGeneratedClass, *VarName);
	if (!Prop)
		Prop = FindFProperty<FProperty>(Blueprint->GeneratedClass, *VarName);
	if (!Prop)
		return FString::Printf(TEXT("ERROR: variable '%s' not found on Blueprint"), *VarName);

	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	if (!EventGraph) return TEXT("ERROR: no event graph found");

	UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(EventGraph);
	GetNode->VariableReference.SetSelfMember(FName(*VarName));
	GetNode->NodePosX = NodeX;
	GetNode->NodePosY = NodeY;

	EventGraph->AddNode(GetNode, false, false);
	GetNode->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return FString::Printf(TEXT("OK: Get %s at (%d,%d)"), *VarName, NodeX, NodeY);
}

FString UBobBotLib::AddVariableSetNode(UBlueprint* Blueprint, const FString& VarName, int32 NodeX, int32 NodeY)
{
	if (!Blueprint) return TEXT("ERROR: null Blueprint");

	FProperty* Prop = FindFProperty<FProperty>(Blueprint->SkeletonGeneratedClass, *VarName);
	if (!Prop)
		Prop = FindFProperty<FProperty>(Blueprint->GeneratedClass, *VarName);
	if (!Prop)
		return FString::Printf(TEXT("ERROR: variable '%s' not found on Blueprint"), *VarName);

	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	if (!EventGraph) return TEXT("ERROR: no event graph found");

	UK2Node_VariableSet* SetNode = NewObject<UK2Node_VariableSet>(EventGraph);
	SetNode->VariableReference.SetSelfMember(FName(*VarName));
	SetNode->NodePosX = NodeX;
	SetNode->NodePosY = NodeY;

	EventGraph->AddNode(SetNode, false, false);
	SetNode->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return FString::Printf(TEXT("OK: Set %s at (%d,%d)"), *VarName, NodeX, NodeY);
}

FString UBobBotLib::AddCastNode(UBlueprint* Blueprint, UClass* TargetClass, int32 NodeX, int32 NodeY)
{
	if (!Blueprint) return TEXT("ERROR: null Blueprint");
	if (!TargetClass) return TEXT("ERROR: null TargetClass");

	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	if (!EventGraph) return TEXT("ERROR: no event graph found");

	UK2Node_DynamicCast* CastNode = NewObject<UK2Node_DynamicCast>(EventGraph);
	CastNode->TargetType = TargetClass;
	CastNode->NodePosX = NodeX;
	CastNode->NodePosY = NodeY;

	EventGraph->AddNode(CastNode, false, false);
	CastNode->AllocateDefaultPins();

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return FString::Printf(TEXT("OK: Cast to %s at (%d,%d)"), *TargetClass->GetName(), NodeX, NodeY);
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
