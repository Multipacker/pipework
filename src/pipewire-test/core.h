#ifndef CORE_H
#define CORE_H

#include "generated.h"



// NOTE(simon): Fonts and icons.

embed_file(default_font, "/data/NotoSans-Regular.ttf");
embed_file(mono_font, "/data/NotoSansMono-Regular.ttf");

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
    X(PortUnknown,               port_unknown,                "Unknown port")                \
    X(PortAudio,                 port_audio,                  "Audio port")                  \
    X(PortVideo,                 port_video,                  "Video port")                  \
    X(PortImage,                 port_image,                  "Image port")                  \
    X(PortBinary,                port_binary,                 "Binary port")                 \
    X(PortStream,                port_stream,                 "Stream port")                 \
    X(PortApplication,           port_application,            "Application port")            \
    X(VolumeAttenuate,           volume_attenuate,            "Volume attenuate")            \
    X(VolumeHardwareAmplify,     volume_hardware_amplify,     "Volume hardware amplify")     \
    X(VolumeSoftwareAmplify,     volume_software_amplify,     "Volume software amplify")

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

typedef struct Handle Handle;
struct Handle {
    U64 u64[2];
};

#define BUILD_TAB_FUNCTION(name) Void name(R2F32 tab_rectangle)
typedef BUILD_TAB_FUNCTION(BuildTabFunction);
internal BUILD_TAB_FUNCTION(build_nil_tab);

typedef struct Tab Tab;
struct Tab {
    Tab *next;
    Tab *previous;

    U64 generation;

    Arena *arena;
    Str8   name;
    Void  *state;
    BuildTabFunction *build;

    B32 query_open;
    U8  query_buffer[1024];
    U64 query_mark;
    U64 query_cursor;
    U64 query_size;
};

global Tab nil_tab = {
    .next     = &nil_tab,
    .previous = &nil_tab,

    .build = build_nil_tab,
};

typedef struct Panel Panel;
struct Panel {
    // NOTE(simon): Tree links.
    Panel *next;
    Panel *previous;
    Panel *first;
    Panel *last;
    Panel *parent;

    U64 generation;

    Tab *first_tab;
    Tab *last_tab;
    Handle active_tab;

    F32   percentage_of_parent;
    Axis2 split_axis;
    U64   child_count;

    R2F32 animated_rectangle_percentage;
};

typedef struct PanelIterator PanelIterator;
struct PanelIterator {
    Panel *next;
    U64 push_count;
    U64 pop_count;
};

global Panel nil_panel = {
    .next      = &nil_panel,
    .previous  = &nil_panel,
    .first     = &nil_panel,
    .last      = &nil_panel,
    .parent    = &nil_panel,
    .first_tab = &nil_tab,
    .last_tab  = &nil_tab,
};

typedef struct Window Window;
struct Window {
    Window *next;
    Window *previous;

    U64 generation;

    Arena *arena;

    Gfx_Window     window;
    Render_Window  render;
    UI_Context    *ui;

    Panel *root_panel;
    Handle active_panel;

    Draw_List   *draw_list;
    UI_EventList ui_events;
};

global Window nil_window = {
    .root_panel = &nil_panel,
};

#define CONTEXT_MEMBERS(X)                                    \
    X(Handle,          tab,                Tab)               \
    X(Handle,          panel,              Panel)             \
    X(Handle,          window,             Window)            \
    X(Handle,          previous_tab,       PreviousTab)       \
    X(Handle,          destination_panel,  DestinationPanel)  \
    X(Handle,          destination_window, DestinationWindow) \
    X(Direction2,      direction,          Direction)         \
    X(Pipewire_Handle, port,               Port)

typedef enum {
    ContextMember_Null,
#define X(type, name, type_name) ContextMember_##type_name,
    CONTEXT_MEMBERS(X)
#undef X
    ContextMember_COUNT,
} ContextMember;

typedef struct Context Context;
struct Context {
    Context *next;

#define X(type, name, type_name) type name;
    CONTEXT_MEMBERS(X)
#undef X
};

#define COMMANDS(X)                                                                                                        \
    X(CloseTab,             "Close tab",                   "Close the current tab")                                        \
    X(MoveTab,              "Move tab",                    "")                                                             \
    X(NextTab,              "Next tab",                    "Switch to the next tab")                                       \
    X(PreviousTab,          "Previous tab",                "Switch to the previous tab")                                   \
    X(FocusPanelLeft,       "Focus panel left",            "Focus the panel above")                                        \
    X(FocusPanelUp,         "Focus panel up",              "Focus the panel on the left")                                  \
    X(FocusPanelRight,      "Focus panel right",           "Focus the panel on the right")                                 \
    X(FocusPanelDown,       "Focus panel down",            "Focus the panel below")                                        \
    X(FocusPanel,           "Focus panel",                 "")                                                             \
    X(ClosePanel,           "Close panel",                 "Close the current panel")                                      \
    X(SplitPanel,           "Split panel",                 "")                                                             \
    X(SelectWordLeft,       "Select word left",            "Extends the selection one word to the left")                   \
    X(SelectWordUp,         "Select word up",              "Extends the selection one word up")                            \
    X(SelectWordRight,      "Select word right",           "Extends the selection one word to the right")                  \
    X(SelectWordDown,       "Select word down",            "Extends the selection one word down")                          \
    X(SelectCharacterLeft,  "Select character left",       "Extends the selection one character to the left")              \
    X(SelectCharacterUp,    "Select character up",         "Extends the selection one character up")                       \
    X(SelectCharacterRight, "Select character right",      "Extends the selection one character to the right")             \
    X(SelectCharacterDown,  "Select character down",       "Extends the selection one character down")                     \
    X(MoveWordLeft,         "Move word left",              "Moves one word to the left")                                   \
    X(MoveWordUp,           "Move word up",                "Moves one word up")                                            \
    X(MoveWordRight,        "Move word right",             "Moves one word to the right")                                  \
    X(MoveWordDown,         "Move word down",              "Moves one word down")                                          \
    X(MoveCharacterLeft,    "Move character left",         "Moves one character to the left")                              \
    X(MoveCharacterUp,      "Move character up",           "Moves one character up")                                       \
    X(MoveCharacterRight,   "Move character right",        "Moves one character to the right")                             \
    X(MoveCharacterDown,    "Move character down",         "Moves one character down")                                     \
    X(SelectHome,           "Select home",                 "Extends the selection to the start of the line")               \
    X(SelectEnd,            "Select end",                  "Extends the selection to the end of the line")                 \
    X(MoveHome,             "Move home",                   "Moves to the start of the line")                               \
    X(MoveEnd,              "Move end",                    "Moves to the end of the line")                                 \
    X(SelectPageUp,         "Select page up",              "Extends the selection on page up")                             \
    X(SelectPageDown,       "Select page down",            "Extends the selection on page down")                           \
    X(MovePageUp,           "Move page up",                "Moves one page up")                                            \
    X(MovePageDown,         "Move page down",              "Moves one page down")                                          \
    X(SelectWholeUp,        "Select whole up",             "Extends the selection to the begining")                        \
    X(SelectWholeDown,      "Select whole down",           "Extends the selection to the start")                           \
    X(MoveWholeUp,          "Move whole up",               "Moves to the begining")                                        \
    X(MoveWholeDown,        "Move whole down",             "Moves to the end")                                             \
    X(RemoveWord,           "Remove word",                 "Removes one word")                                             \
    X(DeleteWord,           "Delete word",                 "Deletes one word")                                             \
    X(RemoveCharacter,      "Remove character",            "Removes one character")                                        \
    X(DeleteCharacter,      "Delete character",            "Deletes one character")                                        \
    X(SelectAll,            "Select all",                  "Selects everything")                                           \
    X(Copy,                 "Copy",                        "Copies the current selection to the clipboard")                \
    X(Paste,                "Paste",                       "Pastes the current clipboard contents")                        \
    X(Cut,                  "Cut",                         "Copies the current selection to the clipboard and deletes it") \
    X(Accept,               "Accept",                      "Accepts the current action")                                   \
    X(Cancel,               "Cancel",                      "Cancles the current action")                                   \
    X(OpenSearch,           "Open search",                 "Open the search field for the current tab")                    \
    X(CloseSearch,          "Close search",                "Close the search field for the current tab")

typedef enum {
    CommandKind_Null,
#define X(name, display_name, description) CommandKind_##name,
    COMMANDS(X)
#undef X
    CommandKind_COUNT,
} CommandKind;

typedef struct Command Command;
struct Command {
    Command    *next;
    CommandKind kind;
    Context    *context;
};

typedef enum {
    DragState_None,
    DragState_Dragging,
    DragState_Dropping,
} DragState;

typedef struct State State;
struct State {
    Arena *arena;

    Arena *frame_arenas[2];
    U64 frame_index;

    U32 frames_to_render;

    Window *first_window;
    Window *last_window;

    Tab    *tab_freelist;
    Panel  *panel_freelist;
    Window *window_freelist;

    // NOTE(simon): Commands.
    Arena   *command_arena;
    Command *first_command;
    Command *last_command;

    Theme theme;
    UI_Palette palettes[ThemePalette_COUNT];

    // NOTE(simon): Contexts (per frame)
    Context  base_context;
    Context *context_stack;

    // NOTE(simon): Drag-and-drop.
    DragState     drag_state;
    Arena        *drag_arena;
    Context      *drag_context;
    ContextMember drag_context_member;

    F32 font_size;

    Pipewire_Handle selected_object;
    Pipewire_Handle selected_object_next;

    Pipewire_Handle selected_port;
};

global State *state;



// NOTE(simon): Frames.

internal Void   request_frame(Void);
internal Arena *frame_arena(Void);

internal Str8     kind_from_object(Pipewire_Object *object);
internal Str8     name_from_object(Arena *arena, Pipewire_Object *object);
internal UI_Input object_button(Pipewire_Object *object);

// NOTE(simon): Context.

#define CONTEXT_VALUE(type, name, type_name) .name = top_context()->name,
#define top_context_values CONTEXT_MEMBERS(CONTEXT_VALUE)
#define push_context(...) push_context_internal(&(Context) { top_context_values __VA_ARGS__ })
#define context_scope(...) defer_loop(push_context(__VA_ARGS__), pop_context())
internal Context *copy_context(Arena *arena, Context *context);
internal Void     push_context_internal(Context *context);
internal Void     pop_context(Void);
internal Context *top_context(Void);

// NOTE(simon): Commands.

#define push_command(kind, ...) push_command_internal(kind, &(Context) { top_context_values __VA_ARGS__ })
internal Void push_command_internal(CommandKind kind, Context *context);

// NOTE(simon): Tabs.

internal Handle handle_from_tab(Tab *tab);
internal Tab  *tab_from_handle(Handle handle);
internal B32   is_nil_tab(Tab *tab);
internal Tab  *create_tab(Str8 title);
internal Void  destroy_tab(Tab *tab);
#define tab_state_from_type(type) ((type *) tab_state_from_size_alignment(sizeof(type), _Alignof(type)))
internal Void *tab_state_from_size_alignment(U64 size, U64 alignment);
internal Str8  query_from_tab(Void);

// NOTE(simon): Panels.

internal Handle        handle_from_panel(Panel *panel);
internal Panel        *panel_from_handle(Handle handle);
internal B32           is_nil_panel(Panel *panel);
internal Panel        *create_panel(Void);
internal Void          destroy_panel(Panel *panel);
internal Void          insert_panel(Panel *parent, Panel *previous, Panel *child);
internal Void          remove_panel(Panel *parent, Panel *child);
internal Void          insert_tab(Panel *panel, Tab *previous_tab, Tab *tab);
internal Void          remove_tab(Panel *panel, Tab *tab);
internal PanelIterator panel_iterator_depth_first_pre_order(Panel *panel, Panel *root_panel);

// NOTE(simon): Windows.

internal Handle  handle_from_window(Window *window);
internal Window *window_from_handle(Handle handle);
internal Window *window_from_gfx_handle(Gfx_Window handle);
internal B32     is_nil_window(Window *window);
internal Window *create_window(Str8 title, U32 width, U32 height);
internal Void    destroy_window(Window *window);

// NOTE(simon): Drag-and-drop.
internal Void drag_begin(ContextMember context_member);
internal B32  drag_is_active(Void);
internal Void drag_cancel(Void);
internal B32  drag_drop(Void);

// NOTE(simon): Views

internal BUILD_TAB_FUNCTION(build_list_tab);
internal BUILD_TAB_FUNCTION(build_property_tab);
internal BUILD_TAB_FUNCTION(build_parameter_tab);
internal BUILD_TAB_FUNCTION(build_graph_tab);
internal BUILD_TAB_FUNCTION(build_volume_tab);

#endif //CORE_H
