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

	/** Create a new function graph on a Blueprint. Returns function name on success. Works on all UE 5.4+ versions. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString AddFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName);

	/** Create a custom event node in a Blueprint's event graph. Returns event name on success. Works on all UE 5.4+ versions. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString AddCustomEvent(UBlueprint* Blueprint, const FString& EventName);

	/** Add a function call node to a Blueprint graph. Returns node description. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString AddFunctionCallNode(UBlueprint* Blueprint, const FString& FunctionName, UClass* TargetClass, int32 NodeX = 0, int32 NodeY = 0);

	/** Add a "Set MPC Scalar Parameter" node. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString AddSetMPCScalarNode(UBlueprint* Blueprint, const FString& MPCPath, const FString& ParamName, int32 NodeX = 0, int32 NodeY = 0);

	/** Add a "Set MPC Vector Parameter" node. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString AddSetMPCVectorNode(UBlueprint* Blueprint, const FString& MPCPath, const FString& ParamName, int32 NodeX = 0, int32 NodeY = 0);

	// -- Blueprint Node Wiring --

	/** Connect two pins in a Blueprint graph by node and pin name. Returns OK or ERROR. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString ConnectPins(UBlueprint* Blueprint, const FString& SourceNodeName, const FString& SourcePinName,
		const FString& TargetNodeName, const FString& TargetPinName);

	/** Add a Branch (if/else) node to a Blueprint's event graph. Returns node description. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString AddBranchNode(UBlueprint* Blueprint, int32 NodeX = 0, int32 NodeY = 0);

	/** Add a "Get Variable" node for a Blueprint member variable. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString AddVariableGetNode(UBlueprint* Blueprint, const FString& VarName, int32 NodeX = 0, int32 NodeY = 0);

	/** Add a "Set Variable" node for a Blueprint member variable. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString AddVariableSetNode(UBlueprint* Blueprint, const FString& VarName, int32 NodeX = 0, int32 NodeY = 0);

	/** Add a Cast node for a given target class. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString AddCastNode(UBlueprint* Blueprint, UClass* TargetClass, int32 NodeX = 0, int32 NodeY = 0);

	// -- Blueprint Graph Reading --

	/** Describe an entire Blueprint graph (or all graphs if GraphName is empty).
	 *  Returns one node per line: name | title | (x,y) | inputs | outputs | links.
	 *  Used by Python tools to introspect existing Blueprint logic. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString DescribeBlueprintGraph(UBlueprint* Blueprint, FName GraphName);

	/** Describe a single Blueprint node in detail by its unique synthesized name
	 *  (e.g. "K2Node_CallFunction_3") or by editable title. Returns full pin list
	 *  with type, direction, default value, and every linked target. Returns an
	 *  ambiguous-match error listing every candidate when the title matches multiple. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Blueprint")
	static FString DescribeBlueprintNode(UBlueprint* Blueprint, const FString& NodeName);

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

	// -- Reflection (bypasses BlueprintReadable gate on protected UPROPERTYs) --

	/** ExportText serialization of any UPROPERTY by name. Empty string if not found / null / unsupported.
	 *  Use only for agent inspection (human-readable). For machine-parsed output, use a typed getter. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Reflection")
	static FString ReadPropertyAsString(UObject* Object, FName PropertyName);

	/** Contents of a TArray<UObject*>-typed UPROPERTY. Empty array on miss / type mismatch / null.
	 *  Covers TArray<TObjectPtr<T>> too. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Reflection")
	static TArray<UObject*> ReadObjectArrayProperty(UObject* Object, FName PropertyName);

	// -- Level / Asset Inspection --

	/** Public-getter wrapper around UWorld::GetStreamingLevels(). UWorld::StreamingLevels is
	 *  a protected UPROPERTY which Python's get_editor_property cannot access. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static TArray<class ULevelStreaming*> GetWorldStreamingLevels(class UWorld* World);

	// -- Static Mesh / LOD / Nanite --

	/** Number of LODs (source models) on a static mesh. -1 if invalid. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static int32 GetStaticMeshLODCount(class UStaticMesh* Mesh);

	/** Per-LOD screen size for a static mesh. Returns -1 if out of range / invalid. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static float GetStaticMeshLODScreenSize(class UStaticMesh* Mesh, int32 LODIndex);

	/** Runtime LOD count from RenderData (the chain actually used at runtime; differs from
	 *  source model count when LODs are auto-built or stripped). -1 if invalid. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static int32 GetStaticMeshRuntimeLODCount(class UStaticMesh* Mesh);

	/** Triangle count of a given runtime LOD. -1 on miss. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static int32 GetStaticMeshNumTriangles(class UStaticMesh* Mesh, int32 LODIndex);

	/** Lightmap resolution for a static mesh. 0 if disabled, -1 if invalid. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static int32 GetStaticMeshLightmapResolution(class UStaticMesh* Mesh);

	/** Material slot count on a static mesh. -1 if invalid. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static int32 GetStaticMeshMaterialCount(class UStaticMesh* Mesh);

	/** Whether Nanite is enabled on a static mesh. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static bool GetStaticMeshNaniteEnabled(class UStaticMesh* Mesh);

	/** Estimated Nanite fallback triangle percent. -1 if Nanite disabled / invalid. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static float GetStaticMeshNaniteFallbackPercent(class UStaticMesh* Mesh);

	// -- Material --

	/** Top-level material expressions. UMaterial::Expressions is protected. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static TArray<class UMaterialExpression*> GetMaterialExpressions(class UMaterial* Material);

	// -- Niagara --

	/** Number of emitter handles on a Niagara system. -1 if invalid. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static int32 GetNiagaraEmitterCount(class UNiagaraSystem* System);

	/** Name of an emitter at the given index, empty on miss. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static FString GetNiagaraEmitterName(class UNiagaraSystem* System, int32 Index);

	/** Whether the emitter at index is enabled. False on miss. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static bool GetNiagaraEmitterEnabled(class UNiagaraSystem* System, int32 Index);

	// -- Skeletal --

	/** All sockets defined on a skeleton. USkeleton::Sockets is protected. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static TArray<class USkeletalMeshSocket*> GetSkeletonSockets(class USkeleton* Skeleton);

	/** Number of material slots on a skeletal mesh. -1 if invalid. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static int32 GetSkeletalMeshMaterialCount(class USkeletalMesh* Mesh);

	// -- Sequencer (MovieScene) --

	/** Number of tracks on a UMovieScene. -1 if invalid. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static int32 GetMovieSceneTrackCount(class UMovieScene* MovieScene);

	/** Class name of the track at index, empty on miss. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static FString GetMovieSceneTrackClass(class UMovieScene* MovieScene, int32 Index);

	/** Number of object bindings on a UMovieScene. -1 if invalid. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static int32 GetMovieSceneBindingCount(class UMovieScene* MovieScene);

	/** Display name of the object binding at index. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static FString GetMovieSceneBindingName(class UMovieScene* MovieScene, int32 Index);

	// -- UMG --

	/** Root widget of a Widget Blueprint's tree. nullptr on miss. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static class UWidget* GetWidgetBlueprintRoot(class UWidgetBlueprint* WidgetBP);

	// -- Blueprint internals --

	/** All function graphs on a Blueprint (including ubergraph pages excluded). */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static TArray<class UEdGraph*> GetBlueprintFunctionGraphsArr(class UBlueprint* Blueprint);

	/** All ubergraph pages on a Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static TArray<class UEdGraph*> GetBlueprintUbergraphPagesArr(class UBlueprint* Blueprint);

	/** All Simple Construction Script nodes (component templates). */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static TArray<class USCS_Node*> GetBlueprintSCSNodes(class UBlueprint* Blueprint);

	// -- Blackboard --

	/** Number of keys on a Blackboard asset. -1 if invalid. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static int32 GetBlackboardKeyCount(class UBlackboardData* Blackboard);

	/** Name of the key at index. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static FString GetBlackboardKeyName(class UBlackboardData* Blackboard, int32 Index);

	/** Type-class name of the key at index (e.g. BlackboardKeyType_Bool). */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static FString GetBlackboardKeyType(class UBlackboardData* Blackboard, int32 Index);

	// -- Input Mapping --

	/** Number of mappings on an Input Mapping Context. -1 if invalid. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static int32 GetInputContextMappingCount(class UInputMappingContext* Context);

	/** Action asset path of the mapping at index. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static FString GetInputContextMappingAction(class UInputMappingContext* Context, int32 Index);

	/** Key (e.g. "SpaceBar") of the mapping at index. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static FString GetInputContextMappingKey(class UInputMappingContext* Context, int32 Index);

	// -- Landscape --

	/** Number of editor layer settings on a landscape proxy. -1 if invalid. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static int32 GetLandscapeEditorLayerCount(class ALandscapeProxy* Landscape);

	/** Name of the editor layer at index. */
	UFUNCTION(BlueprintCallable, Category = "BobBot|Inspection")
	static FString GetLandscapeEditorLayerName(class ALandscapeProxy* Landscape, int32 Index);

private:
	static bool ResolveVarType(const FString& VarType, struct FEdGraphPinType& OutPinType);
	static TMap<FString, FEdGraphPinType> TypeCache;
};
