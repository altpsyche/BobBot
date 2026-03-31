// BobBot - Model Context Protocol AI Tool for Unreal Engine

#include "Widgets/SBobBotInfoTab.h"
#include "BobBotConstants.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "BobBotInfoTab"

// --------------------------------------------------------------------------- //
// Tool catalog data
// --------------------------------------------------------------------------- //
struct FToolEntry
{
	const TCHAR* Name;
	const TCHAR* Params;
};

struct FToolCategory
{
	const TCHAR* Name;
	TArray<FToolEntry> Tools;
};

// clang-format off
static const FToolCategory GToolCategories[] =
{
	{ TEXT("Actors"), {
		{ TEXT("get_selected_actors"), TEXT("") },
		{ TEXT("get_level_actors"), TEXT("class_filter") },
		{ TEXT("spawn_actor"), TEXT("class_path, x, y, z, yaw, pitch, roll") },
		{ TEXT("delete_selected_actors"), TEXT("") },
		{ TEXT("get_actor_properties"), TEXT("actor_label") },
		{ TEXT("set_actor_property"), TEXT("actor_label, property_name, value") },
	}},
	{ TEXT("Assets"), {
		{ TEXT("search_assets"), TEXT("path, name_filter, type_filter") },
		{ TEXT("get_asset_info"), TEXT("asset_path") },
		{ TEXT("create_blueprint"), TEXT("name, parent_class, path") },
		{ TEXT("create_material"), TEXT("name, path") },
	}},
	{ TEXT("Asset Operations"), {
		{ TEXT("rename_asset"), TEXT("old_path, new_path") },
		{ TEXT("duplicate_asset"), TEXT("source_path, dest_path") },
		{ TEXT("delete_asset"), TEXT("asset_path") },
		{ TEXT("move_asset"), TEXT("source_path, dest_folder") },
		{ TEXT("get_asset_references"), TEXT("asset_path") },
		{ TEXT("get_asset_dependencies"), TEXT("asset_path") },
	}},
	{ TEXT("Materials"), {
		{ TEXT("add_material_expression"), TEXT("material_path, expression_type, x, y") },
		{ TEXT("connect_material_to_property"), TEXT("material_path, expression_name, output_name, property_name") },
		{ TEXT("get_material_expressions"), TEXT("material_path") },
	}},
	{ TEXT("Levels"), {
		{ TEXT("get_current_level"), TEXT("") },
		{ TEXT("open_level"), TEXT("level_path") },
		{ TEXT("save_current_level"), TEXT("") },
	}},
	{ TEXT("Level Streaming"), {
		{ TEXT("add_streaming_level"), TEXT("level_path") },
		{ TEXT("remove_streaming_level"), TEXT("level_path") },
		{ TEXT("get_streaming_levels"), TEXT("") },
	}},
	{ TEXT("Viewport"), {
		{ TEXT("capture_viewport"), TEXT("filename, width, height") },
		{ TEXT("run_console_command"), TEXT("command") },
		{ TEXT("get_output_log"), TEXT("lines") },
	}},
	{ TEXT("Context"), {
		{ TEXT("get_project_info"), TEXT("") },
		{ TEXT("get_editor_state"), TEXT("") },
	}},
	{ TEXT("Core"), {
		{ TEXT("execute_unreal_python"), TEXT("code") },
		{ TEXT("ping_unreal"), TEXT("") },
	}},
	{ TEXT("Editor Operations"), {
		{ TEXT("select_actors"), TEXT("actor_labels") },
		{ TEXT("deselect_all"), TEXT("") },
		{ TEXT("undo"), TEXT("") },
		{ TEXT("redo"), TEXT("") },
		{ TEXT("focus_on_actor"), TEXT("actor_label") },
		{ TEXT("set_actor_label"), TEXT("actor_label, new_label") },
		{ TEXT("set_actor_folder"), TEXT("actor_label, folder_path") },
		{ TEXT("get_editor_selection"), TEXT("") },
	}},
	{ TEXT("Tags & Layers"), {
		{ TEXT("set_actor_tags"), TEXT("actor_label, tags") },
		{ TEXT("get_actors_by_tag"), TEXT("tag") },
		{ TEXT("set_actor_layer"), TEXT("actor_label, layer_name") },
		{ TEXT("get_actor_tags"), TEXT("actor_label") },
	}},
	{ TEXT("Components"), {
		{ TEXT("add_component_to_actor"), TEXT("actor_label, component_type, component_name") },
		{ TEXT("remove_component"), TEXT("actor_label, component_name") },
		{ TEXT("get_component_properties"), TEXT("actor_label, component_name") },
		{ TEXT("set_component_property"), TEXT("actor_label, component_name, property_name, value") },
	}},
	{ TEXT("World"), {
		{ TEXT("get_world_settings"), TEXT("") },
		{ TEXT("set_world_setting"), TEXT("property_name, value") },
		{ TEXT("get_game_mode"), TEXT("") },
		{ TEXT("set_game_mode"), TEXT("game_mode_path") },
	}},
	{ TEXT("Collision"), {
		{ TEXT("set_collision_preset"), TEXT("actor_label, preset_name") },
		{ TEXT("set_collision_enabled"), TEXT("actor_label, enabled") },
		{ TEXT("get_collision_info"), TEXT("actor_label") },
	}},
	{ TEXT("Physics"), {
		{ TEXT("set_simulate_physics"), TEXT("actor_label, enabled") },
		{ TEXT("set_collision_channel"), TEXT("actor_label, channel") },
		{ TEXT("get_physics_info"), TEXT("actor_label") },
		{ TEXT("set_physics_properties"), TEXT("actor_label, mass, linear_damping, angular_damping") },
	}},
	{ TEXT("Lighting"), {
		{ TEXT("create_light"), TEXT("light_type, x, y, z, intensity, color") },
		{ TEXT("set_light_properties"), TEXT("actor_label, intensity, color, temperature, attenuation_radius") },
		{ TEXT("get_all_lights"), TEXT("") },
		{ TEXT("set_lightmap_resolution"), TEXT("actor_label, resolution") },
		{ TEXT("create_sky_atmosphere"), TEXT("") },
	}},
	{ TEXT("Camera"), {
		{ TEXT("create_camera"), TEXT("x, y, z, yaw, pitch, fov") },
		{ TEXT("set_camera_properties"), TEXT("actor_label, fov, aperture, focus_distance") },
		{ TEXT("get_active_viewport_camera"), TEXT("") },
		{ TEXT("set_viewport_camera"), TEXT("x, y, z, yaw, pitch") },
	}},
	{ TEXT("Texture & Mesh"), {
		{ TEXT("get_static_mesh_info"), TEXT("mesh_path") },
		{ TEXT("set_static_mesh_on_actor"), TEXT("actor_label, mesh_path") },
		{ TEXT("get_texture_info"), TEXT("texture_path") },
		{ TEXT("create_texture_from_file"), TEXT("file_path, name, dest_path") },
		{ TEXT("set_material_texture_parameter"), TEXT("material_path, param_name, texture_path") },
	}},
	{ TEXT("Import/Export"), {
		{ TEXT("import_asset"), TEXT("file_path, destination_path") },
		{ TEXT("export_asset"), TEXT("asset_path, file_path") },
		{ TEXT("import_fbx"), TEXT("file_path, destination_path, import_animations") },
	}},
	{ TEXT("Post-Process"), {
		{ TEXT("create_post_process_volume"), TEXT("x, y, z, infinite_extent") },
		{ TEXT("set_post_process_setting"), TEXT("actor_label, setting, value") },
		{ TEXT("get_post_process_settings"), TEXT("actor_label") },
		{ TEXT("set_color_grading"), TEXT("actor_label, saturation, contrast, gain, offset") },
		{ TEXT("get_rendering_stats"), TEXT("") },
	}},
	{ TEXT("Splines"), {
		{ TEXT("create_spline_actor"), TEXT("points, closed") },
		{ TEXT("get_spline_info"), TEXT("actor_label") },
		{ TEXT("add_spline_point"), TEXT("actor_label, x, y, z, index") },
		{ TEXT("set_spline_mesh"), TEXT("actor_label, mesh_path") },
	}},
	{ TEXT("Data Tables"), {
		{ TEXT("create_data_table"), TEXT("name, struct_path, path") },
		{ TEXT("add_data_table_row"), TEXT("table_path, row_name, values_json") },
		{ TEXT("get_data_table_rows"), TEXT("table_path") },
	}},
	{ TEXT("Skeletal Mesh"), {
		{ TEXT("get_skeleton_info"), TEXT("skeleton_path") },
		{ TEXT("get_skeletal_mesh_info"), TEXT("mesh_path") },
		{ TEXT("create_socket"), TEXT("skeleton_path, bone_name, socket_name") },
		{ TEXT("attach_actor_to_socket"), TEXT("actor_label, parent_label, socket_name") },
	}},
	{ TEXT("Sequencer"), {
		{ TEXT("create_sequence"), TEXT("name, path") },
		{ TEXT("get_sequence_info"), TEXT("sequence_path") },
		{ TEXT("add_actor_to_sequence"), TEXT("sequence_path, actor_label") },
		{ TEXT("set_sequence_length"), TEXT("sequence_path, end_frame") },
		{ TEXT("play_sequence"), TEXT("sequence_path") },
	}},
	{ TEXT("Animation"), {
		{ TEXT("create_anim_blueprint"), TEXT("name, skeleton_path, path") },
		{ TEXT("create_anim_montage"), TEXT("name, animation_path, path") },
		{ TEXT("create_blend_space_1d"), TEXT("name, skeleton_path, path") },
		{ TEXT("get_skeleton_animations"), TEXT("skeleton_path") },
		{ TEXT("get_anim_blueprint_info"), TEXT("anim_bp_path") },
	}},
	{ TEXT("Blueprint Advanced"), {
		{ TEXT("create_blueprint_function"), TEXT("blueprint_path, function_name, inputs, outputs") },
		{ TEXT("create_blueprint_event"), TEXT("blueprint_path, event_name") },
		{ TEXT("get_blueprint_functions"), TEXT("blueprint_path") },
		{ TEXT("get_blueprint_components"), TEXT("blueprint_path") },
		{ TEXT("set_blueprint_parent_class"), TEXT("blueprint_path, parent_class") },
		{ TEXT("create_blueprint_interface"), TEXT("name, path, functions") },
	}},
	{ TEXT("Enhanced Input"), {
		{ TEXT("create_input_action"), TEXT("name, value_type, path") },
		{ TEXT("create_input_mapping_context"), TEXT("name, path") },
		{ TEXT("add_input_mapping"), TEXT("context_path, action_path, key_name") },
		{ TEXT("get_input_actions"), TEXT("path") },
	}},
	{ TEXT("Audio"), {
		{ TEXT("create_sound_cue"), TEXT("name, sound_wave_path, path") },
		{ TEXT("get_audio_assets"), TEXT("path") },
		{ TEXT("set_actor_audio"), TEXT("actor_label, sound_path") },
	}},
	{ TEXT("Landscape"), {
		{ TEXT("get_landscape_info"), TEXT("") },
		{ TEXT("set_landscape_material"), TEXT("material_path") },
		{ TEXT("get_landscape_layers"), TEXT("") },
	}},
	{ TEXT("Foliage"), {
		{ TEXT("get_foliage_types"), TEXT("") },
		{ TEXT("add_foliage_type"), TEXT("static_mesh_path") },
		{ TEXT("get_foliage_stats"), TEXT("") },
	}},
	{ TEXT("Niagara/VFX"), {
		{ TEXT("create_niagara_system"), TEXT("name, path") },
		{ TEXT("get_niagara_info"), TEXT("system_path") },
		{ TEXT("set_niagara_parameter"), TEXT("system_path, param_name, value") },
	}},
	{ TEXT("AI/Behavior"), {
		{ TEXT("create_behavior_tree"), TEXT("name, path") },
		{ TEXT("create_blackboard"), TEXT("name, path") },
		{ TEXT("add_blackboard_key"), TEXT("blackboard_path, key_name, key_type") },
		{ TEXT("get_blackboard_keys"), TEXT("blackboard_path") },
		{ TEXT("create_environment_query"), TEXT("name, path") },
		{ TEXT("get_ai_assets"), TEXT("path") },
	}},
	{ TEXT("PCG"), {
		{ TEXT("create_pcg_graph"), TEXT("name, path") },
		{ TEXT("get_pcg_graph_info"), TEXT("graph_path") },
		{ TEXT("execute_pcg_graph"), TEXT("actor_label") },
		{ TEXT("get_pcg_volumes"), TEXT("") },
	}},
	{ TEXT("UMG/Widgets"), {
		{ TEXT("create_widget_blueprint"), TEXT("name, parent_class, path") },
		{ TEXT("get_widget_tree"), TEXT("widget_path") },
		{ TEXT("create_widget_component"), TEXT("actor_label, widget_path") },
		{ TEXT("get_all_widget_blueprints"), TEXT("path") },
	}},
	{ TEXT("PIE Runtime"), {
		{ TEXT("start_pie"), TEXT("") },
		{ TEXT("stop_pie"), TEXT("") },
		{ TEXT("is_pie_running"), TEXT("") },
		{ TEXT("get_pie_actors"), TEXT("class_filter") },
		{ TEXT("execute_pie_console_command"), TEXT("command") },
	}},
	{ TEXT("Source Control"), {
		{ TEXT("get_source_control_status"), TEXT("asset_path") },
		{ TEXT("check_out_asset"), TEXT("asset_path") },
		{ TEXT("check_in_asset"), TEXT("asset_path, description") },
		{ TEXT("revert_asset"), TEXT("asset_path") },
	}},
	{ TEXT("Build"), {
		{ TEXT("build_lighting"), TEXT("quality") },
		{ TEXT("compile_blueprints"), TEXT("") },
		{ TEXT("validate_assets"), TEXT("path") },
		{ TEXT("get_map_check_errors"), TEXT("") },
	}},
	{ TEXT("Movie Render"), {
		{ TEXT("create_render_job"), TEXT("sequence_path, output_dir, format, resolution") },
		{ TEXT("get_render_queue_status"), TEXT("") },
		{ TEXT("render_sequence_to_images"), TEXT("sequence_path, output_dir, format") },
	}},
	{ TEXT("Debug/Profiling"), {
		{ TEXT("get_frame_stats"), TEXT("") },
		{ TEXT("get_memory_stats"), TEXT("") },
		{ TEXT("get_gpu_stats"), TEXT("") },
		{ TEXT("benchmark_scene"), TEXT("duration_seconds") },
	}},
};
// clang-format on

static const int32 GNumCategories = UE_ARRAY_COUNT(GToolCategories);

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
};

static const int32 GNumApiGroups = UE_ARRAY_COUNT(GApiGroups);

/** Compute the total tool count once at static init. */
static int32 ComputeTotalTools()
{
	int32 N = 0;
	for (int32 i = 0; i < GNumCategories; ++i)
		N += GToolCategories[i].Tools.Num();
	return N;
}
static const int32 GTotalTools = ComputeTotalTools();

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

/** Dark code-block card used for command cards, tool bodies, and API groups. */
static TSharedRef<SBorder> MakeCard(TSharedRef<SWidget> Content, FMargin InnerPadding = FMargin(10, 6))
{
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
		.BorderBackgroundColor(BobBot::Colors::CodeBlockBg)
		.Padding(InnerPadding)
		[
			Content
		];
}

/** Small stat badge for the About section. */
static TSharedRef<SWidget> MakeStatBadge(const FText& Number, const FText& Label)
{
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
		.BorderBackgroundColor(FLinearColor(0.06f, 0.06f, 0.06f, 1.f))
		.Padding(FMargin(12, 8))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(Number)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
				.ColorAndOpacity(FSlateColor(BobBot::Colors::BotGreen))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 2, 0, 0)
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
			]
		];
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
// --------------------------------------------------------------------------- //
TSharedRef<SWidget> SBobBotInfoTab::BuildSlashCommandsSection()
{
	struct FCmd { const TCHAR* Name; const TCHAR* Desc; };
	static const FCmd Commands[] = {
		{ TEXT("/clear"),        TEXT("Clear current chat session") },
		{ TEXT("/cost"),         TEXT("Show session cost, messages, and model") },
		{ TEXT("/model [name]"), TEXT("Switch model (sonnet / opus / haiku)") },
		{ TEXT("/new"),          TEXT("Start a new conversation") },
		{ TEXT("/chats"),        TEXT("List all saved conversations") },
		{ TEXT("/help"),         TEXT("Show available commands") },
	};

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight()
		[
			BobBot::UI::SectionHeading(LOCTEXT("CmdHeading", "SLASH COMMANDS"))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 8)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CmdDesc", "Type these in the chat input."))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
		];

	for (const FCmd& Cmd : Commands)
	{
		Box->AddSlot().AutoHeight().Padding(0, 2)
		[
			MakeCard(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Cmd.Name))
					.Font(FCoreStyle::GetDefaultFontStyle("Mono", 10))
					.ColorAndOpacity(FSlateColor(BobBot::Colors::Blue))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Cmd.Desc))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
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
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight()
		[
			BobBot::UI::SectionHeading(
				FText::Format(LOCTEXT("ToolsHeading", "TOOLS ({0})"), FText::AsNumber(GTotalTools)))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 8)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("ToolsDesc", "{0} tools across {1} categories. Claude selects the right tool automatically."),
				FText::AsNumber(GTotalTools), FText::AsNumber(GNumCategories)))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.ColorAndOpacity(FSlateColor(BobBot::Colors::DimGray))
			.AutoWrapText(true)
		];

	for (int32 i = 0; i < GNumCategories; ++i)
	{
		const FToolCategory& Cat = GToolCategories[i];

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
					.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
					.ColorAndOpacity(FSlateColor(BobBot::Colors::CodeText))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock)
					.Text(bHasParams
						? FText::FromString(FString::Printf(TEXT("(%s)"), Tool.Params))
						: FText::GetEmpty())
					.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
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
			.BorderBackgroundColor(FLinearColor(0.04f, 0.04f, 0.04f, 1.f))
			.HeaderPadding(FMargin(6, 4))
			.Padding(FMargin(0))
			.HeaderContent()
			[
				SNew(STextBlock)
				.Text(HeaderText)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
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
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
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
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FSlateColor(BobBot::Colors::Blue))
			];

		for (const TCHAR* Method : Group.Methods)
		{
			MethodList->AddSlot().AutoHeight().Padding(0, 1)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Method))
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
				.ColorAndOpacity(FSlateColor(BobBot::Colors::CodeText))
			];
		}

		Box->AddSlot().AutoHeight().Padding(0, 3)
		[
			MakeCard(MethodList, FMargin(10, 8))
		];
	}

	return Box;
}

// --------------------------------------------------------------------------- //
// Section 4: About — branded header with stat badges
// --------------------------------------------------------------------------- //
TSharedRef<SWidget> SBobBotInfoTab::BuildAboutSection()
{
	int32 TotalTools = GTotalTools;
	int32 TotalApiMethods = GTotalApiMethods;

	return SNew(SVerticalBox)

		// Plugin name — large green
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PluginName", "BobBot"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
			.ColorAndOpacity(FSlateColor(BobBot::Colors::BotGreen))
		]

		// Subtitle
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 12)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PluginSubtitle", "MCP AI Tool for Unreal Engine"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
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
				MakeStatBadge(FText::AsNumber(GNumCategories), LOCTEXT("StatCatsLabel", "Categories"))
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
				FText::FromString(TEXT("2.0 Beta")))
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
