#ifndef CORE_H
#define CORE_H

#include "generated.h"



// NOTE(simon): Fonts and icons.

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
    X(SecondaryButtonBorder,     secondary_button_border,     "Secondary button border")

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



// NOTE(simon): Patchbay

typedef struct GraphNode GraphNode;
struct GraphNode {
    GraphNode *next;
    GraphNode *previous;
    V2F32      position;

    Pipewire_Handle handle;

    U64 last_frame_used;
};

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

    V2F32      graph_offset;
    GraphNode *first_node;
    GraphNode *last_node;
    GraphNode *node_freelist;

    Pipewire_Handle selected_object;
    Pipewire_Handle selected_object_next;
    Pipewire_Handle hovered_object;
    Pipewire_Handle hovered_object_next;

    Pipewire_Handle selected_port;
};

global State *state;



// NOTE(simon): Frames.

internal Void   request_frame(Void);
internal Arena *frame_arena(Void);

internal Str8     kind_from_object(Pipewire_Object *object);
internal Str8     name_from_object(Pipewire_Object *object);
internal UI_Input object_button(Pipewire_Object *object);

#endif //CORE_H
