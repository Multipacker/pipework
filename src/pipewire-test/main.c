#include "src/base/base_include.h"
#include "src/graphics/graphics_include.h"
#include "src/render/render_include.h"
#include "src/font/font_include.h"
#include "src/font_cache/font_cache_include.h"
#include "src/draw/draw_include.h"
#include "src/ui/ui_include.h"
#include "src/nat/nat_include.h"

#include "src/base/base_include.c"
#include "src/graphics/graphics_include.c"
#include "src/render/render_include.c"
#include "src/font/font_include.c"
#include "src/font_cache/font_cache_include.c"
#include "src/draw/draw_include.c"
#include "src/ui/ui_include.c"
#include "src/nat/nat_include.c"

#include "pipewire.h"
#include "pipewire.c"

#include "core.h"
#include "core.c"

internal S32 os_run(Str8List arguments) {
    Arena *arena = arena_create();
    state = arena_push_struct(arena, State);
    state->arena = arena;
    state->command_arena = arena_create();
    state->drag_arena = arena_create();

    for (U64 i = 0; i < array_count(state->frame_arenas); ++i) {
        state->frame_arenas[i] = arena_create();
    }

    state->font_size = 11.0f;
    state->frames_to_render = 4;

    {
        Theme *theme = &state->theme;
        theme->name = str8_literal("OpenColor Dark");

        V4F32 gray0 = color_from_srgba_u32(0xf8f9faff);
        V4F32 gray1 = color_from_srgba_u32(0xf1f3f5ff);
        V4F32 gray2 = color_from_srgba_u32(0xe9ecefff);
        V4F32 gray3 = color_from_srgba_u32(0xdee2e6ff);
        V4F32 gray4 = color_from_srgba_u32(0xced4daff);
        V4F32 gray5 = color_from_srgba_u32(0xadb5bdff);
        V4F32 gray6 = color_from_srgba_u32(0x868e96ff);
        V4F32 gray7 = color_from_srgba_u32(0x495057ff);
        V4F32 gray8 = color_from_srgba_u32(0x343a40ff);
        V4F32 gray9 = color_from_srgba_u32(0x212529ff);

        V4F32 red6 = color_from_srgba_u32(0xfa5252ff);

        V4F32 pink6 = color_from_srgba_u32(0xe64980ff);

        V4F32 grape6 = color_from_srgba_u32(0xbe4bdbff);

        V4F32 violet6 = color_from_srgba_u32(0x7950f2ff);

        V4F32 indigo6 = color_from_srgba_u32(0x4c6ef5ff);

        V4F32 blue6 = color_from_srgba_u32(0x228be6ff);

        V4F32 cyan6 = color_from_srgba_u32(0x15aabfff);

        V4F32 teal6 = color_from_srgba_u32(0x12b886ff);

        V4F32 green6 = color_from_srgba_u32(0x40c057ff);

        V4F32 lime6 = color_from_srgba_u32(0x82c91eff);

        V4F32 yellow6 = color_from_srgba_u32(0xfab005ff);

        V4F32 orange4 = color_from_srgba_u32(0xffa94dff);

        theme->text  = gray0;
        theme->weak_text = gray5;
        theme->hover = gray4;
        theme->cursor = orange4;
        theme->selection = gray1;
        theme->selection.a = 0.3f;
        theme->focus = orange4;

        theme->disabled_overlay         = gray6;
        theme->disabled_overlay.a       = 0.5f;
        theme->drop_site_overlay        = gray7;
        theme->drop_site_overlay.a      = 0.5f;
        theme->inactive_panel_overlay   = v4f32(0.0f, 0.0f, 0.0f, 0.8f);
        theme->inactive_panel_overlay.a = 0.5f;
        theme->drop_shadow              = v4f32(0.0f, 0.0f, 0.0f, 0.8f);

        theme->base_background         = gray9;
        theme->base_border             = gray8;
        theme->title_bar_background    = gray8;
        theme->title_bar_border        = gray7;
        theme->tab_background          = gray6;
        theme->tab_border              = gray5;
        theme->inactive_tab_background = gray7;
        theme->inactive_tab_border     = gray6;
        theme->button_background       = gray8;
        theme->button_border           = gray7;
        theme->secondary_button_background = gray7;
        theme->secondary_button_border     = gray6;

        theme->port_unknown     = gray0;
        theme->port_audio       = blue6;
        theme->port_video       = yellow6;
        theme->port_image       = green6;
        theme->port_binary      = grape6;
        theme->port_stream      = red6;
        theme->port_application = pink6;
    }

    Log *log = log_create();
    log_select(log);

    gfx_init();
    render_init();

    Window *window = create_window(str8_literal("Pipewire-test"), 1280, 720);
    Panel *left   = create_panel();
    Panel *middle = create_panel();
    Panel *right  = create_panel();
    left->percentage_of_parent   = 0.2f;
    middle->percentage_of_parent = 0.6f;
    right->percentage_of_parent  = 0.2f;
    insert_panel(window->root_panel, &nil_panel, left);
    insert_panel(window->root_panel, left, middle);
    insert_panel(window->root_panel, middle, right);

    Tab *tab0 = create_tab(str8_literal("Object list"));
    Tab *tab1 = create_tab(str8_literal("Graph"));
    Tab *tab2 = create_tab(str8_literal("Properties"));
    Tab *tab3 = create_tab(str8_literal("Parameters"));
    Tab *tab4 = create_tab(str8_literal("Volume"));
    tab0->build = build_list_tab;
    tab1->build = build_graph_tab;
    tab2->build = build_property_tab;
    tab3->build = build_parameter_tab;
    tab4->build = build_volume_tab;
    insert_tab(left,   &nil_tab, tab0);
    insert_tab(middle, &nil_tab, tab1);
    insert_tab(right,  &nil_tab, tab2);
    insert_tab(right,  tab2,     tab3);
    insert_tab(right,  tab3,     tab4);

    window->active_panel = handle_from_panel(middle);

    font_cache_create();
    gfx_set_update_function(update);

    pipewire_init();

    while (state->first_window) {
        update();
    }

    pipewire_deinit();

    return 0;
}
