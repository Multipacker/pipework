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

// NOTE(simon): Fonts and icons.

#include "generated.h"
embed_file(default_font, "/data/NotoSans-Regular.ttf");
embed_file(icon_font, "/data/fontello/fontello.ttf");
global Str8 icon_kind_text[] = {
    [UI_IconKind_Minimize]   = str8_literal_compile("\uF2D1"),
    [UI_IconKind_Maximize]   = str8_literal_compile("\uF2D0"),
    [UI_IconKind_Close]      = str8_literal_compile("\uE807"),
    [UI_IconKind_Pin]        = str8_literal_compile("\uE809"),
    [UI_IconKind_Eye]        = str8_literal_compile("\uE80A"),
    [UI_IconKind_NoEye]      = str8_literal_compile("\uE80C"),
    [UI_IconKind_LeftArrow]  = str8_literal_compile("\uE801"),
    [UI_IconKind_RightArrow] = str8_literal_compile("\uE800"),
    [UI_IconKind_UpArrow]    = str8_literal_compile("\uE802"),
    [UI_IconKind_DownArrow]  = str8_literal_compile("\uE80B"),
    [UI_IconKind_LeftAngle]  = str8_literal_compile("\uE804"),
    [UI_IconKind_RightAngle] = str8_literal_compile("\uE805"),
    [UI_IconKind_UpAngle]    = str8_literal_compile("\uE806"),
    [UI_IconKind_DownAngle]  = str8_literal_compile("\uE803"),
    [UI_IconKind_Check]      = str8_literal_compile("\uE808"),
    [UI_IconKind_File]       = str8_literal_compile("\uF15B"),
    [UI_IconKind_Folder]     = str8_literal_compile("\uE80D"),
};

// NOTE(simon): Themes.

#define THEME_COLORS                                                                         \
    X(Text,                      text,                        "Text")                        \
    X(WeakText,                  weak_text,                   "Weak text")                   \
    X(Hover,                     hover,                       "Hover")                       \
    X(Cursor,                    cursor,                      "Cursor")                      \
    X(Selection,                 selection,                   "Selection")                   \
    X(Focus,                     focus,                       "Focus")                       \
    X(DropShadow,                drop_shadow,                 "Drop shadow")                 \
    X(DisabledOverlay,           disabled_overlay,            "Disabled overlay")            \
    X(DropSiteOverlay,           drop_site_overlay,           "Drop site overlay")           \
    X(InactivePanelOverlay,      inactive_panel_overlay,      "Inactive panel overlay")      \
    X(BaseBackground,            base_background,             "Base background")             \
    X(BaseBorder,                base_border,                 "Base border")                 \
    X(TitleBarBackground,        title_bar_background,        "Title bar background")        \
    X(TitleBarBorder,            title_bar_border,            "Title bar border")            \
    X(TabBackground,             tab_background,              "Tab background")              \
    X(TabBorder,                 tab_border,                  "Tab border")                  \
    X(InactiveTabBackground,     inactive_tab_background,     "Inactive tab background")     \
    X(InactiveTabBorder,         inactive_tab_border,         "Inactive tab border")         \
    X(ButtonBackground,          button_background,           "Button background")           \
    X(ButtonBorder,              button_border,               "Button border")               \
    X(SecondaryButtonBackground, secondary_button_background, "Secondary button background") \
    X(SecondaryButtonBorder,     secondary_button_border,     "Secondary button border")     \

#define X(name, snake_name, display_name) ThemeColor_##name,
typedef enum {
    THEME_COLORS
    ThemeColor_COUNT,
} ThemeColor;
#undef X

#define X(name, snake_name, display_name) str8_literal_compile(display_name),
global Str8 theme_color_names[] = {
    THEME_COLORS
};
#undef X

#define X(name, snake_name, display_name) V4F32 snake_name;
typedef struct Theme Theme;
struct Theme {
    Str8 name;
    union {
        V4F32 colors[ThemeColor_COUNT];
        struct {
            THEME_COLORS
        };
    };
};
#undef X

typedef enum {
    ThemePalette_Base,
    ThemePalette_TitleBar,
    ThemePalette_Button,
    ThemePalette_SecondaryButton,
    ThemePalette_Tab,
    ThemePalette_InactiveTab,
    ThemePalette_DropSiteOverlay,
    ThemePalette_COUNT,
} ThemePalette;

internal V4F32      color_from_theme(ThemeColor color);
internal UI_Palette palette_from_theme(ThemePalette palette);

#undef global
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wc2y-extensions"
#include <pipewire/pipewire.h>
#pragma clang diagnostic pop
#define global static

#include "pipewire.h"
#include "pipewire.c"

typedef struct State State;
struct State {
    Arena *arena;

    Arena *frame_arenas[2];
    U64 frame_index;

    B32 running;
    U32 frames_to_render;

    Theme theme;
    UI_Palette palettes[ThemePalette_COUNT];

    F32 font_size;

    Gfx_Window     window;
    Render_Window  render;
    UI_Context    *ui;

    Pipewire_Handle selected_object;
    Pipewire_Handle selected_object_next;
    Pipewire_Handle hovered_object;
    Pipewire_Handle hovered_object_next;
};

global State *state;

internal Void request_frame(Void) {
    state->frames_to_render = 4;
}

internal Arena *frame_arena(Void) {
    Arena *result = state->frame_arenas[state->frame_index % array_count(state->frame_arenas)];
    return result;
}

internal V4F32 color_from_theme(ThemeColor color) {
    V4F32 result = state->theme.colors[color];
    return result;
}

internal UI_Palette palette_from_theme(ThemePalette palette) {
    UI_Palette result = state->palettes[palette];
    return result;
}

internal Str8 kind_from_object(Pipewire_Object *object) {
    Str8 result = { 0 };

    switch (object->kind) {
        case Pipewire_Object_Module: {
            result = str8_literal("Module");
        } break;
        case Pipewire_Object_Factory: {
            result = str8_literal("Factory");
        } break;
        case Pipewire_Object_Client: {
            result = str8_literal("Client");
        } break;
        case Pipewire_Object_Device: {
            result = str8_literal("Device");
        } break;
        case Pipewire_Object_Node: {
            result = str8_literal("Node");
        } break;
        case Pipewire_Object_Port: {
            result = str8_literal("Port");
        } break;
        case Pipewire_Object_Link: {
            result = str8_literal("Link");
        } break;
        case Pipewire_Object_COUNT: {
        } break;
    }

    return result;
}

internal Str8 name_from_object(Pipewire_Object *object) {
    Str8 name = { 0 };

    switch (object->kind) {
        case Pipewire_Object_Module: {
            name = pipewire_object_property_string_from_name(object, str8_literal("module.name"));

            if (!name.size) {
                name = str8_format(frame_arena(), "Module %u", object->id);
            }
        } break;
        case Pipewire_Object_Factory: {
            name = pipewire_object_property_string_from_name(object, str8_literal("factory.name"));

            if (!name.size) {
                name = str8_format(frame_arena(), "Factory %u", object->id);
            }
        } break;
        case Pipewire_Object_Client: {
            name = pipewire_object_property_string_from_name(object, str8_literal("client.name"));

            if (!name.size) {
                name = pipewire_object_property_string_from_name(object, str8_literal("application.name"));
            }

            if (!name.size) {
                name = str8_format(frame_arena(), "Client %u", object->id);
            }
        } break;
        case Pipewire_Object_Device: {
            name = pipewire_object_property_string_from_name(object, str8_literal("device.nick"));

            if (!name.size) {
                name = pipewire_object_property_string_from_name(object, str8_literal("device.name"));
            }

            if (!name.size) {
                name = str8_format(frame_arena(), "Device %u", object->id);
            }
        } break;
        case Pipewire_Object_Node: {
            name = pipewire_object_property_string_from_name(object, str8_literal("node.nick"));

            if (!name.size) {
                name = pipewire_object_property_string_from_name(object, str8_literal("node.name"));
            }

            if (!name.size) {
                name = str8_format(frame_arena(), "Node %u", object->id);
            }
        } break;
        case Pipewire_Object_Port: {
            name = pipewire_object_property_string_from_name(object, str8_literal("port.alias"));

            if (!name.size) {
                name = pipewire_object_property_string_from_name(object, str8_literal("port.name"));
            }

            if (!name.size) {
                name = str8_format(frame_arena(), "Port %u", object->id);
            }
        } break;
        case Pipewire_Object_Link: {
            name = str8_format(frame_arena(), "Link %u", object->id);
        } break;
        case Pipewire_Object_COUNT: {
        } break;
    }

    return name;
}

internal UI_Input object_button(Pipewire_Object *object) {
    Str8 name = name_from_object(object);

    if (pipewire_object_from_handle(state->hovered_object) == object) {
        UI_Palette palette = ui_palette_top();
        palette.background = color_from_theme(ThemeColor_Focus);
        ui_palette_next(palette);
    }

    UI_Input input = ui_button_format("%.*s###%p", str8_expand(name), object);
    if (input.flags & UI_InputFlag_Hovering) {
        state->hovered_object_next = pipewire_handle_from_object(object);
    }
    if (input.flags & UI_InputFlag_Clicked) {
        state->selected_object_next = pipewire_handle_from_object(object);
    }
    return input;
}

internal Void update(Void) {
    local U32 depth = 0;

    arena_reset(frame_arena());

    Gfx_EventList graphics_events = { 0 };
    if (depth == 0) {
        ++depth;
        graphics_events = gfx_get_events(frame_arena(), state->frames_to_render == 0);
        --depth;
    }

    UI_EventList ui_events = { 0 };

    // NOTE(simon): Consume events.
    for (Gfx_Event *event = graphics_events.first, *next = 0; event; event = next) {
        next = event->next;

        B32 consume = false;

        if (event->kind == Gfx_EventKind_Quit) {
            state->running = false;
        } else if (event->kind == Gfx_EventKind_KeyPress || event->kind == Gfx_EventKind_KeyRelease || event->kind == Gfx_EventKind_Text || event->kind == Gfx_EventKind_Scroll) {
            consume = true;
            UI_Event *ui_event = arena_push_struct(frame_arena(), UI_Event);
            switch (event->kind) {
                case Gfx_EventKind_Null:       ui_event->kind = UI_EventKind_Null;       break;
                case Gfx_EventKind_Quit:       ui_event->kind = UI_EventKind_Null;       break;
                case Gfx_EventKind_KeyPress:   ui_event->kind = UI_EventKind_KeyPress;   break;
                case Gfx_EventKind_KeyRelease: ui_event->kind = UI_EventKind_KeyRelease; break;
                case Gfx_EventKind_MouseMove:  ui_event->kind = UI_EventKind_Null;       break;
                case Gfx_EventKind_Text:       ui_event->kind = UI_EventKind_Text;       break;
                case Gfx_EventKind_Scroll:     ui_event->kind = UI_EventKind_Scroll;     break;
                case Gfx_EventKind_Resize:     ui_event->kind = UI_EventKind_Null;       break;
                case Gfx_EventKind_FileDrop:   ui_event->kind = UI_EventKind_Null;       break;
                case Gfx_EventKind_Wakeup:     ui_event->kind = UI_EventKind_Null;       break;
                case Gfx_EventKind_COUNT:      ui_event->kind = UI_EventKind_Null;       break;
            }
            ui_event->text      = event->text;
            ui_event->position  = event->position;
            ui_event->scroll    = event->scroll;
            ui_event->key       = event->key;
            ui_event->modifiers = event->key_modifiers;

            if (ui_event->kind != UI_EventKind_Null) {
                ui_event_list_push_event(&ui_events, ui_event);
            }
        }

        if (consume) {
            dll_remove(graphics_events.first, graphics_events.last, event);
        }
    }

    // NOTE(simon): Build palettes
    for (ThemePalette code = 0; code < ThemePalette_COUNT; ++code) {
        state->palettes[code].cursor    = state->theme.cursor;
        state->palettes[code].selection = state->theme.selection;
    }
    state->palettes[ThemePalette_Base].background = state->theme.base_background;
    state->palettes[ThemePalette_Base].border     = state->theme.base_border;
    state->palettes[ThemePalette_Base].text       = state->theme.text;
    state->palettes[ThemePalette_TitleBar].background = state->theme.title_bar_background;
    state->palettes[ThemePalette_TitleBar].border     = state->theme.title_bar_border;
    state->palettes[ThemePalette_TitleBar].text       = state->theme.text;
    state->palettes[ThemePalette_Button].background = state->theme.button_background;
    state->palettes[ThemePalette_Button].border     = state->theme.button_border;
    state->palettes[ThemePalette_Button].text       = state->theme.text;
    state->palettes[ThemePalette_SecondaryButton].background = state->theme.secondary_button_background;
    state->palettes[ThemePalette_SecondaryButton].border     = state->theme.secondary_button_border;
    state->palettes[ThemePalette_SecondaryButton].text       = state->theme.text;
    state->palettes[ThemePalette_Tab].background = state->theme.tab_background;
    state->palettes[ThemePalette_Tab].border     = state->theme.tab_border;
    state->palettes[ThemePalette_Tab].text       = state->theme.text;
    state->palettes[ThemePalette_InactiveTab].background = state->theme.inactive_tab_background;
    state->palettes[ThemePalette_InactiveTab].border     = state->theme.inactive_tab_border;
    state->palettes[ThemePalette_InactiveTab].text       = state->theme.text;
    state->palettes[ThemePalette_DropSiteOverlay].background = state->theme.drop_site_overlay;
    state->palettes[ThemePalette_DropSiteOverlay].border     = state->theme.drop_site_overlay;
    state->palettes[ThemePalette_DropSiteOverlay].text       = state->theme.text;

    // NOTE(simon): Build icon info.
    UI_IconInfo icon_info = { 0 };
    icon_info.icon_font = font_cache_font_from_static_data(&icon_font);
    icon_info.icon_kind_text[UI_IconKind_Minimize]   = icon_kind_text[UI_IconKind_Minimize];
    icon_info.icon_kind_text[UI_IconKind_Maximize]   = icon_kind_text[UI_IconKind_Maximize];
    icon_info.icon_kind_text[UI_IconKind_Close]      = icon_kind_text[UI_IconKind_Close];
    icon_info.icon_kind_text[UI_IconKind_Pin]        = icon_kind_text[UI_IconKind_Pin];
    icon_info.icon_kind_text[UI_IconKind_Eye]        = icon_kind_text[UI_IconKind_Eye];
    icon_info.icon_kind_text[UI_IconKind_NoEye]      = icon_kind_text[UI_IconKind_NoEye];
    icon_info.icon_kind_text[UI_IconKind_LeftArrow]  = icon_kind_text[UI_IconKind_LeftArrow];
    icon_info.icon_kind_text[UI_IconKind_RightArrow] = icon_kind_text[UI_IconKind_RightArrow];
    icon_info.icon_kind_text[UI_IconKind_UpArrow]    = icon_kind_text[UI_IconKind_UpArrow];
    icon_info.icon_kind_text[UI_IconKind_DownArrow]  = icon_kind_text[UI_IconKind_DownArrow];
    icon_info.icon_kind_text[UI_IconKind_LeftAngle]  = icon_kind_text[UI_IconKind_LeftAngle];
    icon_info.icon_kind_text[UI_IconKind_RightAngle] = icon_kind_text[UI_IconKind_RightAngle];
    icon_info.icon_kind_text[UI_IconKind_UpAngle]    = icon_kind_text[UI_IconKind_UpAngle];
    icon_info.icon_kind_text[UI_IconKind_DownAngle]  = icon_kind_text[UI_IconKind_DownAngle];
    icon_info.icon_kind_text[UI_IconKind_Check]      = icon_kind_text[UI_IconKind_Check];
    icon_info.icon_kind_text[UI_IconKind_File]       = icon_kind_text[UI_IconKind_File];
    icon_info.icon_kind_text[UI_IconKind_Folder]     = icon_kind_text[UI_IconKind_Folder];

    V2U32 client_size      = gfx_client_area_from_window(state->window);
    R2F32 client_rectangle = r2f32(0.0f, 0.0f, (F32) client_size.x, (F32) client_size.y);

    draw_begin_frame();

    ui_select_state(state->ui);
    ui_begin(state->window, &ui_events, &icon_info, 1.0f / 60.0f);
    ui_palette_push(palette_from_theme(ThemePalette_Base));
    ui_font_push(font_cache_font_from_static_data(&default_font));
    ui_font_size_push((U32) (state->font_size * gfx_dpi_from_window(state->window) / 72.0f));

    S64 object_count = { 0 };
    for (Pipewire_Object *object = pipewire_state->first_object; object; object = object->all_next) {
        ++object_count;
    }
    Pipewire_Object **objects = arena_push_array_no_zero(frame_arena(), Pipewire_Object *, (U64) object_count);
    Pipewire_Object **object_ptr = objects;
    for (Pipewire_Object *object = pipewire_state->first_object; object; object = object->all_next) {
        *object_ptr = object;
        ++object_ptr;
    }

    V2F32 size = v2f32((F32) client_size.width / 2.0f, (F32) client_size.height);
    F32 row_height = 2.0f * (F32) ui_font_size_top();
    local UI_ScrollPosition scroll_position = { 0 };

    R1S64 visible_range = { 0 };
    ui_text_x_padding(5.0f)
    ui_width(ui_size_fill())
    ui_height(ui_size_fill())
    ui_row() {
        ui_palette(palette_from_theme(ThemePalette_Button))
        ui_scroll_region(size, row_height, object_count, &visible_range, 0, &scroll_position) {
            ui_width(ui_size_fill())
            ui_height(ui_size_pixels(row_height, 1.0f))
            for (S64 i = visible_range.min; i < visible_range.max; ++i) {
                Pipewire_Object *object = objects[i];
                object_button(object );
            }
        }

        Pipewire_Object *selected_object = pipewire_object_from_handle(state->selected_object);
        ui_width(ui_size_pixels(size.width, 1.0f))
        ui_height(ui_size_fill())
        ui_column()
        if (!pipewire_object_is_nil(selected_object)) {
            ui_height(ui_size_children_sum(1.0f))
            ui_row()
            ui_width(ui_size_text_content(0, 1.0f))
            ui_height(ui_size_text_content(0, 1.0f)) {
                ui_label(kind_from_object(selected_object));
                ui_spacer_sized(ui_size_ems(0.5f, 1.0f));
                ui_label(name_from_object(selected_object));
            }

            ui_spacer_sized(ui_size_ems(0.5f, 1.0f));

            ui_row() {
                ui_width_push(ui_size_pixels(size.width / 2.0f, 1.0f));
                UI_Box *name_column_box = ui_column_begin();
                ui_width_next(ui_size_text_content(0, 1.0f));
                ui_height_next(ui_size_text_content(0, 1.0f));
                ui_label(str8_literal("Name"));
                ui_column_end();

                UI_Box *value_column_box = ui_column_begin();
                ui_width_next(ui_size_text_content(0, 1.0f));
                ui_height_next(ui_size_text_content(0, 1.0f));
                ui_label(str8_literal("Value"));
                ui_column_end();
                ui_width_pop();

                ui_width(ui_size_text_content(0, 1.0f))
                ui_height(ui_size_text_content(0, 1.0f))
                for (Pipewire_Property *property = selected_object->first_property; property; property = property->next) {
                    Pipewire_Object *reference = &pipewire_nil_object;

                    Str8 last_component = str8_skip(property->name, 1 + str8_last_index_of(property->name, '.'));
                    if (str8_equal(last_component, str8_literal("id")) || str8_equal(last_component, str8_literal("client")) || str8_equal(last_component, str8_literal("device")) || str8_equal(last_component, str8_literal("node")) || str8_equal(last_component, str8_literal("port"))) {
                        U32 id = pipewire_object_property_u32_from_name(selected_object, property->name);
                        reference = pipewire_object_from_id(id);
                    }

                    ui_parent_next(name_column_box);
                    ui_label(property->name);
                    ui_parent_next(value_column_box);

                    if (!pipewire_object_is_nil(reference)) {
                        ui_palette_next(palette_from_theme(ThemePalette_Button));
                        object_button(reference);
                    } else {
                        ui_label(property->value);
                    }
                }
            }
        }
    }

    state->selected_object = state->selected_object_next;
    state->hovered_object  = state->hovered_object_next;
    memory_zero_struct(&state->hovered_object_next);

    ui_font_size_pop();
    ui_font_pop();
    ui_palette_pop();
    ui_end();

    if (ui_is_animating_from_context(state->ui)) {
        request_frame();
    }

    // NOTE(simon): Draw UI.
    Draw_List *draw_list = draw_list_create();
    draw_list_scope(draw_list) {
        // NOTE(simon): Draw background.
        draw_rectangle(client_rectangle, color_from_theme(ThemeColor_BaseBackground), 0, 0, 0);

        // NOTE(simon): Draw border.
        draw_rectangle(r2f32_pad(client_rectangle, 1.0f), color_from_theme(ThemeColor_TitleBarBorder), 0, 1.0f, 1.0f);

        for (UI_Box *box = state->ui->root; !ui_box_is_null(box);) {
            // NOTE(simon): Draw drop shadow.
            if (box->flags & UI_BoxFlag_DrawDropShadow) {
                draw_rectangle(
                    r2f32(
                        box->calculated_rectangle.min.x - 4.0f,
                        box->calculated_rectangle.min.y - 4.0f,
                        box->calculated_rectangle.max.x + 12.0f,
                        box->calculated_rectangle.max.y + 12.0f
                    ),
                    color_from_theme(ThemeColor_DropShadow),
                    0.8f, 0.0f, 8.0f
                );
            }

            // NOTE(simon): Draw background.
            if (box->flags & UI_BoxFlag_DrawBackground) {
                Render_Shape *shape = draw_rectangle(r2f32_pad(box->calculated_rectangle, 1), box->palette.background, 0.0f, 0.0f, 1.0f);
                memory_copy(shape->radies, box->corner_radies, sizeof(shape->radies));

                if (box->flags & UI_BoxFlag_DrawHot && box->hot_t > 0.0f) {
                    F32 active_t = box->active_t;
                    if (!(box->flags & UI_BoxFlag_DrawActive)) {
                        active_t = 0.0f;
                    }
                    V4F32 color = color_from_theme(ThemeColor_Hover);
                    color.a *= 0.2f * (box->hot_t - active_t);

                    Render_Shape *rect = draw_rectangle(box->calculated_rectangle, color, 0.0f, 0.0f, 1.0f);
                    memory_copy(rect->radies, box->corner_radies, sizeof(rect->radies));
                }

                if (box->flags & UI_BoxFlag_DrawActive && box->active_t > 0.0f) {
                    Render_Shape *rect = draw_rectangle(box->calculated_rectangle, v4f32(0.0f, 0.0f, 0.0f, 0.0f), 0.0f, 0.0f, 1.0f);
                    V4F32 color = color_from_theme(ThemeColor_Hover);
                    color.r *= 0.3f;
                    color.g *= 0.3f;
                    color.b *= 0.3f;
                    color.a *= 0.5f * box->active_t;
                    rect->colors[Corner_10] = color;
                    rect->colors[Corner_11] = color;
                    memory_copy(rect->radies, box->corner_radies, sizeof(rect->radies));
                }
            }

            // NOTE(simon): Draw text.
            if (box->flags & UI_BoxFlag_DrawText) {
                V2F32 origin = ui_box_text_location(box);

                // NOTE(simon): Draw fuzzy matches.
                if (box->flags & UI_BoxFlag_DrawFuzzyMatches) {
                    F32 ascent = box->text.ascent;
                    F32 descent = box->text.descent;
                    for (FuzzyMatch *match = box->fuzzy_matches.first; match; match = match->next) {
                        F32 pixel_min =  f32_infinity();
                        F32 pixel_max = -f32_infinity();
                        U64 byte_offset = 0;
                        F32 advance = 0.0f;
                        for (U64 i = 0; i < box->text.letter_count; ++i) {
                            FontCache_Letter *letter = &box->text.letters[i];

                            if (match->min <= byte_offset && byte_offset < match->max) {
                                F32 pre_offset  = advance + letter->offset.x;
                                F32 post_offset = advance + letter->advance;
                                pixel_min = f32_min(pre_offset,  pixel_min);
                                pixel_max = f32_max(post_offset, pixel_max);
                            }

                            advance += letter->advance;
                            byte_offset += letter->decode_size;
                        }
                        V4F32 color = color_from_theme(ThemeColor_Focus);
                        color.a *= 0.2f;
                        draw_rectangle(
                            r2f32(
                                f32_floor(origin.x + pixel_min),
                                f32_floor(origin.y - ascent),
                                f32_floor(origin.x + pixel_max),
                                f32_floor(origin.y - descent)
                            ),
                            color,
                            0,
                            0,
                            0
                        );
                    }
                }

                // NOTE(simon): Draw text.
                {
                    F32 advance = 0.0f;
                    for (U64 i = 0; i < box->text.letter_count; ++i) {
                        FontCache_Letter *letter = &box->text.letters[i];
                        draw_glyph(
                            r2f32(
                                f32_floor(origin.x + letter->offset.x + advance),
                                f32_floor(origin.y + letter->offset.y),
                                f32_floor(origin.x + letter->offset.x + advance + letter->size.x),
                                f32_floor(origin.y + letter->offset.y + letter->size.y)
                            ),
                            letter->source,
                            letter->texture,
                            box->palette.text
                        );
                        advance += letter->advance;
                    }
                }
            }

            // NOTE(simon): Push clip.
            if (box->flags & UI_BoxFlag_Clip) {
                R2F32 top_clip = draw_clip_top();
                R2F32 new_clip = r2f32_intersect(r2f32_pad(top_clip, -1.0f), box->calculated_rectangle);
                draw_clip_push(new_clip);
            }

            // NOTE(simon): Custom draw list.
            if (box->draw_list) {
                draw_transform(m3f32_translation(box->calculated_rectangle.min)) {
                    draw_sub_list(box->draw_list);
                }
            }

            // NOTE(simon): Custom draw callback.
            if (box->draw_function) {
                box->draw_function(box, box->draw_data);
            }

            UI_BoxIterator iterator = ui_box_iterator_depth_first_pre_order(box);

            // NOTE(simon): We use `<=` because we need to pop our state when
            // moving to our siblings. Traversing siblings sets both `push_count`
            // and `pop_count` to 0.
            U32 pop_index = 0;
            for (UI_Box *parent = box; pop_index <= iterator.pop_count; parent = parent->parent, ++pop_index) {
                if (parent == box && iterator.push_count) {
                    continue;
                }

                // NOTE(simon): Pop clip.
                if (parent->flags & UI_BoxFlag_Clip) {
                    draw_clip_pop();
                }

                // NOTE(simon): Draw border.
                if (parent->flags & UI_BoxFlag_DrawBorder) {
                    Render_Shape *shape = draw_rectangle(r2f32_pad(parent->calculated_rectangle, 1.0f), parent->palette.border, 0.0f, 1.0f, 1.0f);
                    memory_copy(shape->radies, parent->corner_radies, sizeof(shape->radies));

                    if (parent->flags & UI_BoxFlag_DrawHot && parent->hot_t > 0.0f) {
                        V4F32 color = color_from_theme(ThemeColor_Hover);
                        color.a *= parent->hot_t;

                        Render_Shape *rect = draw_rectangle(r2f32_pad(parent->calculated_rectangle, 1.0f), color, 0.0f, 1.0f, 1.0f);
                        memory_copy(rect->radies, parent->corner_radies, sizeof(rect->radies));
                    }
                }

                // NOTE(simon): Draw focus overlay.
                if (parent->flags & UI_BoxFlag_Clickable && parent->focus_active_t > 0.01f && !(parent->flags & UI_BoxFlag_DisableFocusOverlay)) {
                    V4F32 color = color_from_theme(ThemeColor_Focus);
                    color.a *= 0.05f * parent->focus_active_t;
                    Render_Shape *shape = draw_rectangle(parent->calculated_rectangle, color, 0.0f, 0.0f, 0.0f);
                    memory_copy(shape->radies, parent->corner_radies, sizeof(shape->radies));
                }

                // NOTE(simon): Draw focus border.
                if (parent->flags & UI_BoxFlag_Clickable && parent->focus_hot_t > 0.01f && !(parent->flags & UI_BoxFlag_DisableFocusBorder)) {
                    V4F32 color = color_from_theme(ThemeColor_Focus);
                    color.a *= parent->focus_hot_t;
                    Render_Shape *shape = draw_rectangle(parent->calculated_rectangle, color, 0.0f, 1.0f, 1.0f);
                    memory_copy(shape->radies, parent->corner_radies, sizeof(shape->radies));
                }

                // NOTE(simon): Draw disable overlay.
                if (parent->flags & UI_BoxFlag_Disabled) {
                    V4F32 color = color_from_theme(ThemeColor_DisabledOverlay);
                    color.a *= parent->disabled_t;
                    Render_Shape *shape = draw_rectangle(parent->calculated_rectangle, color, 0.0f, 0.0f, 1.0f);
                    memory_copy(shape->radies, parent->corner_radies, sizeof(shape->radies));
                }

                // NOTE(simon): Debug lines for UI.
                if (0) {
                    draw_rectangle(parent->calculated_rectangle, color_from_srgba_u32(0xff00ffff), 0.0f, 1.0f, 1.0f);
                }
            }

            box = iterator.next;
        }
    }

    render_begin();
    render_window_begin(state->window, state->render);
    draw_submit_list(state->window, state->render, draw_list);
    render_window_end(state->window, state->render);
    render_end();

    if (state->frames_to_render > 0) {
        --state->frames_to_render;
    }

    if (PROFILE_BUILD) {
        request_frame();
    }

    ++state->frame_index;
}

internal S32 os_run(Str8List arguments) {
    Arena *arena = arena_create();
    state = arena_push_struct(arena, State);
    state->arena    = arena;

    for (U64 i = 0; i < array_count(state->frame_arenas); ++i) {
        state->frame_arenas[i] = arena_create();
    }

    state->running = true;
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

        V4F32 green6 = color_from_srgba_u32(0x40c057ff);

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
    }

    Log *log = log_create();
    log_select(log);

    gfx_init();
    render_init();

    state->window = gfx_window_create(str8_literal("Pipewire-test"), 1280, 720);
    state->render = render_create(state->window);
    state->ui = ui_create();

    font_cache_create();
    gfx_set_update_function(update);

    pipewire_init();

    while (state->running) {
        update();
    }

    render_destroy(state->window, state->render);
    gfx_window_close(state->window);

    return 0;
}
