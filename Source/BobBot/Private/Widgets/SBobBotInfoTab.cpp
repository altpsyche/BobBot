// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "Widgets/SBobBotInfoTab.h"
#include "BobBotChatController.h"
#include "BobBotConstants.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "BobBotInfoTab"

// --------------------------------------------------------------------------- //
// Tool catalog data
//
// The tool list is no longer hand-maintained here. The Python bridge writes
// `<ProjectRoot>/Saved/BobBot/.tool_manifest.json` at startup from its
// `_TOOL_REGISTRY`; this widget reads that file and builds the UI from it.
// If the manifest is missing, the tab shows a "manifest not found — restart
// the bridge" message and lists nothing.
// --------------------------------------------------------------------------- //
struct FToolEntry
{
	FString Name;
	FString Params;
};

struct FToolCategory
{
	FString Name;
	TArray<FToolEntry> Tools;
};

static FString GetToolManifestPath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BobBot"), TEXT(".tool_manifest.json"));
}

static TArray<FToolCategory> LoadToolManifest()
{
	TArray<FToolCategory> Categories;
	const FString ManifestPath = GetToolManifestPath();

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ManifestPath))
	{
		return Categories;
	}

	TSharedPtr<FJsonValue> RootVal;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootVal) || !RootVal.IsValid())
	{
		return Categories;
	}

	const TArray<TSharedPtr<FJsonValue>>* RootArr = nullptr;
	if (!RootVal->TryGetArray(RootArr))
	{
		return Categories;
	}

	TMap<FString, int32> CatByName;
	for (const TSharedPtr<FJsonValue>& Val : *RootArr)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Val.IsValid() || !Val->TryGetObject(Obj) || !Obj->IsValid())
		{
			continue;
		}
		FString CatName, ToolName;
		(*Obj)->TryGetStringField(TEXT("category"), CatName);
		(*Obj)->TryGetStringField(TEXT("name"), ToolName);
		if (CatName.IsEmpty() || ToolName.IsEmpty())
		{
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* ParamsArr = nullptr;
		FString ParamsStr;
		if ((*Obj)->TryGetArrayField(TEXT("params"), ParamsArr) && ParamsArr)
		{
			for (const TSharedPtr<FJsonValue>& PV : *ParamsArr)
			{
				FString P;
				if (PV.IsValid() && PV->TryGetString(P))
				{
					if (!ParamsStr.IsEmpty()) { ParamsStr += TEXT(", "); }
					ParamsStr += P;
				}
			}
		}

		int32 Idx;
		if (int32* Found = CatByName.Find(CatName))
		{
			Idx = *Found;
		}
		else
		{
			Idx = Categories.Add({ CatName, {} });
			CatByName.Add(CatName, Idx);
		}
		Categories[Idx].Tools.Add({ ToolName, ParamsStr });
	}

	Categories.Sort([](const FToolCategory& A, const FToolCategory& B) { return A.Name < B.Name; });
	for (FToolCategory& C : Categories)
	{
		C.Tools.Sort([](const FToolEntry& A, const FToolEntry& B) { return A.Name < B.Name; });
	}

	return Categories;
}

// (Static GToolCategories[] removed; tool catalog now loaded at runtime via LoadToolManifest.)


// --------------------------------------------------------------------------- //
// BobBotLib API data (file-scope so both Tools and About sections can use it)
// --------------------------------------------------------------------------- //
struct FApiGroup
{
	const TCHAR* Name;
	TArray<const TCHAR*> Methods;
};

static const FApiGroup GApiGroups[] = {
	{ TEXT("Variables"), {
		TEXT("add_blueprint_variable(bp, name, type, editable)"),
		TEXT("remove_blueprint_variable(bp, name)"),
		TEXT("set_blueprint_variable_default(bp, name, value)"),
		TEXT("set_blueprint_variable_category(bp, name, category)"),
		TEXT("set_blueprint_variable_tooltip(bp, name, tooltip)"),
		TEXT("set_blueprint_variable_slider_range(bp, name, min, max)"),
		TEXT("get_blueprint_variable_names(bp)"),
	}},
	{ TEXT("Components"), {
		TEXT("add_component_to_blueprint(bp, component_class, name)"),
	}},
	{ TEXT("Graph"), {
		TEXT("add_function_graph(bp, function_name)"),
		TEXT("add_custom_event(bp, event_name)"),
		TEXT("add_function_call_node(bp, func_name, target_class, x, y)"),
		TEXT("add_set_mpc_scalar_node(bp, mpc_path, param_name, x, y)"),
		TEXT("add_set_mpc_vector_node(bp, mpc_path, param_name, x, y)"),
	}},
	{ TEXT("Compilation"), {
		TEXT("compile_blueprint(bp)"),
		TEXT("compile_blueprint_with_status(bp)"),
	}},
	{ TEXT("Utility"), {
		TEXT("get_blueprint_cdo(bp)"),
		TEXT("set_cdo_property(bp, property_name, value)"),
		TEXT("clear_type_cache()"),
	}},
	{ TEXT("Reflection"), {
		TEXT("read_property_as_string(obj, name) — bypasses BlueprintReadable"),
		TEXT("read_object_array_property(obj, name) — TArray<UObject*> reader"),
	}},
	{ TEXT("Inspection"), {
		TEXT("get_world_streaming_levels(world)"),
		TEXT("get_static_mesh_lod_count(mesh)"),
		TEXT("get_static_mesh_lod_screen_size(mesh, idx)"),
		TEXT("get_static_mesh_runtime_lod_count(mesh)"),
		TEXT("get_static_mesh_num_triangles(mesh, lod)"),
		TEXT("get_static_mesh_lightmap_resolution(mesh)"),
		TEXT("get_static_mesh_material_count(mesh)"),
		TEXT("get_static_mesh_nanite_enabled(mesh)"),
		TEXT("get_static_mesh_nanite_fallback_percent(mesh)"),
		TEXT("get_material_expressions(material)"),
		TEXT("get_niagara_emitter_count(system)"),
		TEXT("get_niagara_emitter_name(system, idx)"),
		TEXT("get_niagara_emitter_enabled(system, idx)"),
		TEXT("get_skeleton_sockets(skeleton)"),
		TEXT("get_skeletal_mesh_material_count(mesh)"),
		TEXT("get_movie_scene_track_count(ms)"),
		TEXT("get_movie_scene_track_class(ms, idx)"),
		TEXT("get_movie_scene_binding_count(ms)"),
		TEXT("get_movie_scene_binding_name(ms, idx)"),
		TEXT("get_widget_blueprint_root(wbp)"),
		TEXT("get_blueprint_function_graphs_arr(bp)"),
		TEXT("get_blueprint_ubergraph_pages_arr(bp)"),
		TEXT("get_blueprint_scs_nodes(bp)"),
		TEXT("get_blackboard_key_count(bb)"),
		TEXT("get_blackboard_key_name(bb, idx)"),
		TEXT("get_blackboard_key_type(bb, idx)"),
		TEXT("get_input_context_mapping_count(ctx)"),
		TEXT("get_input_context_mapping_action(ctx, idx)"),
		TEXT("get_input_context_mapping_key(ctx, idx)"),
		TEXT("get_landscape_editor_layer_count(ls)"),
		TEXT("get_landscape_editor_layer_name(ls, idx)"),
	}},
};

static const int32 GNumApiGroups = UE_ARRAY_COUNT(GApiGroups);

/** Total tool count from the runtime manifest. Cached on first call. */
static int32 GetTotalTools()
{
	static int32 Cached = -1;
	if (Cached < 0)
	{
		Cached = 0;
		for (const FToolCategory& C : LoadToolManifest())
		{
			Cached += C.Tools.Num();
		}
	}
	return Cached;
}

static int32 GetNumToolCategories()
{
	static int32 Cached = -1;
	if (Cached < 0)
	{
		Cached = LoadToolManifest().Num();
	}
	return Cached;
}

static int32 ComputeTotalApiMethods()
{
	int32 N = 0;
	for (int32 i = 0; i < GNumApiGroups; ++i)
		N += GApiGroups[i].Methods.Num();
	return N;
}
static const int32 GTotalApiMethods = ComputeTotalApiMethods();

// --------------------------------------------------------------------------- //
// Local helpers
// --------------------------------------------------------------------------- //

/** Small stat badge for the About section. */
static TSharedRef<SWidget> MakeStatBadge(const FText& Number, const FText& Label)
{
	return BobBot::UI::Toolbar(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(Number)
			.Font(BobBot::Theme::FontStat())
			.ColorAndOpacity(FSlateColor(BobBot::Colors::BotGreen))
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 2, 0, 0)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(BobBot::Theme::FontCaption())
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
		]
	, FMargin(12, 8));
}

// --------------------------------------------------------------------------- //
// Construction
// --------------------------------------------------------------------------- //
void SBobBotInfoTab::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;

	ChildSlot
	[
		SNew(SScrollBox)

		// ===== SLASH COMMANDS =====
		+ SScrollBox::Slot().Padding(12, 12, 12, 4)
		[
			BuildSlashCommandsSection()
		]

		+ SScrollBox::Slot().Padding(0, 8) [ SNew(SSeparator) ]

		// ===== TOOLS =====
		+ SScrollBox::Slot().Padding(12, 8, 12, 4)
		[
			BuildToolsSection()
		]

		+ SScrollBox::Slot().Padding(0, 8) [ SNew(SSeparator) ]

		// ===== BOBBOT LIB API =====
		+ SScrollBox::Slot().Padding(12, 8, 12, 4)
		[
			BuildBobBotLibSection()
		]

		+ SScrollBox::Slot().Padding(0, 8) [ SNew(SSeparator) ]

		// ===== ABOUT =====
		+ SScrollBox::Slot().Padding(12, 8, 12, 20)
		[
			BuildAboutSection()
		]
	];
}

// --------------------------------------------------------------------------- //
// Section 1: Slash Commands — dark reference cards
//
// Reads directly from FBobBotChatController::GetSlashCommands(). When a new
// command is registered there, it shows up here automatically — no manual
// duplication, no drift.
// --------------------------------------------------------------------------- //
TSharedRef<SWidget> SBobBotInfoTab::BuildSlashCommandsSection()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight()
		[
			BobBot::UI::SectionHeading(LOCTEXT("CmdHeading", "SLASH COMMANDS"))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 8)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CmdDesc", "Type these in the chat input."))
			.Font(BobBot::Theme::FontSmall())
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
		];

	if (Controller == nullptr)
	{
		Box->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CmdNoController", "(Chat controller not available.)"))
			.Font(BobBot::Theme::FontCaption())
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
		];
		return Box;
	}

	for (const FBobBotSlashCommand& Cmd : Controller->GetSlashCommands())
	{
		const FString DisplayName = Cmd.Name + Cmd.UsageSuffix;
		Box->AddSlot().AutoHeight().Padding(0, 2)
		[
			BobBot::UI::Card(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(DisplayName))
					.Font(BobBot::Theme::FontCode())
					.ColorAndOpacity(FSlateColor(BobBot::Colors::Blue))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Cmd.Description))
					.Font(BobBot::Theme::FontCaption())
					.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
				],
				FMargin(10, 6)
			)
		];
	}

	return Box;
}

// --------------------------------------------------------------------------- //
// Section 2: Tools — collapsible categories with dark code bodies
// --------------------------------------------------------------------------- //
TSharedRef<SWidget> SBobBotInfoTab::BuildToolsSection()
{
	const TArray<FToolCategory> Categories = LoadToolManifest();
	int32 TotalTools = 0;
	for (const FToolCategory& C : Categories)
	{
		TotalTools += C.Tools.Num();
	}

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight()
		[
			BobBot::UI::SectionHeading(
				FText::Format(LOCTEXT("ToolsHeading", "TOOLS ({0})"), FText::AsNumber(TotalTools)))
		];

	if (Categories.Num() == 0)
	{
		Box->AddSlot().AutoHeight().Padding(0, 8, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ToolsManifestMissing",
				"Tool manifest not found. Restart the BobBot bridge from the Connect tab — the bridge writes the manifest at startup."))
			.Font(BobBot::Theme::FontSmall())
			.ColorAndOpacity(FSlateColor(BobBot::Colors::Yellow))
			.AutoWrapText(true)
		];
		return Box;
	}

	Box->AddSlot().AutoHeight().Padding(0, 2, 0, 8)
	[
		SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("ToolsDesc", "{0} tools across {1} categories. BobBot selects the right tool automatically."),
			FText::AsNumber(TotalTools), FText::AsNumber(Categories.Num())))
		.Font(BobBot::Theme::FontSmall())
		.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
		.AutoWrapText(true)
	];

	for (const FToolCategory& Cat : Categories)
	{

		// Build tool list inside a dark card
		TSharedRef<SVerticalBox> ToolList = SNew(SVerticalBox);
		for (const FToolEntry& Tool : Cat.Tools)
		{
			FString NameStr(Tool.Name);
			FString ParamsStr(Tool.Params);
			bool bHasParams = ParamsStr.Len() > 0;

			ToolList->AddSlot().AutoHeight().Padding(0, 1)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::FromString(NameStr))
					.Font(BobBot::Theme::FontCode())
					.ColorAndOpacity(FSlateColor(BobBot::Colors::CodeText))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock)
					.Text(bHasParams
						? FText::FromString(FString::Printf(TEXT("(%s)"), *Tool.Params))
						: FText::GetEmpty())
					.Font(BobBot::Theme::FontCode())
					.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
				]
			];
		}

		// Category count badge
		FText HeaderText = FText::Format(LOCTEXT("CatHeader", "{0}  ({1})"),
			FText::FromString(Cat.Name), FText::AsNumber(Cat.Tools.Num()));

		Box->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(true)
			.BorderBackgroundColor(BobBot::Theme::Surface)
			.HeaderPadding(FMargin(6, 4))
			.Padding(FMargin(0))
			.HeaderContent()
			[
				SNew(STextBlock)
				.Text(HeaderText)
				.Font(BobBot::Theme::FontDropdownTitle())
				.ColorAndOpacity(FSlateColor(FLinearColor::White))
			]
			.BodyContent()
			[
				// Left blue accent bar + dark code body
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor(BobBot::Colors::Blue)
					.Padding(FMargin(1.5f, 0))
					[
						SNullWidget::NullWidget
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor(BobBot::Colors::CodeBlockBg)
					.Padding(FMargin(10, 6))
					[
						ToolList
					]
				]
			]
		];
	}

	return Box;
}

// --------------------------------------------------------------------------- //
// Section 3: BobBotLib API — grouped dark cards
// --------------------------------------------------------------------------- //
TSharedRef<SWidget> SBobBotInfoTab::BuildBobBotLibSection()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight()
		[
			BobBot::UI::SectionHeading(LOCTEXT("ApiHeading", "BOBBOT LIB API"))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 8)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ApiDesc", "C++ methods via unreal.BobBotLib inside execute_unreal_python. Fills UE Python API gaps."))
			.Font(BobBot::Theme::FontSmall())
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
			.AutoWrapText(true)
		];

	for (const FApiGroup& Group : GApiGroups)
	{
		// Build method list for this group
		TSharedRef<SVerticalBox> MethodList = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Group.Name))
				.Font(BobBot::Theme::FontDropdownTitle())
				.ColorAndOpacity(FSlateColor(BobBot::Colors::Blue))
			];

		for (const TCHAR* Method : Group.Methods)
		{
			MethodList->AddSlot().AutoHeight().Padding(0, 1)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Method))
				.Font(BobBot::Theme::FontCode())
				.ColorAndOpacity(FSlateColor(BobBot::Colors::CodeText))
			];
		}

		Box->AddSlot().AutoHeight().Padding(0, 3)
		[
			BobBot::UI::Card(MethodList, FMargin(10, 8))
		];
	}

	return Box;
}

// --------------------------------------------------------------------------- //
// Section 4: About — branded header with stat badges
// --------------------------------------------------------------------------- //
TSharedRef<SWidget> SBobBotInfoTab::BuildAboutSection()
{
	int32 TotalTools = GetTotalTools();
	int32 TotalApiMethods = GTotalApiMethods;

	return SNew(SVerticalBox)

		// Plugin name — large green
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PluginName", "BobBot"))
			.Font(BobBot::Theme::FontTitle())
			.ColorAndOpacity(FSlateColor(BobBot::Colors::BotGreen))
		]

		// Subtitle
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 12)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PluginSubtitle", "MCP AI Tool for Unreal Engine"))
			.Font(BobBot::Theme::FontBody())
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
		]

		// Stat badges row (computed dynamically from tool catalog data)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 4, 0)
			[
				MakeStatBadge(FText::AsNumber(TotalTools), LOCTEXT("StatToolsLabel", "Tools"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2, 0)
			[
				MakeStatBadge(FText::AsNumber(GetNumToolCategories()), LOCTEXT("StatCatsLabel", "Categories"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4, 0, 0, 0)
			[
				MakeStatBadge(FText::AsNumber(TotalApiMethods), LOCTEXT("StatApiLabel", "C++ Methods"))
			]
		]

		// Credits
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
		[
			BobBot::UI::KeyValueRow(
				LOCTEXT("VersionKey", "Version:"),
				FText::FromString(TEXT("Beta")))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 2)
		[
			BobBot::UI::KeyValueRow(
				LOCTEXT("AuthorKey", "Author:"),
				FText::FromString(TEXT("Siva Vadlamani")))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 2)
		[
			BobBot::UI::KeyValueRow(
				LOCTEXT("WebKey", "Web:"),
				FText::FromString(TEXT("altpsyche.dev")),
				FSlateColor(BobBot::Colors::Blue))
		];
}

#undef LOCTEXT_NAMESPACE
