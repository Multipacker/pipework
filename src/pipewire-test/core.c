// NOTE(simon): Themes.

internal V4F32 color_from_theme(ThemeColor color) {
    V4F32 result = state->theme.colors[color];
    return result;
}

internal UI_Palette palette_from_theme(ThemePalette palette) {
    UI_Palette result = state->palettes[palette];
    return result;
}



// NOTE(simon): Frames.

internal Void request_frame(Void) {
    state->frames_to_render = 4;
}

internal Arena *frame_arena(Void) {
    Arena *result = state->frame_arenas[state->frame_index % array_count(state->frame_arenas)];
    return result;
}



// NOTE(simon): Context.

internal Context *copy_context(Arena *arena, Context *context) {
    Context *copy = arena_push_struct(arena, Context);
    *copy = *context;
    copy->next = 0;
    return copy;
}

internal Void push_context_internal(Context *context) {
    Context *copy = copy_context(frame_arena(), context);
    sll_stack_push(state->context_stack, copy);
}

internal Void pop_context(Void) {
    sll_stack_pop(state->context_stack);
}

internal Context *top_context(Void) {
    Context *result = state->context_stack;
    return result;
}



// NOTE(simon): Commands.

internal Void push_command_internal(CommandKind kind, Context *context) {
    Command *command = arena_push_struct(state->command_arena, Command);
    command->kind = kind;
    command->context = copy_context(state->command_arena, context);
    sll_queue_push(state->first_command, state->last_command, command);
}



// NOTE(simon): Tabs.

internal Handle handle_from_tab(Tab *tab) {
    Handle handle = { 0 };

    if (tab) {
        handle.u64[0] = tab->generation;
        handle.u64[1] = integer_from_pointer(tab);
    }

    return handle;
}

internal Tab *tab_from_handle(Handle handle) {
    Tab *tab = (Tab *) pointer_from_integer(handle.u64[1]);

    if (!tab || tab->generation != handle.u64[0]) {
        tab = &nil_tab;
    }

    return tab;
}

internal B32 is_nil_tab(Tab *tab) {
    B32 result = !tab || tab == &nil_tab;
    return result;
}

internal Tab *create_tab(Str8 name) {
    Tab *tab = state->tab_freelist;
    if (tab) {
        sll_stack_pop(state->tab_freelist);
        U64 generation = tab->generation;
        memory_zero_struct(tab);
        tab->generation = generation;
    } else {
        tab = arena_push_struct(state->arena, Tab);
    }

    tab->next     = &nil_tab;
    tab->previous = &nil_tab;

    tab->arena = arena_create();
    tab->name  = str8_copy(tab->arena, name);
    tab->build = build_nil_tab;

    return tab;
}

internal Void destroy_tab(Tab *tab) {
    if (!is_nil_tab(tab)) {
        arena_destroy(tab->arena);

        ++tab->generation;
        sll_stack_push(state->tab_freelist, tab);
    }
}

internal Void *tab_state_from_size_alignment(U64 size, U64 alignment) {
    Tab *tab = tab_from_handle(top_context()->tab);

    Void *tab_state = tab->state;
    if (!tab_state) {
        if (!is_nil_tab(tab)) {
            tab_state = tab->state = arena_push(tab->arena, size, alignment);
        } else {
            tab_state = arena_push(frame_arena(), size, alignment);
        }
    }

    return tab_state;
}




// NOTE(simon): Panels.

internal Handle handle_from_panel(Panel *panel) {
    Handle handle = { 0 };

    if (panel) {
        handle.u64[0] = panel->generation;
        handle.u64[1] = integer_from_pointer(panel);
    }

    return handle;
}

internal Panel *panel_from_handle(Handle handle) {
    Panel *panel = (Panel *) pointer_from_integer(handle.u64[1]);

    if (!panel || panel->generation != handle.u64[0]) {
        panel = &nil_panel;
    }

    return panel;
}

internal B32 is_nil_panel(Panel *panel) {
    B32 result = !panel || panel == &nil_panel;
    return result;
}

internal Panel *create_panel(Void) {
    Panel *panel = state->panel_freelist;
    if (panel) {
        sll_stack_pop(state->panel_freelist);
        U64 generation = panel->generation;
        memory_zero_struct(panel);
        panel->generation = generation;
    } else {
        panel = arena_push_struct(state->arena, Panel);
    }

    panel->next     = &nil_panel;
    panel->previous = &nil_panel;
    panel->first    = &nil_panel;
    panel->last     = &nil_panel;
    panel->parent   = &nil_panel;

    panel->first_tab = &nil_tab;
    panel->last_tab  = &nil_tab;

    return panel;
}

internal Void destroy_panel(Panel *panel) {
    if (!is_nil_panel(panel)) {
        // NOTE(simon): Free all child panels.
        for (Panel *child = panel->first, *next = &nil_panel; !is_nil_panel(child); child = next) {
            next = child->next;

            destroy_panel(child);
        }

        // NOTE(simon): Free all tabs.
        for (Tab *tab = panel->first_tab, *next = &nil_tab; !is_nil_tab(tab); tab = next) {
            next = tab->next;
            destroy_tab(tab);
        }

        ++panel->generation;
        sll_stack_push(state->panel_freelist, panel);
    }
}

internal Void insert_panel(Panel *parent, Panel *previous, Panel *child) {
    if (!is_nil_panel(parent) && !is_nil_panel(child)) {
        dll_insert_next_previous_zero(parent->first, parent->last, previous, child, next, previous, &nil_panel);
        ++parent->child_count;
        child->parent = parent;
    }
}

internal Void remove_panel(Panel *parent, Panel *child) {
    if (!is_nil_panel(parent) && !is_nil_panel(child)) {
        dll_remove_next_previous_zero(parent->first, parent->last, child, next, previous, &nil_panel);
        --parent->child_count;
        child->next = child->previous = child->parent = &nil_panel;
    }
}

internal Void insert_tab(Panel *panel, Tab *previous_tab, Tab *tab) {
    dll_insert_next_previous_zero(panel->first_tab, panel->last_tab, previous_tab, tab, next, previous, &nil_tab);
    panel->active_tab = handle_from_tab(tab);
    ++panel->child_count;
}

internal Void remove_tab(Panel *panel, Tab *tab) {
    if (tab_from_handle(panel->active_tab) == tab) {
        if (!is_nil_tab(tab->next)) {
            panel->active_tab = handle_from_tab(tab->next);
        } else if (!is_nil_tab(tab->previous)) {
            panel->active_tab = handle_from_tab(tab->previous);
        } else {
            panel->active_tab = handle_from_tab(&nil_tab);
        }
    }

    dll_remove_next_previous_zero(panel->first_tab, panel->last_tab, tab, next, previous, &nil_tab);
    --panel->child_count;
}

internal PanelIterator panel_iterator_depth_first_pre_order(Panel *panel, Panel *root_panel) {
    PanelIterator iterator = { 0 };
    iterator.next = &nil_panel;

    if (!is_nil_panel(panel->first)) {
        iterator.next = panel->first;
        ++iterator.push_count;
    } else {
        for (Panel *parent = panel; parent != root_panel; parent = parent->parent) {
            if (!is_nil_panel(parent->next)) {
                iterator.next = parent->next;
                break;
            }
            ++iterator.pop_count;
        }
    }

    return iterator;
}

internal R2F32 rectangle_from_panel_parent_rectangle(Panel *panel, Panel *parent, R2F32 parent_rectangle) {
    R2F32 rectangle = parent_rectangle;

    V2F32 parent_size = r2f32_size(parent_rectangle);

    for (Panel *child = parent->first; !is_nil_panel(child) && child != panel; child = child->next) {
        rectangle.min.values[parent->split_axis] += child->percentage_of_parent * parent_size.values[parent->split_axis];
    }
    rectangle.max.values[parent->split_axis] = rectangle.min.values[parent->split_axis] + panel->percentage_of_parent * parent_size.values[parent->split_axis];

    return rectangle;
}

internal R2F32 rectangle_from_panel(Panel *query_panel, R2F32 root_rectangle) {
    Arena_Temporary scratch = arena_get_scratch(0, 0);
    typedef struct PanelNode PanelNode;
    struct PanelNode {
        PanelNode *next;
        Panel     *panel;
    };

    // NOTE(simon): Collect parents.
    PanelNode *panel_stack = 0;
    for (Panel *panel = query_panel; !is_nil_panel(panel->parent); panel = panel->parent) {
        PanelNode *panel_node = arena_push_struct(scratch.arena, PanelNode);
        panel_node->panel = panel;
        sll_stack_push(panel_stack, panel_node);
    }

    R2F32 panel_rectangle = root_rectangle;
    for (PanelNode *panel_node = panel_stack; panel_node; panel_node = panel_node->next) {
        Panel *panel  = panel_node->panel;
        Panel *parent = panel->parent;

        V2F32 panel_size = r2f32_size(panel_rectangle);

        panel_rectangle = rectangle_from_panel_parent_rectangle(panel, parent, panel_rectangle);
    }

    arena_end_temporary(scratch);
    return panel_rectangle;
}




// NOTE(simon): Windows.

internal Handle handle_from_window(Window *window) {
    Handle handle = { 0 };

    if (window) {
        handle.u64[0] = window->generation;
        handle.u64[1] = integer_from_pointer(window);
    }

    return handle;
}

internal Window *window_from_handle(Handle handle) {
    Window *window = (Window *) pointer_from_integer(handle.u64[1]);

    if (!window || window->generation != handle.u64[0]) {
        window = &nil_window;
    }

    return window;
}

internal Window *window_from_gfx_handle(Gfx_Window handle) {
    Window *result = &nil_window;

    for (Window *window = state->first_window; window; window = window->next) {
        if (gfx_window_equal(window->window, handle)) {
            result = window;
            break;
        }
    }

    return result;
}

internal B32 is_nil_window(Window *window) {
    B32 result = !window || window == &nil_window;
    return result;
}

internal Window *create_window(Str8 title, U32 width, U32 height) {
    Window *window = state->window_freelist;
    if (window) {
        sll_stack_pop(state->window_freelist);
        U64 generation = window->generation;
        memory_zero_struct(window);
        window->generation = generation;
    } else {
        window = arena_push_struct(state->arena, Window);
    }

    dll_push_back(state->first_window, state->last_window, window);

    window->arena  = arena_create();
    window->window = gfx_window_create(title, width, height);
    window->render = render_create(window->window);
    window->ui     = ui_create();

    window->root_panel = create_panel();

    return window;
}

internal Void destroy_window(Window *window) {
    if (!is_nil_window(window)) {
        dll_remove(state->first_window, state->last_window, window);

        destroy_panel(window->root_panel);

        render_destroy(window->window, window->render);
        gfx_window_close(window->window);
        arena_destroy(window->arena);

        ++window->generation;
        sll_stack_push(state->window_freelist, window);
    }
}



// NOTE(simon): Drag-and-drop.

internal Void drag_begin(ContextMember context_member) {
    if (!drag_is_active()) {
        arena_reset(state->drag_arena);
        state->drag_state = DragState_Dragging;
        state->drag_context = copy_context(state->drag_arena, top_context());
        state->drag_context_member = context_member;
    }
}

internal B32 drag_is_active(Void) {
    B32 result = state->drag_state == DragState_Dragging || state->drag_state == DragState_Dropping;
    return result;
}

internal Void drag_cancel(Void) {
    state->drag_state = DragState_None;
}

internal B32 drag_drop(Void) {
    B32 result = false;
    if (state->drag_state == DragState_Dropping) {
        result = true;
        state->drag_state = DragState_None;
    }
    return result;
}



internal Str8 kind_from_object(Pipewire_Object *object) {
    Str8 result = { 0 };

    switch (object->kind) {
        case Pipewire_Object_Null: {
            result = str8_literal("Null");
        } break;
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
        case Pipewire_Object_Null: {
            name = str8_literal("Null");
        } break;
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

internal BUILD_TAB_FUNCTION(build_nil_tab) {
}

internal BUILD_TAB_FUNCTION(build_list_tab) {
    typedef struct TabState TabState;
    struct TabState {
        UI_ScrollPosition all_objects_scroll_position;
    };

    TabState *tab_state = tab_state_from_type(TabState);

    // NOTE(simon): Collect pipewire objects.
    S64 object_count = { 0 };
    Pipewire_Object **objects = 0;
    {
        for (Pipewire_Object *object = pipewire_state->first_object; object; object = object->all_next) {
            ++object_count;
        }

        objects = arena_push_array_no_zero(frame_arena(), Pipewire_Object *, (U64) object_count);
        Pipewire_Object **object_ptr = objects;
        for (Pipewire_Object *object = pipewire_state->first_object; object; object = object->all_next) {
            *object_ptr = object;
            ++object_ptr;
        }
    }

    V2F32 tab_size   = r2f32_size(tab_rectangle);
    F32   row_height = 2.0f * (F32) ui_font_size_top();

    // NOTE(simon): Build list of all objects.
    R1S64 visible_range = { 0 };
    ui_palette(palette_from_theme(ThemePalette_Button))
    ui_scroll_region(tab_size, row_height, object_count, &visible_range, 0, &tab_state->all_objects_scroll_position) {
        ui_text_x_padding(5.0f)
        ui_width(ui_size_fill())
        ui_height(ui_size_pixels(row_height, 1.0f))
        for (S64 i = visible_range.min; i < visible_range.max; ++i) {
            Pipewire_Object *object = objects[i];
            object_button(object);
        }
    }
}

internal BUILD_TAB_FUNCTION(build_property_tab) {
    typedef struct TabState TabState;
    struct TabState {
        UI_ScrollPosition scroll_position;
        F32 column_widths[2];
    };

    B32 is_new_tab = !tab_from_handle(top_context()->tab)->state;
    TabState *tab_state = tab_state_from_type(TabState);

    if (is_new_tab) {
        tab_state->column_widths[0] = 1.0f / 2.0f;
        tab_state->column_widths[1] = 1.0f / 2.0f;
    }

    Pipewire_Object *selected_object = pipewire_object_from_handle(state->selected_object);

    // NOTE(simon): Collect all properties.
    S64 property_count = 0;
    Pipewire_Property **properties = 0;
    {
        for (Pipewire_Property *property = selected_object->first_property; property; property = property->next) {
            ++property_count;
        }
        properties = arena_push_array(frame_arena(), Pipewire_Property *, (U64) property_count);
        for (Pipewire_Property *property = selected_object->first_property, **property_ptr = properties; property; property = property->next) {
            *property_ptr++ = property;
        }
    }

    V2F32 tab_size   = r2f32_size(tab_rectangle);
    F32   row_height = 2.0f * (F32) ui_font_size_top();

    R1S64 visible_range = { 0 };
    ui_palette(palette_from_theme(ThemePalette_Button))
    ui_font(font_cache_font_from_static_data(&mono_font))
    ui_scroll_region(tab_size, row_height, property_count, &visible_range, 0, &tab_state->scroll_position)
    ui_text_x_padding(5.0f)
    ui_palette(palette_from_theme(ThemePalette_Base)) {
        // NOTE(simon): Build column resize handles
        F32 column_position = 0.0f;
        F32 container_width = ui_parent_top()->size[Axis2_X].value;
        ui_fixed_y(0.0f)
        ui_width(ui_size_pixels(5.0f, 1.0f))
        ui_height(ui_size_pixels(row_height * (F32) r1s64_size(visible_range), 1.0f))
        ui_hover_cursor(Gfx_Cursor_SizeWE)
        for (U32 i = 0; i < array_count(tab_state->column_widths) - 1; ++i) {
            column_position += tab_state->column_widths[i];

            ui_fixed_x_next(column_position * container_width);
            UI_Box *handle = ui_create_box_from_string_format(UI_BoxFlag_Clickable, "###resize_handle_%u", i);
            UI_Input input = ui_input_from_box(handle);

            if (input.flags & UI_InputFlag_LeftDragging) {
                if (input.flags & UI_InputFlag_LeftPressed) {
                    F32 min_width = tab_state->column_widths[i + 0];
                    F32 max_width = tab_state->column_widths[i + 1];
                    V2F32 drag_data = v2f32(min_width, max_width);
                    ui_set_drag_data(&drag_data);
                }

                V2F32 drag_data = *ui_get_drag_data(V2F32);

                F32 min_width_percentage_pre_drag = drag_data.x;
                F32 max_width_percentage_pre_drag = drag_data.y;
                F32 min_width_pixels_pre_drag = container_width * min_width_percentage_pre_drag;
                F32 max_width_pixels_pre_drag = container_width * max_width_percentage_pre_drag;

                F32 drag_delta = ui_drag_delta().x;
                F32 clamped_drag_delta = drag_delta;
                if (drag_delta < 0.0f) {
                    clamped_drag_delta = -f32_min(-drag_delta, min_width_pixels_pre_drag);
                } else {
                    clamped_drag_delta = f32_min(drag_delta, max_width_pixels_pre_drag);
                }

                F32 min_width_pixels_post_drag = min_width_pixels_pre_drag + clamped_drag_delta;
                F32 max_width_pixels_post_drag = max_width_pixels_pre_drag - clamped_drag_delta;
                F32 min_width_percentage_post_drag = min_width_pixels_post_drag / container_width;
                F32 max_width_percentage_post_drag = max_width_pixels_post_drag / container_width;
                tab_state->column_widths[i + 0] = min_width_percentage_post_drag;
                tab_state->column_widths[i + 1] = max_width_percentage_post_drag;
            }
        }

        // NOTE(simon): Build rows.
        for (S64 i = visible_range.min; i < visible_range.max; ++i) {
            Pipewire_Property *property = properties[i];

            // NOTE(simon): Use heuristics to determine if the property is a reference to another object.
            Pipewire_Object *reference = &pipewire_nil_object;
            Str8 last_component = str8_skip(property->name, 1 + str8_last_index_of(property->name, '.'));
            if (
                str8_equal(last_component, str8_literal("id")) ||
                str8_equal(last_component, str8_literal("client")) ||
                str8_equal(last_component, str8_literal("device")) ||
                str8_equal(last_component, str8_literal("node")) ||
                str8_equal(last_component, str8_literal("port"))
            ) {
                U32 id = pipewire_object_property_u32_from_name(selected_object, property->name);
                reference = pipewire_object_from_id(id);
            }

            ui_width(ui_size_parent_percent(1.0f, 1.0f))
            ui_row() {
                ui_width_next(ui_size_parent_percent(tab_state->column_widths[0], 1.0f));
                ui_extra_box_flags_next(UI_BoxFlag_Clip | UI_BoxFlag_DrawBorder);
                ui_row() {
                    ui_width_next(ui_size_text_content(0.0f, 1.0f));
                    ui_label(property->name);
                }

                ui_width_next(ui_size_parent_percent(tab_state->column_widths[1], 1.0f));
                ui_extra_box_flags_next(UI_BoxFlag_DrawBorder);
                ui_row() {
                    ui_width_next(ui_size_fill());
                    // NOTE(simon): Create a button if we are a reference.
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
}

internal BUILD_TAB_FUNCTION(build_parameter_tab) {
    typedef struct Expansion Expansion;
    struct Expansion {
        Expansion *next;
        Expansion *previous;

        U64 hash;
    };
    typedef struct ExpansionList ExpansionList;
    struct ExpansionList {
        Expansion *first;
        Expansion *last;
    };
    typedef struct TabState TabState;
    struct TabState {
        UI_ScrollPosition scroll_position;
        F32 column_widths[3];

        ExpansionList *expansion_table;
        U64 expansion_table_size;
        Expansion *expansion_freelist;
    };

    B32 is_new_tab = !tab_from_handle(top_context()->tab)->state;
    TabState *tab_state = tab_state_from_type(TabState);

    if (is_new_tab) {
        tab_state->column_widths[0] = 1.0f / 3.0f;
        tab_state->column_widths[1] = 1.0f / 3.0f;
        tab_state->column_widths[2] = 1.0f / 3.0f;

        tab_state->expansion_table_size = 512;
        tab_state->expansion_table = arena_push_array(tab_from_handle(top_context()->tab)->arena, ExpansionList, tab_state->expansion_table_size);
    }

    Pipewire_Object *selected_object = pipewire_object_from_handle(state->selected_object);

    // NOTE(simon): Collect all parameters.
    typedef struct Row Row;
    struct Row {
        Row *next;
        Str8 label;
        Str8 value;
        Str8 type;
        U64  depth;
        B32  is_expandable;
        U64  hash;
    };

    Row *rows = 0;
    S64  row_count = 0;
    {
        typedef struct Work Work;
        struct Work {
            Work *next;

            // NOTE(simon): Display data set by parent.
            Str8 label;
            U64  depth;

            // NOTE(simon): Raw SPA data.
            Void *body;
            U32   type;
            U32   size;
            const struct spa_type_info *type_info;

            // NOTE(simon): Unique hash for this item.
            U64 hash;
        };

        // NOTE(simon): Queue up all parameters.
        Work *first_work = 0;
        Work *last_work = 0;
        for (Pipewire_Parameter *parameter = selected_object->first_parameter; parameter; parameter = parameter->next) {
            Work *work = arena_push_struct(frame_arena(), Work);
            work->label = str8_cstr((CStr) spa_debug_type_find_short_name(spa_type_param, parameter->id));
            work->body = SPA_POD_BODY(parameter->param);
            work->type = parameter->param->type;
            work->size = parameter->param->size;
            work->type_info = spa_debug_type_find(SPA_TYPE_ROOT, parameter->param->type);
            work->hash = u64_hash(parameter->id);
            sll_queue_push(first_work, last_work, work);
        }

        // NOTE(simon): Generate one row for each work item and potentially
        // generate more work for children.
        Row *first_row = 0;
        Row *last_row = 0;
        while (first_work) {
            Work *work = first_work;
            sll_queue_pop(first_work, last_work);

            // NOTE(simon): Find expansion state.
            Expansion *expanded = 0;
            ExpansionList *expansion_list = &tab_state->expansion_table[work->hash % tab_state->expansion_table_size];
            for (Expansion *expansion = expansion_list->first; expansion; expansion = expansion->next) {
                if (expansion->hash == work->hash) {
                    expanded = expansion;
                    break;
                }
            }

            // NOTE(simon): Work list to generate children.
            Work *first_member_work = 0;
            Work *last_member_work  = 0;

            // NOTE(simon): Create the row and fill with default data.
            Row *row = arena_push_struct(frame_arena(), Row);
            row->label = work->label;
            row->depth = work->depth;
            row->type  = str8_cstr((CStr) spa_debug_type_short_name(spa_debug_type_find(SPA_TYPE_ROOT, work->type)->name));
            row->value = str8_literal("???");
            row->hash  = work->hash;

            // NOTE(simon): Fill out row and generate member work based on
            // type.
            if (work->size >= pipewire_spa_pod_min_type_size(work->type)) {
                switch (work->type) {
                    case SPA_TYPE_None: {
                        row->value = str8_literal("None");
                    } break;
                    case SPA_TYPE_Bool: {
                        S32 value = *(S32 *) work->body;
                        row->value = value ? str8_literal("True") : str8_literal("False");
                    } break;
                    case SPA_TYPE_Id: {
                        U32 value = *(U32 *) work->body;

                        struct spa_type_info *info = 0;
                        if (work->type_info && work->type_info->values) {
                            info = (struct spa_type_info *) spa_debug_type_find(work->type_info->values, value);
                        }

                        if (info) {
                            Str8 enum_name = str8_cstr((CStr) spa_debug_type_short_name(info->name));
                            row->value = str8_format(frame_arena(), "%.*s (%u)", str8_expand(enum_name), value);
                        } else {
                            row->value = str8_format(frame_arena(), "%u", value);
                        }
                    } break;
                    case SPA_TYPE_Int: {
                        S32 value = *(S32 *) work->body;
                        row->value = str8_format(frame_arena(), "%d", value);
                    } break;
                    case SPA_TYPE_Long: {
                        S64 value = *(S64 *) work->body;
                        row->value = str8_format(frame_arena(), "%ld", value);
                    } break;
                    case SPA_TYPE_Float: {
                        F32 value = *(F32 *) work->body;
                        row->value = str8_format(frame_arena(), "%f", value);
                    } break;
                    case SPA_TYPE_Double: {
                        F64 value = *(F64 *) work->body;
                        row->value = str8_format(frame_arena(), "%f", value);
                    } break;
                    case SPA_TYPE_String: {
                        Str8 value = str8(work->body, work->size);
                        row->value = value;
                    } break;
                    case SPA_TYPE_Bytes: {
                        row->value = str8_literal("TODO: Not implemented yet");
                    } break;
                    case SPA_TYPE_Rectangle: {
                        struct spa_rectangle *value = (struct spa_rectangle *) work->body;
                        row->value = str8_format(frame_arena(), "%u x %u", value->width, value->height);
                    } break;
                    case SPA_TYPE_Fraction: {
                        struct spa_fraction *value = (struct spa_fraction *) work->body;
                        row->value = str8_format(frame_arena(), "%u / %u", value->num, value->denom);
                    } break;
                    case SPA_TYPE_Bitmap: {
                        row->value = str8_literal("TODO: Not implemented yet");
                    } break;
                    case SPA_TYPE_Array: {
                        struct spa_pod_array_body *value = (struct spa_pod_array_body *) work->body;
                        row->value = str8_literal("[ ... ]");
                        row->is_expandable = true;

                        // NOTE(simon): Generate work for all children.
                        U64 index = 0;
                        Void *child = 0;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
                        SPA_POD_ARRAY_BODY_FOREACH(value, work->size, child) {
#pragma clang diagnostic pop
                            Work *child_work = arena_push_struct(frame_arena(), Work);
                            child_work->label = str8_format(frame_arena(), "[%lu]", index);
                            child_work->depth = work->depth + 1;
                            child_work->body = child;
                            child_work->type = value->child.type;
                            child_work->size = value->child.size;
                            child_work->type_info = work->type_info->values;
                            child_work->hash = hash_combine(work->hash, u64_hash(index));
                            sll_queue_push(first_member_work, last_member_work, child_work);
                            ++index;
                        }
                    } break;
                    case SPA_TYPE_Struct: {
                        row->value = str8_literal("[ ... ]");
                        row->is_expandable = true;

                        // NOTE(simon): Generate work for all children.
                        struct spa_pod *child = 0;
                        U64 index = 0;
                        SPA_POD_FOREACH(work->body, work->size, child) {
                            Work *child_work = arena_push_struct(frame_arena(), Work);
                            child_work->label = str8_format(frame_arena(), "%lu", index);
                            child_work->depth = work->depth + 1;
                            child_work->body = SPA_POD_BODY(child);
                            child_work->type = child->type;
                            child_work->size = child->size;
                            child_work->type_info = spa_debug_type_find(SPA_TYPE_ROOT, child->type);
                            child_work->hash = hash_combine(work->hash, u64_hash(index));
                            sll_queue_push(first_member_work, last_member_work, child_work);
                            ++index;
                        }
                    } break;
                    case SPA_TYPE_Object: {
                        struct spa_pod_object_body *value = (struct spa_pod_object_body *) work->body;
                        const struct spa_type_info *type_info = spa_debug_type_find(SPA_TYPE_ROOT, value->type);
                        row->value = str8_literal("[ ... ]");
                        row->is_expandable = true;
                        row->type = str8_cstr((CStr) spa_debug_type_short_name(type_info->name));

                        // NOTE(simon): Generate work for all children.
                        struct spa_pod_prop *prop = 0;
                        U64 index = 0;
                        SPA_POD_OBJECT_BODY_FOREACH(value, work->size, prop) {
                            struct spa_type_info *child_type = (struct spa_type_info *) spa_debug_type_find(type_info->values, prop->key);
                            Work *child_work = arena_push_struct(frame_arena(), Work);

                            // NOTE(simon): Try to find the property name from
                            // the type information.
                            if (child_type) {
                                child_work->label = str8_cstr((CStr) spa_debug_type_short_name(child_type->name));
                            } else if (prop->key >= SPA_PROP_START_CUSTOM) {
                                child_work->label = str8_format(frame_arena(), "Custom %u", prop->key - SPA_PROP_START_CUSTOM);
                            } else {
                                child_work->label = str8_literal("Unknown");
                            }

                            child_work->depth = work->depth + 1;
                            child_work->body = SPA_POD_CONTENTS(struct spa_pod_prop, prop);
                            child_work->type = prop->value.type;
                            child_work->size = prop->value.size;
                            child_work->type_info = child_type ? child_type : spa_debug_type_find(SPA_TYPE_ROOT, prop->value.type);
                            child_work->hash = hash_combine(work->hash, u64_hash(index));
                            sll_queue_push(first_member_work, last_member_work, child_work);
                            ++index;
                        }
                    } break;
                    case SPA_TYPE_Sequence: {
                        row->value = str8_literal("TODO: Not implemented yet");
                    } break;
                    case SPA_TYPE_Pointer: {
                        row->value = str8_literal("TODO: Not implemented yet");
                    } break;
                    case SPA_TYPE_Fd: {
                        row->value = str8_literal("TODO: Not implemented yet");
                    } break;
                    case SPA_TYPE_Choice: {
                        struct spa_pod_choice_body *value = (struct spa_pod_choice_body *) work->body;
                        row->value = str8_literal("[ ... ]");
                        row->is_expandable = true;

                        Str8 range_labels[] = { str8_literal_compile("default"), str8_literal_compile("min"), str8_literal_compile("max"), };
                        Str8 step_labels[]  = { str8_literal_compile("default"), str8_literal_compile("min"), str8_literal_compile("max"), str8_literal_compile("step"), };
                        Str8 enum_labels[]  = { str8_literal_compile("default"), };

                        Str8 *labels = 0;
                        U64   label_count = 0;

                        // NOTE(simon): Modify the type and set custom labels.
                        switch (value->type) {
                            case SPA_CHOICE_None: {
                                row->type = str8_literal("Choice:None");
                            } break;
                            case SPA_CHOICE_Range: {
                                labels = range_labels;
                                label_count = array_count(range_labels);
                                row->type = str8_literal("Choice:Range");
                            } break;
                            case SPA_CHOICE_Step: {
                                labels = step_labels;
                                label_count = array_count(step_labels);
                                row->type = str8_literal("Choice:Step");
                            } break;
                            case SPA_CHOICE_Enum: {
                                labels = enum_labels;
                                label_count = array_count(enum_labels);
                                row->type = str8_literal("Choice:Enum");
                            } break;
                            case SPA_CHOICE_Flags: {
                                row->type = str8_literal("Choice:Flags");
                            } break;
                            default: {
                                row->type = str8_literal("Choice:Unknown");
                            } break;
                        }

                        // NOTE(simon): Generate work for all children.
                        U64 index = 0;
                        Void *child = 0;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
                        SPA_POD_CHOICE_BODY_FOREACH(value, work->size, child) {
#pragma clang diagnostic pop
                            Work *child_work = arena_push_struct(frame_arena(), Work);
                            if (index < label_count) {
                                child_work->label = labels[index];
                            } else {
                                child_work->label = str8_format(frame_arena(), "%lu", index - label_count);
                            }
                            child_work->depth = work->depth + 1;
                            child_work->body = child;
                            child_work->type_info = work->type_info;
                            child_work->type = value->child.type;
                            child_work->size = value->child.size;
                            child_work->hash = hash_combine(work->hash, u64_hash(index));
                            sll_queue_push(first_member_work, last_member_work, child_work);
                            ++index;
                        }

                    } break;
                    case SPA_TYPE_Pod: {
                        row->value = str8_literal("TODO: Not implemented yet");
                    } break;
                }
            }

            // NOTE(simon): Add generated row to output.
            sll_queue_push(first_row, last_row, row);
            ++row_count;

            // NOTE(simon): Add members as new work if we are expanded.
            if (expanded && first_member_work) {
                last_member_work->next = first_work;
                first_work = first_member_work;
            }
        }

        // NOTE(simon): Flatten generated rows to array.
        rows = arena_push_array(frame_arena(), Row, (U64) row_count);
        for (Row *row = first_row, *row_ptr = rows; row; row = row->next) {
            *row_ptr++ = *row;
        }
    }

    V2F32 tab_size   = r2f32_size(tab_rectangle);
    F32   row_height = 2.0f * (F32) ui_font_size_top();

    R1S64 visible_range = { 0 };
    ui_palette(palette_from_theme(ThemePalette_Button))
    ui_font(font_cache_font_from_static_data(&mono_font))
    ui_scroll_region(tab_size, row_height, row_count, &visible_range, 0, &tab_state->scroll_position)
    ui_text_x_padding(5.0f)
    ui_palette(palette_from_theme(ThemePalette_Base)) {
        // NOTE(simon): Build column resize handles
        F32 column_position = 0.0f;
        F32 container_width = ui_parent_top()->size[Axis2_X].value;
        ui_fixed_y(0.0f)
        ui_width(ui_size_pixels(5.0f, 1.0f))
        ui_height(ui_size_pixels(row_height * (F32) r1s64_size(visible_range), 1.0f))
        ui_hover_cursor(Gfx_Cursor_SizeWE)
        for (U32 i = 0; i < array_count(tab_state->column_widths) - 1; ++i) {
            column_position += tab_state->column_widths[i];

            ui_fixed_x_next(column_position * container_width);
            UI_Box *handle = ui_create_box_from_string_format(UI_BoxFlag_Clickable, "###resize_handle_%u", i);
            UI_Input input = ui_input_from_box(handle);

            if (input.flags & UI_InputFlag_LeftDragging) {
                if (input.flags & UI_InputFlag_LeftPressed) {
                    F32 min_width = tab_state->column_widths[i + 0];
                    F32 max_width = tab_state->column_widths[i + 1];
                    V2F32 drag_data = v2f32(min_width, max_width);
                    ui_set_drag_data(&drag_data);
                }

                V2F32 drag_data = *ui_get_drag_data(V2F32);

                F32 min_width_percentage_pre_drag = drag_data.x;
                F32 max_width_percentage_pre_drag = drag_data.y;
                F32 min_width_pixels_pre_drag = container_width * min_width_percentage_pre_drag;
                F32 max_width_pixels_pre_drag = container_width * max_width_percentage_pre_drag;

                F32 drag_delta = ui_drag_delta().x;
                F32 clamped_drag_delta = drag_delta;
                if (drag_delta < 0.0f) {
                    clamped_drag_delta = -f32_min(-drag_delta, min_width_pixels_pre_drag);
                } else {
                    clamped_drag_delta = f32_min(drag_delta, max_width_pixels_pre_drag);
                }

                F32 min_width_pixels_post_drag = min_width_pixels_pre_drag + clamped_drag_delta;
                F32 max_width_pixels_post_drag = max_width_pixels_pre_drag - clamped_drag_delta;
                F32 min_width_percentage_post_drag = min_width_pixels_post_drag / container_width;
                F32 max_width_percentage_post_drag = max_width_pixels_post_drag / container_width;
                tab_state->column_widths[i + 0] = min_width_percentage_post_drag;
                tab_state->column_widths[i + 1] = max_width_percentage_post_drag;
            }
        }

        // NOTE(simon): Build rows.
        for (S64 i = visible_range.min; i < visible_range.max; ++i) {
            Row *row = &rows[i];

            // NOTE(simon): Find expansion state.
            Expansion *expanded = 0;
            ExpansionList *expansion_list = &tab_state->expansion_table[row->hash % tab_state->expansion_table_size];
            for (Expansion *expansion = expansion_list->first; expansion; expansion = expansion->next) {
                if (expansion->hash == row->hash) {
                    expanded = expansion;
                    break;
                }
            }

            ui_width_next(ui_size_fill());
            ui_row() {
                // NOTE(simon): Build label.
                ui_width_next(ui_size_parent_percent(tab_state->column_widths[0], 1.0f));
                ui_layout_axis_next(Axis2_X);
                ui_hover_cursor_next(Gfx_Cursor_Hand);
                UI_Box *label_box = ui_create_box_from_string_format(
                    UI_BoxFlag_DrawBorder | UI_BoxFlag_Clip |
                    (row->is_expandable ? UI_BoxFlag_Clickable : 0),
                    "###paramter_%lu", row->hash
                );
                ui_parent(label_box) {
                    // NOTE(simon): Indentation due to nesting depth.
                    ui_spacer_sized(ui_size_ems((F32) row->depth, 1.0f));

                    // NOTE(simon): Expansion button or spacer depending on
                    // if we are expandable or not.
                    ui_width_next(ui_size_ems(1.5f, 1.0f));
                    ui_text_align_next(UI_TextAlign_Center);
                    ui_font_next(ui_icon_font());
                    if (row->is_expandable) {
                        ui_label(ui_icon_string_from_kind(expanded ? UI_IconKind_DownAngle : UI_IconKind_RightAngle));
                    } else {
                        ui_spacer();
                    }

                    ui_width_next(ui_size_text_content(0.0f, 1.0f));
                    ui_label(row->label);
                }

                UI_Input label_input = ui_input_from_box(label_box);

                // NOTE(simon): Handle expansion.
                if (row->is_expandable) {
                    if (label_input.flags & UI_InputFlag_Clicked) {
                        if (expanded) {
                            dll_remove(expansion_list->first, expansion_list->last, expanded);
                            sll_stack_push(tab_state->expansion_freelist, expanded);
                        } else {
                            expanded = tab_state->expansion_freelist;
                            if (expanded) {
                                sll_stack_pop(tab_state->expansion_freelist);
                                memory_zero_struct(expanded);
                            } else {
                                expanded = arena_push_struct(tab_from_handle(top_context()->tab)->arena, Expansion);
                            }
                            expanded->hash = row->hash;
                            dll_push_back(expansion_list->first, expansion_list->last, expanded);
                        }
                    }
                }

                // NOTE(simon): Build value.
                ui_width_next(ui_size_parent_percent(tab_state->column_widths[1], 1.0f));
                ui_extra_box_flags_next(UI_BoxFlag_Clip | UI_BoxFlag_DrawBorder);
                ui_row() {
                    ui_width_next(ui_size_text_content(0.0f, 1.0f));
                    ui_label(row->value);
                }

                // NOTE(simon): Build type.
                ui_width_next(ui_size_parent_percent(tab_state->column_widths[2], 1.0f));
                ui_extra_box_flags_next(UI_BoxFlag_DrawBorder);
                ui_row() {
                    ui_width_next(ui_size_text_content(0.0f, 1.0f));
                    ui_label(row->type);
                }
            }
        }
    }
}

internal V4F32 color_from_port_media_type(Pipewire_Object *object) {
    // NOTE(simon): Find Format or EnumFormat parameter.
    struct spa_pod *format_parameter = 0;
    if (object->kind == Pipewire_Object_Port) {
        format_parameter = pipewire_object_parameter_from_id(object, SPA_PARAM_Format)->param;
        if (!format_parameter) {
            // NOTE(simon): This assumes that a port cannot support multiple
            // differnet media types, only different subtypes.
            format_parameter = pipewire_object_parameter_from_id(object, SPA_PARAM_EnumFormat)->param;
        }
    }

    // NOTE(simon): Find the media type property POD.
    const struct spa_pod *media_type_pod = 0;
    if (format_parameter) {
        const struct spa_pod_prop *media_type_property = spa_pod_find_prop(format_parameter, 0, SPA_FORMAT_mediaType);
        if (media_type_property) {
            media_type_pod = &media_type_property->value;
        }
    }

    // NOTE(simon): Extract media type.
    U32 media_type = SPA_MEDIA_TYPE_unknown;
    if (media_type_pod) {
        U32 media_type_count = 0;
        U32 choice = SPA_CHOICE_None;
        struct spa_pod *media_types = spa_pod_get_values(media_type_pod, &media_type_count, &choice);

        if (media_types->type == SPA_TYPE_Id && media_type_count >= 1) {
            media_type = *(U32 *) SPA_POD_BODY(media_types);
        }
    }

    // NOTE(simon): Pick color based on media type.
    V4F32 port_color = color_from_theme(ThemeColor_PortUnknown);
    switch (media_type) {
        case SPA_MEDIA_TYPE_unknown:     port_color = color_from_theme(ThemeColor_PortUnknown);     break;
        case SPA_MEDIA_TYPE_audio:       port_color = color_from_theme(ThemeColor_PortAudio);       break;
        case SPA_MEDIA_TYPE_video:       port_color = color_from_theme(ThemeColor_PortVideo);       break;
        case SPA_MEDIA_TYPE_image:       port_color = color_from_theme(ThemeColor_PortImage);       break;
        case SPA_MEDIA_TYPE_binary:      port_color = color_from_theme(ThemeColor_PortBinary);      break;
        case SPA_MEDIA_TYPE_stream:      port_color = color_from_theme(ThemeColor_PortStream);      break;
        case SPA_MEDIA_TYPE_application: port_color = color_from_theme(ThemeColor_PortApplication); break;
    }

    return port_color;
}

internal BUILD_TAB_FUNCTION(build_graph_tab) {
    typedef struct GraphNode GraphNode;
    struct GraphNode {
        GraphNode *next;
        GraphNode *previous;
        V2F32      position;

        U64 last_frame_touched;

        Pipewire_Handle handle;
    };

    typedef struct TabState TabState;
    struct TabState {
        V2F32 graph_offset;

        GraphNode *first_node;
        GraphNode *last_node;
        GraphNode *node_freelist;
    };

    TabState *tab_state = tab_state_from_type(TabState);

    // NOTE(simon): Collect pipewire objects.
    S64 object_count = { 0 };
    Pipewire_Object **objects = 0;
    {
        for (Pipewire_Object *object = pipewire_state->first_object; object; object = object->all_next) {
            ++object_count;
        }

        objects = arena_push_array_no_zero(frame_arena(), Pipewire_Object *, (U64) object_count);
        Pipewire_Object **object_ptr = objects;
        for (Pipewire_Object *object = pipewire_state->first_object; object; object = object->all_next) {
            *object_ptr = object;
            ++object_ptr;
        }
    }

    V2F32 tab_size    = r2f32_size(tab_rectangle);
    F32   row_height  = 2.0f * (F32) ui_font_size_top();
    F32   port_radius = ui_size_ems(0.5f, 1.0f).value;

    // NOTE(simon): Build graph.
    ui_width_next(ui_size_pixels(tab_size.width, 1.0f));
    ui_height_next(ui_size_pixels(tab_size.height, 1.0f));
    UI_Box *node_graph_box = ui_create_box_from_string(UI_BoxFlag_Clip | UI_BoxFlag_Clickable | UI_BoxFlag_Scrollable, str8_literal("###node_graph"));
    ui_text_x_padding(5.0f + port_radius)
    ui_palette(palette_from_theme(ThemePalette_Button))
    ui_parent(node_graph_box) {
        V2F32 no_ports_offset     = v2f32(0.0f * 20.0f * (F32) ui_font_size_top(), 0.0f);
        V2F32 only_output_offset  = v2f32(1.0f * 20.0f * (F32) ui_font_size_top(), 0.0f);
        V2F32 input_output_offset = v2f32(2.0f * 20.0f * (F32) ui_font_size_top(), 0.0f);
        V2F32 only_inputs_offset  = v2f32(3.0f * 20.0f * (F32) ui_font_size_top(), 0.0f);

        typedef struct PortNode PortNode;
        struct PortNode {
            PortNode        *next;
            PortNode        *previous;
            V2F32            position;
            Pipewire_Object *port;
        };
        PortNode *first_port = 0;
        PortNode *last_port  = 0;

        for (S64 i = 0; i < object_count; ++i) {
            Pipewire_Object *node = objects[i];
            if (node->kind != Pipewire_Object_Node) {
                continue;
            }

            // NOTE(simon): Count number of input and output ports and
            // determine the maximum width of input and output names.
            U32 input_port_count  = 0;
            U32 output_port_count = 0;
            F32 input_port_name_max_width  = 0.0f;
            F32 output_port_name_max_width = 0.0f;
            for (Pipewire_Object *child = node->first; !pipewire_object_is_nil(child); child = child->next) {
                Str8 direction = pipewire_object_property_string_from_name(child, str8_literal("port.direction"));
                Str8 port_name = pipewire_object_property_string_from_name(child, str8_literal("port.name"));
                F32 port_name_width = font_cache_size_from_font_text_size(ui_font_top(), port_name, ui_font_size_top()).width + 2.0f * ui_text_x_padding_top();
                if (str8_equal(direction, str8_literal("in"))) {
                    ++input_port_count;
                    input_port_name_max_width = f32_max(input_port_name_max_width, port_name_width);
                } else if (str8_equal(direction, str8_literal("out"))) {
                    ++output_port_count;
                    output_port_name_max_width = f32_max(output_port_name_max_width, port_name_width);
                }
            }

            Str8 node_name = name_from_object(node);
            F32 node_name_width = font_cache_size_from_font_text_size(ui_font_top(), node_name, ui_font_size_top()).width + 2.0f * ui_text_x_padding_top();

            // NOTE(simon): Calculate node size.
            F32 node_width = f32_max(input_port_name_max_width + output_port_name_max_width, node_name_width);
            F32 node_height = row_height * (F32) (1 + u32_max(input_port_count, output_port_count));

            // NOTE(simon): Grab y offset depending on which ports are
            // available and increment for next one.
            V2F32 default_position = { 0 };
            if (input_port_count == 0 && output_port_count == 0) {
                default_position = no_ports_offset;
                no_ports_offset.y += node_height + row_height;
            } else if (input_port_count == 0 && output_port_count != 0) {
                default_position = only_output_offset;
                only_output_offset.y += node_height + row_height;
            } else if (input_port_count != 0 && output_port_count == 0) {
                default_position = only_inputs_offset;
                only_inputs_offset.y += node_height + row_height;
            } else if (input_port_count != 0 && output_port_count != 0) {
                default_position = input_output_offset;
                input_output_offset.y += node_height + row_height;
            }

            // NOTE(simon): Grab cached node state or create a new one with
            // default values.
            GraphNode *graph_node = 0;
            for (GraphNode *candidate = tab_state->first_node; candidate; candidate = candidate->next) {
                if (pipewire_object_from_handle(candidate->handle) == node) {
                    graph_node = candidate;
                    break;
                }
            }
            if (!graph_node) {
                graph_node = tab_state->node_freelist;
                if (graph_node) {
                    sll_stack_pop(tab_state->node_freelist);
                    memory_zero_struct(graph_node);
                } else {
                    graph_node = arena_push_struct(tab_from_handle(top_context()->tab)->arena, GraphNode);
                }
                graph_node->handle = pipewire_handle_from_object(node);
                graph_node->position = default_position;
                dll_push_back(tab_state->first_node, tab_state->last_node, graph_node);
            }
            graph_node->last_frame_touched = state->frame_index;

            if (pipewire_object_from_handle(state->hovered_object) == node) {
                UI_Palette palette = ui_palette_top();
                palette.background = color_from_theme(ThemeColor_Focus);
                ui_palette_next(palette);
            }

            // NOTE(simon): Build node.
            ui_corner_radius_next(5.0f);
            ui_fixed_position_next(v2f32_subtract(graph_node->position, tab_state->graph_offset));
            ui_width_next(ui_size_pixels(node_width, 1.0f));
            ui_height_next(ui_size_pixels(node_height, 1.0f));
            ui_layout_axis_next(Axis2_Y);
            UI_Box *node_box = ui_create_box_from_string_format(UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawDropShadow | UI_BoxFlag_Clickable, "###node_%u", node->id);
            ui_parent(node_box)
            ui_width(ui_size_fill())
            ui_height(ui_size_pixels(row_height, 1.0f)) {
                ui_text_align_next(UI_TextAlign_Center);
                ui_label(name_from_object(node));

                // NOTE(simon): Build port columns.
                UI_Box *input_column  = &global_ui_null_box;
                UI_Box *output_column = &global_ui_null_box;
                ui_height(ui_size_fill())
                ui_row()
                ui_layout_axis(Axis2_Y) {
                    ui_width_next(ui_size_pixels(input_port_name_max_width, 1.0f));
                    input_column = ui_create_box(0);
                    ui_spacer_sized(ui_size_fill());
                    ui_width_next(ui_size_pixels(output_port_name_max_width, 1.0f));
                    output_column = ui_create_box(0);
                }

                // NOTE(simon): Build ports.
                U32 input_port_index = 0;
                U32 output_port_index = 0;
                for (Pipewire_Object *child = node->first; !pipewire_object_is_nil(child); child = child->next) {
                    PortNode *port_node = arena_push_struct(frame_arena(), PortNode);
                    port_node->port = child;

                    Str8 direction = pipewire_object_property_string_from_name(child, str8_literal("port.direction"));
                    Str8 port_name = pipewire_object_property_string_from_name(child, str8_literal("port.name"));

                    V2F32 local_position = { 0 };
                    local_position.y = 0.5f * row_height;
                    U32 port_index = 0;
                    if (str8_equal(direction, str8_literal("in"))) {
                        ui_parent_next(input_column);
                        ui_text_align_next(UI_TextAlign_Left);
                        local_position.x = 0.0f;
                        port_index = input_port_index++;
                        port_node->position = v2f32(
                            graph_node->position.x + 0.0f,
                            graph_node->position.y + local_position.y + (F32) (1 + port_index) * row_height
                        );
                    } else if (str8_equal(direction, str8_literal("out"))) {
                        ui_parent_next(output_column);
                        ui_text_align_next(UI_TextAlign_Right);
                        local_position.x = output_port_name_max_width;
                        port_index = output_port_index++;
                        port_node->position = v2f32(
                            graph_node->position.x + node_width,
                            graph_node->position.y + local_position.y + (F32) (1 + port_index) * row_height
                        );
                    }
                    UI_Box *label = ui_label(port_name);

                    UI_Input port_input = { 0 };
                    ui_parent(label) {
                        UI_Palette palette = { 0 };
                        palette.background = color_from_port_media_type(child);
                        ui_palette_next(palette);
                        ui_fixed_x_next(local_position.x - port_radius);
                        ui_fixed_y_next(local_position.y - port_radius);
                        ui_width_next(ui_size_pixels(2.0f * port_radius, 1.0f));
                        ui_height_next(ui_size_pixels(2.0f * port_radius, 1.0f));
                        ui_corner_radius_next(port_radius);
                        UI_Box *port = ui_create_box_from_string_format(UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawHot | UI_BoxFlag_DrawActive | UI_BoxFlag_Clickable | UI_BoxFlag_DropTarget, "###port_%p", child);
                        port_input = ui_input_from_box(port);
                    }

                    if (port_input.flags & UI_InputFlag_Dragging && v2f32_length(ui_drag_delta()) > 10.0f) {
                        context_scope(.port = pipewire_handle_from_object(child)) {
                            drag_begin(ContextMember_Port);
                        }
                    }

                    if (drag_is_active() && state->drag_context_member == ContextMember_Port && ui_drop_hot_key() == port_input.box->key) {
                        if (drag_drop()) {
                            Pipewire_Object *output_port = pipewire_object_from_handle(state->drag_context->port);
                            Pipewire_Object *input_port  = child;

                            Str8 output_direction = pipewire_object_property_string_from_name(output_port, str8_literal("port.direction"));
                            Str8 input_direction  = pipewire_object_property_string_from_name(input_port,  str8_literal("port.direction"));

                            if (str8_equal(input_direction, str8_literal("out"))) {
                                swap(input_port,      output_port,      Pipewire_Object *);
                                swap(input_direction, output_direction, Str8);
                            }

                            Pipewire_Object *existing_link = &pipewire_nil_object;
                            for (S64 j = 0; j < object_count; ++j) {
                                Pipewire_Object *link = objects[j];
                                if (link->kind != Pipewire_Object_Link) {
                                    continue;
                                }

                                U32 output_port_id = pipewire_object_property_u32_from_name(link, str8_literal("link.output.port"));
                                U32 input_port_id  = pipewire_object_property_u32_from_name(link, str8_literal("link.input.port"));

                                if (output_port_id == output_port->id && input_port_id == input_port->id) {
                                    existing_link = link;
                                }
                            }

                            if (str8_equal(output_direction, str8_literal("out")) && str8_equal(input_direction, str8_literal("in"))) {
                                if (!pipewire_object_is_nil(existing_link)) {
                                    pipewire_remove(pipewire_handle_from_object(existing_link));
                                } else {
                                    pipewire_link(pipewire_handle_from_object(output_port), pipewire_handle_from_object(input_port));
                                }
                            }
                        }
                    }

                    dll_push_back(first_port, last_port, port_node);
                }
            }

            UI_Input node_input = ui_input_from_box(node_box);
            if (node_input.flags & UI_InputFlag_Hovering) {
                state->hovered_object_next = pipewire_handle_from_object(node);
            }
            if (node_input.flags & UI_InputFlag_RightClicked) {
                state->selected_object_next = pipewire_handle_from_object(node);
            }
            if (node_input.flags & UI_InputFlag_LeftDragging) {
                if (node_input.flags & UI_InputFlag_LeftPressed) {
                    V2F32 drag_data = graph_node->position;
                    ui_set_drag_data(&drag_data);
                }

                V2F32 position_pre_drag = *ui_get_drag_data(V2F32);
                V2F32 position_post_drag = v2f32_add(position_pre_drag, ui_drag_delta());
                graph_node->position = position_post_drag;
            }
        }

        // NOTE(simon): Draw connections.
        Draw_List *connections = draw_list_create();
        draw_list_scope(connections)
        for (S64 i = 0; i < object_count; ++i) {
            Pipewire_Object *node = objects[i];
            if (node->kind != Pipewire_Object_Link) {
                continue;
            }

            // NOTE(simon): Find input and output ports.
            PortNode *output_port = 0;
            PortNode *input_port  = 0;
            U32 output_port_id = pipewire_object_property_u32_from_name(node, str8_literal("link.output.port"));
            U32 input_port_id  = pipewire_object_property_u32_from_name(node, str8_literal("link.input.port"));
            for (PortNode *port_node = first_port; port_node; port_node = port_node->next) {
                if (port_node->port->id == output_port_id) {
                    output_port = port_node;
                } else if (port_node->port->id == input_port_id) {
                    input_port = port_node;
                }
            }

            // NOTE(simon): Draw two quadratic beziers to approximmate the
            // look of a cubic beizer between ports.
            if (output_port && input_port) {
                V4F32 link_color = color_from_port_media_type(output_port->port);
                V2F32 output_point = v2f32_subtract(output_port->position, tab_state->graph_offset);
                V2F32 input_point  = v2f32_subtract(input_port->position, tab_state->graph_offset);
                V2F32 middle       = v2f32_scale(v2f32_add(input_point, output_point), 0.5f);
                V2F32 c0_control   = v2f32(output_point.x + f32_abs(input_point.x - output_point.x) * 0.25f, output_point.y);
                V2F32 c1_control   = v2f32(input_point.x  - f32_abs(input_point.x - output_point.x) * 0.25f, input_point.y);
                draw_bezier(output_point, c0_control, middle,      link_color, 2.0f, 1.0f, 1.0f);
                draw_bezier(middle,       c1_control, input_point, link_color, 2.0f, 1.0f, 1.0f);
            }
        }
        if (drag_is_active() && state->drag_context_member == ContextMember_Port) {
            draw_list_scope(connections) {
                Pipewire_Object *port = pipewire_object_from_handle(state->drag_context->port);
                V2F32 mouse = v2f32_subtract(ui_mouse(), tab_rectangle.min);

                // NOTE(simon): Find port node.
                PortNode *node = 0;
                for (PortNode *port_node = first_port; port_node; port_node = port_node->next) {
                    if (port_node->port->id == port->id) {
                        node = port_node;
                    }
                    if (v2f32_length(v2f32_subtract(mouse, v2f32_subtract(port_node->position, tab_state->graph_offset))) <= port_radius) {
                        mouse = v2f32_subtract(port_node->position, tab_state->graph_offset);
                    }
                }

                // NOTE(simon): Draw two quadratic beziers to approximmate the
                // look of a cubic beizer between ports.
                if (node) {
                    V4F32 link_color = color_from_port_media_type(port);
                    V2F32 output_point = v2f32_subtract(node->position, tab_state->graph_offset);
                    V2F32 input_point  = mouse;

                    Str8 direction = pipewire_object_property_string_from_name(port, str8_literal("port.direction"));
                    if (str8_equal(direction, str8_literal("in"))) {
                        swap(output_point, input_point, V2F32);
                    }

                    V2F32 middle       = v2f32_scale(v2f32_add(input_point, output_point), 0.5f);
                    V2F32 c0_control   = v2f32(output_point.x + f32_abs(input_point.x - output_point.x) * 0.25f, output_point.y);
                    V2F32 c1_control   = v2f32(input_point.x  - f32_abs(input_point.x - output_point.x) * 0.25f, input_point.y);
                    draw_bezier(output_point, c0_control, middle,      link_color, 2.0f, 1.0f, 1.0f);
                    draw_bezier(middle,       c1_control, input_point, link_color, 2.0f, 1.0f, 1.0f);
                }
            }
        }
        ui_box_set_draw_list(node_graph_box, connections);
    }

    UI_Input node_graph_input = ui_input_from_box(node_graph_box);
    tab_state->graph_offset = v2f32_subtract(tab_state->graph_offset, v2f32_scale(node_graph_input.scroll, row_height));
    if (node_graph_input.flags & UI_InputFlag_RightDragging) {
        if (node_graph_input.flags & UI_InputFlag_RightPressed) {
            V2F32 drag_data = tab_state->graph_offset;
            ui_set_drag_data(&drag_data);
        }

        V2F32 position_pre_drag = *ui_get_drag_data(V2F32);
        V2F32 position_post_drag = v2f32_subtract(position_pre_drag, ui_drag_delta());
        tab_state->graph_offset = position_post_drag;
    }

    // NOTE(simon): Recycle nodes that haven't been touched this build.
    for (GraphNode *node = tab_state->first_node, *next = 0; node; node = next) {
        next = node->next;

        if (node->last_frame_touched != state->frame_index) {
            dll_remove(tab_state->first_node, tab_state->last_node, node);
            sll_stack_push(tab_state->node_freelist, node);
        }
    }
}

// NOTE(simon): Based off of PulseAudios guide lines:
//   https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/Clients/WritingVolumeControlUIs/
internal UI_BOX_DRAW_FUNCTION(draw_display) {
    V2F32 *draw_data = (V2F32 *) data;

    F32 min    = 0.0f;
    F32 norm   = 1.0f;
    F32 max    = 2.0f;

    F32 base   = draw_data->x;
    F32 volume = draw_data->y;

    V2F32 min_pos = box->calculated_rectangle.min;
    F32 width  = box->calculated_size.width;
    F32 height = box->calculated_size.height;

    F32 volume_pixels = width * (volume - min) / (max - min);
    F32 green_yellow_border_pixels = width * (base - min) / (max - min);
    F32 yellow_red_border_pixels   = width * (norm - min) / (max - min);

    if (min < base && base < norm) {
        draw_rectangle(
            r2f32(min_pos.x, min_pos.y + height / 2.0f - 5.0f, min_pos.x + green_yellow_border_pixels, min_pos.y + height / 2.0f + 5.0f),
            color_from_theme(ThemeColor_VolumeAttenuate),
            0.0f,
            0.0f,
            0.0f
        );
        draw_rectangle(
            r2f32(min_pos.x + green_yellow_border_pixels, min_pos.y + height / 2.0f - 5.0f, min_pos.x + yellow_red_border_pixels, min_pos.y + height / 2.0f + 5.0f),
            color_from_theme(ThemeColor_VolumeHardwareAmplify),
            0.0f,
            0.0f,
            0.0f
        );
        draw_rectangle(
            r2f32(min_pos.x + yellow_red_border_pixels, min_pos.y + height / 2.0f - 5.0f, min_pos.x + width, min_pos.y + height / 2.0f + 5.0f),
            color_from_theme(ThemeColor_VolumeSoftwareAmplify),
            0.0f,
            0.0f,
            0.0f
        );
    } else {
        draw_rectangle(
            r2f32(min_pos.x, min_pos.y + height / 2.0f - 5.0f, min_pos.x + yellow_red_border_pixels, min_pos.y + height / 2.0f + 5.0f),
            color_from_theme(ThemeColor_VolumeAttenuate),
            0.0f,
            0.0f,
            0.0f
        );
        draw_rectangle(
            r2f32(min_pos.x + yellow_red_border_pixels, min_pos.y + height / 2.0f - 5.0f, min_pos.x + width, min_pos.y + height / 2.0f + 5.0f),
            color_from_theme(ThemeColor_VolumeSoftwareAmplify),
            0.0f,
            0.0f,
            0.0f
        );
    }

    draw_rectangle(
        r2f32(min_pos.x + volume_pixels - 20.0f, min_pos.y, min_pos.x + volume_pixels + 20.0f, min_pos.y + height),
        color_from_theme(ThemeColor_ButtonBackground),
        height / 2.0f,
        0.0f,
        1.0f
    );

    draw_rectangle(
        r2f32(min_pos.x + volume_pixels - 1.0f, min_pos.y, min_pos.x + volume_pixels + 1.0f, min_pos.y + height),
        color_from_theme(ThemeColor_ButtonBorder),
        0.0f,
        0.0f,
        0.0f
    );
}

internal UI_BOX_DRAW_FUNCTION(draw_volume_slider) {
    V2F32 *draw_data = (V2F32 *) data;

    F32 min    = 0.0f;
    F32 norm   = 1.0f;
    F32 max    = 2.0f;

    F32 base   = draw_data->x;
    F32 volume = draw_data->y;

    V2F32 min_pos = box->calculated_rectangle.min;
    F32 width  = box->calculated_size.width;
    F32 height = box->calculated_size.height;

    F32 volume_pixels = (width - 40.0f) * (volume - min) / (max - min);
    F32 green_yellow_border_pixels = (width - 40.0f) * (base - min) / (max - min);
    F32 yellow_red_border_pixels   = (width - 40.0f) * (norm - min) / (max - min);

    F32 track_width = 3.0f;

    // NOTE(simon): Track
    draw_rectangle(
        r2f32(min_pos.x + 20.0f, min_pos.y + height / 2.0f - track_width, min_pos.x + width - 20.0f, min_pos.y + height / 2.0f + track_width),
        color_from_theme(ThemeColor_DropShadow),
        track_width,
        0.0f,
        1.0f
    );

    // NOTE(simon): Volume indicator
    draw_rectangle(
        r2f32(min_pos.x + 21.0f, min_pos.y + height / 2.0f - track_width + 1.0f, min_pos.x + 20.0f + volume_pixels, min_pos.y + height / 2.0f + track_width - 1.0f),
        box->palette.cursor,
        track_width,
        0.0f,
        1.0f
    );


    // NOTE(simon): Knob
    draw_rectangle(
        r2f32(min_pos.x + volume_pixels, min_pos.y, min_pos.x + volume_pixels + 40.0f, min_pos.y + height),
        color_from_theme(ThemeColor_ButtonBorder),
        2.0f,
        0.0f,
        1.0f
    );

    // NOTE(simon): Gradients.
    Render_Shape *upper = draw_rectangle(
        r2f32(min_pos.x + volume_pixels, min_pos.y + 2.0f, min_pos.x + volume_pixels + 18.0f, min_pos.y + height - 2.0f),
        color_from_theme(ThemeColor_DropShadow),
        4.0f,
        0.0f,
        1.0f
    );
    upper->colors[Corner_00].a = 0.0f;
    upper->colors[Corner_10].a = 0.0f;
    upper->colors[Corner_01].a = 0.5f;
    upper->colors[Corner_11].a = 0.5f;
    upper->radies[Corner_00] = 0.0f;
    upper->radies[Corner_10] = 0.0f;

    Render_Shape *lower = draw_rectangle(
        r2f32(min_pos.x + volume_pixels + 22.0f, min_pos.y + 2.0f, min_pos.x + volume_pixels + 40.0f, min_pos.y + height - 2.0f),
        color_from_theme(ThemeColor_Hover),
        4.0f,
        0.0f,
        1.0f
    );
    lower->colors[Corner_00].a = 0.5f;
    lower->colors[Corner_10].a = 0.5f;
    lower->colors[Corner_01].a = 0.0f;
    lower->colors[Corner_11].a = 0.0f;
    lower->radies[Corner_01] = 0.0f;
    lower->radies[Corner_11] = 0.0f;

    // NOTE(simon): Middle line of knob.
    draw_rectangle(
        r2f32(min_pos.x + volume_pixels + 19.0f, min_pos.y + 1.0f, min_pos.x + volume_pixels + 21.0f, min_pos.y + height - 1.0f),
        box->palette.border,
        0.0f,
        0.0f,
        0.0f
    );
}

internal UI_Input volume_slider(F32 base, F32 *value, UI_Key key) {
    ui_height_push(ui_size_ems(1.5f, 1.0f));
    F32 min  = 0.0f;
    F32 norm = 1.0f;
    F32 max  = 2.0f;

    V2F32 *draw_data = arena_push_struct(ui_frame_arena(), V2F32);
    ui_draw_data_next(draw_data);
    ui_draw_function_next(draw_volume_slider);
    ui_hover_cursor_next(Gfx_Cursor_SizeWE);

    UI_Box *box = ui_create_box_from_key(
        UI_BoxFlag_Clickable,
        key
    );

    UI_Input input = ui_input_from_box(box);

    // NOTE(simon): Changing the value by dragging with the mouse.
    if (input.flags & UI_InputFlag_LeftDragging) {
        if (input.flags & UI_InputFlag_LeftPressed) {
            F32 drag_data = *value;
            ui_set_drag_data(&drag_data);
        }

        F32 value_pre_drag          = *ui_get_drag_data(F32);
        F32 percentage_pre_drag     = (value_pre_drag - min) / (max - min);
        F32 pixels_pre_drag         = percentage_pre_drag * r2f32_size(box->calculated_rectangle).width;
        F32 drag_delta              = ui_drag_delta().x;
        F32 pixels_post_drag        = pixels_pre_drag + drag_delta;
        F32 percentage_post_drag    = pixels_post_drag / r2f32_size(box->calculated_rectangle).width;
        F32 value_post_drag         = min + (percentage_post_drag) * (max - min);
        F32 clamped_value_post_drag = f32_min(f32_max(min, value_post_drag), max);
        *value = clamped_value_post_drag;
    }

    *draw_data = v2f32(base, *value);

    ui_height_pop();
    return input;
}

internal UI_BOX_DRAW_FUNCTION(draw_light) {
    B32 *light_up = (B32 *) data;
    V2F32 center = r2f32_center(box->calculated_rectangle);
    draw_circle(center, 5.0f, color_from_theme(ThemeColor_DropShadow), 0.0f, 1.0f);
    if (*light_up) {
        draw_circle(center, 4.0f, color_from_theme(ThemeColor_Focus), 0.0f, 1.0f);
    }
}

internal UI_Input light_toggle(B32 is_checked, Str8 label, UI_Key key) {
    ui_palette_push(palette_from_theme(ThemePalette_Button));

    ui_width_next(ui_size_ems(3.0f, 1.0f));
    ui_height_next(ui_size_ems(3.0f, 1.0f));
    ui_hover_cursor_next(Gfx_Cursor_Hand);
    ui_corner_radius_next(2.0f);
    ui_layout_axis_next(Axis2_Y);
    UI_Box *check = ui_create_box_from_key(
        UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder |
        UI_BoxFlag_DrawHot | UI_BoxFlag_DrawActive |
        UI_BoxFlag_Clickable | UI_BoxFlag_KeyboardClickable,
        key
    );

    ui_parent(check)
    ui_width(ui_size_fill())
    ui_height(ui_size_parent_percent(0.5f, 1.0f)) {
        B32 *light = arena_push_struct(ui_frame_arena(), B32);
        *light = is_checked;
        ui_draw_data_next(light);
        ui_draw_function_next(draw_light);
        ui_create_box(0);

        ui_text_align_next(UI_TextAlign_Center);
        ui_label(label);
    }

    UI_Input check_input = ui_input_from_box(check);
    ui_palette_pop();
    return check_input;
}

internal UI_Input light_toggle_b32(B32 *is_checked, Str8 label, UI_Key key) {
    UI_Input input = light_toggle(*is_checked, label, key);
    if (input.flags & UI_InputFlag_Clicked) {
        *is_checked = !*is_checked;
    }
    return input;
}

internal BUILD_TAB_FUNCTION(build_volume_tab) {
    Arena_Temporary scratch = arena_get_scratch(0, 0);

    // NOTE(simon): Collect pipewire objects.
    S64 object_count = { 0 };
    Pipewire_Object **objects = 0;
    {
        for (Pipewire_Object *object = pipewire_state->first_object; object; object = object->all_next) {
            ++object_count;
        }

        objects = arena_push_array_no_zero(scratch.arena, Pipewire_Object *, (U64) object_count);
        Pipewire_Object **object_ptr = objects;
        for (Pipewire_Object *object = pipewire_state->first_object; object; object = object->all_next) {
            *object_ptr = object;
            ++object_ptr;
        }
    }

    ui_width(ui_size_fill())
    ui_height(ui_size_fill())
    ui_column()
    ui_width(ui_size_text_content(0.0f, 1.0f))
    ui_height(ui_size_text_content(0.0f, 1.0f))
    for (S64 i = 0; i < object_count; ++i) {
        Pipewire_Object *object = objects[i];
        if (object->kind != Pipewire_Object_Node) {
            continue;
        }

        Pipewire_Parameter *props = pipewire_object_parameter_from_id(object, SPA_PARAM_Props);
        if (pipewire_parameter_is_nil(props)) {
            continue;
        }

        U32 id              = 0;
        B32 mute            = false;
        U32 channel_volumes_size  = 0;
        U32 channel_volumes_type  = 0;
        U32 channel_volumes_count = 0;
        F32 *channel_volumes = 0;
        F32 volume_base     = 0.0f;
        F32 volume_step     = 0.0f;
        U32 channel_map_size  = 0;
        U32 channel_map_type  = 0;
        U32 channel_map_count = 0;
        U32 *channel_map = 0;

        struct spa_pod_parser parser = { 0 };
        spa_pod_parser_pod(&parser, props->param);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
        spa_pod_parser_get_object(
            &parser,
            SPA_TYPE_OBJECT_Props,   &id,
            SPA_PROP_mute,           SPA_POD_OPT_Bool(&mute),
            SPA_PROP_channelVolumes, SPA_POD_OPT_Array(&channel_volumes_size, &channel_volumes_type, &channel_volumes_count, &channel_volumes),
            SPA_PROP_volumeBase,     SPA_POD_OPT_Float(&volume_base),
            SPA_PROP_volumeStep,     SPA_POD_OPT_Float(&volume_step),
            SPA_PROP_channelMap,     SPA_POD_OPT_Array(&channel_map_size, &channel_map_type, &channel_map_count, &channel_map)
            //SPA_PROP_monitorMute,    SPA_POD_OPT_Bool(&monitor_mute),
            //SPA_PROP_monitorVolumes, ,
            //SPA_PROP_softMute,       SPA_POD_OPT_Bool(&soft_mute),
            //SPA_PROP_softVolumes, ,
            //SPA_PROP_volumeRampSamples, ,
            //SPA_PROP_volumeRampStepSamples, ,
            //SPA_PROP_volumeRampTime, ,
            //SPA_PROP_volumeRampStepTime, ,
            //SPA_PROP_volumeRampScale, ,
        );
#pragma clang diagnostic pop

        Str8 name = name_from_object(object);
        UI_Input slider_input = { 0 };
        UI_Input mute_input   = { 0 };

        // NOTE(simon): Determine collective volume for all channels by
        // computing them max.
        F32 volume = 0.0f;
        for (U32 j = 0; j < channel_volumes_count; ++j) {
            volume = f32_max(volume, channel_volumes[j]);
        }

        // NOTE(simon): Use the same "linearization" as PulseAudio.
        F32 linear = f32_cbrt(volume);

        ui_label_format("%.*s, %.0f%%", str8_expand(name), 100.0f * linear);
        ui_width(ui_size_children_sum(1.0f))
        ui_height(ui_size_children_sum(1.0f))
        ui_row() {
            ui_spacer_sized(ui_size_ems(0.5f, 1.0f));

            mute_input = light_toggle_b32(&mute, str8_literal("Mute"), ui_key_from_string_format(ui_active_seed_key(), "%p_mute", object));

            ui_spacer_sized(ui_size_ems(0.5f, 1.0f));

            UI_Key channel_key = ui_key_from_string_format(ui_active_seed_key(), "%p_volume", object);
            ui_width_next(ui_size_ems(20.0f, 1.0f));
            slider_input = volume_slider(volume_base, &linear, channel_key);

            // NOTE(simon): "Delinearize" as PulseAudio after modification.
            volume = linear * linear * linear;

            F32 db = 20.0f * f32_ln(volume) / f32_ln(10.0f);
            ui_width_next(ui_size_text_content(0.0f, 1.0f));
            ui_label_format("%.2f dB", db);
        }

        // NOTE(simon): Distribute the new volume across the channels.
        for (U32 j = 0; j < channel_volumes_count; ++j) {
            channel_volumes[j] = volume;
        }

        if (mute_input.flags & UI_InputFlag_LeftClicked || slider_input.flags & UI_InputFlag_LeftDragging) {
            U8 buffer[4096];
            struct spa_pod_builder builder = { 0 };
            spa_pod_builder_init(&builder, buffer, sizeof(buffer));
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
            struct spa_pod *pod = spa_pod_builder_add_object(
                &builder,
                SPA_TYPE_OBJECT_Props, SPA_PARAM_Props,
                SPA_PROP_mute, SPA_POD_Bool(mute),
                SPA_PROP_channelVolumes, SPA_POD_Array(sizeof(F32), SPA_TYPE_Float, channel_volumes_count, channel_volumes)
            );
#pragma clang diagnostic pop
            pw_node_set_param((struct pw_node *) object->proxy, SPA_PARAM_Props, 0, pod);
        }
    }

    arena_end_temporary(scratch);
}

internal Void update(Void) {
    local U32 depth = 0;

    Arena_Temporary scratch = arena_get_scratch(0, 0);
    arena_reset(frame_arena());

    // NOTE(simon): Setup base context.
    memory_zero_struct(&state->base_context);
    state->context_stack = &state->base_context;

    state->selected_object = state->selected_object_next;
    state->hovered_object  = state->hovered_object_next;
    memory_zero_struct(&state->hovered_object_next);

    Gfx_EventList graphics_events = { 0 };
    if (depth == 0) {
        ++depth;
        graphics_events = gfx_get_events(scratch.arena, state->frames_to_render == 0);
        --depth;
    }



    // NOTE(simon): Process key bindings.
    typedef struct Binding Binding;
    struct Binding {
        Gfx_Key         key;
        Gfx_KeyModifier modifiers;
        CommandKind     command_kind;
    };

    Binding bindings[] = {
        { Gfx_Key_Tab,       Gfx_KeyModifier_Control,                         CommandKind_NextTab,              },
        { Gfx_Key_Tab,       Gfx_KeyModifier_Shift | Gfx_KeyModifier_Control, CommandKind_PreviousTab,          },
        { Gfx_Key_H,         Gfx_KeyModifier_Control,                         CommandKind_FocusPanelLeft,       },
        { Gfx_Key_J,         Gfx_KeyModifier_Control,                         CommandKind_FocusPanelDown,       },
        { Gfx_Key_K,         Gfx_KeyModifier_Control,                         CommandKind_FocusPanelUp,         },
        { Gfx_Key_L,         Gfx_KeyModifier_Control,                         CommandKind_FocusPanelRight,      },
        { Gfx_Key_Left,      Gfx_KeyModifier_Shift | Gfx_KeyModifier_Control, CommandKind_SelectWordLeft,       },
        { Gfx_Key_Up,        Gfx_KeyModifier_Shift | Gfx_KeyModifier_Control, CommandKind_SelectWordUp,         },
        { Gfx_Key_Right,     Gfx_KeyModifier_Shift | Gfx_KeyModifier_Control, CommandKind_SelectWordRight,      },
        { Gfx_Key_Down,      Gfx_KeyModifier_Shift | Gfx_KeyModifier_Control, CommandKind_SelectWordDown,       },
        { Gfx_Key_Left,      Gfx_KeyModifier_Shift,                           CommandKind_SelectCharacterLeft,  },
        { Gfx_Key_Up,        Gfx_KeyModifier_Shift,                           CommandKind_SelectCharacterUp,    },
        { Gfx_Key_Right,     Gfx_KeyModifier_Shift,                           CommandKind_SelectCharacterRight, },
        { Gfx_Key_Down,      Gfx_KeyModifier_Shift,                           CommandKind_SelectCharacterDown,  },
        { Gfx_Key_Left,      Gfx_KeyModifier_Control,                         CommandKind_MoveWordLeft,         },
        { Gfx_Key_Up,        Gfx_KeyModifier_Control,                         CommandKind_MoveWordUp,           },
        { Gfx_Key_Right,     Gfx_KeyModifier_Control,                         CommandKind_MoveWordRight,        },
        { Gfx_Key_Down,      Gfx_KeyModifier_Control,                         CommandKind_MoveWordDown,         },
        { Gfx_Key_Left,      0,                                               CommandKind_MoveCharacterLeft,    },
        { Gfx_Key_Up,        0,                                               CommandKind_MoveCharacterUp,      },
        { Gfx_Key_Right,     0,                                               CommandKind_MoveCharacterRight,   },
        { Gfx_Key_Down,      0,                                               CommandKind_MoveCharacterDown,    },
        { Gfx_Key_Home,      Gfx_KeyModifier_Shift,                           CommandKind_SelectHome,           },
        { Gfx_Key_End,       Gfx_KeyModifier_Shift,                           CommandKind_SelectEnd,            },
        { Gfx_Key_Home,      0,                                               CommandKind_MoveHome,             },
        { Gfx_Key_End,       0,                                               CommandKind_MoveEnd,              },
        { Gfx_Key_PageUp,    Gfx_KeyModifier_Shift,                           CommandKind_SelectPageUp,         },
        { Gfx_Key_PageDown,  Gfx_KeyModifier_Shift,                           CommandKind_SelectPageDown,       },
        { Gfx_Key_PageUp,    0,                                               CommandKind_MovePageUp,           },
        { Gfx_Key_PageDown,  0,                                               CommandKind_MovePageDown,         },
        { Gfx_Key_Home,      Gfx_KeyModifier_Shift | Gfx_KeyModifier_Control, CommandKind_SelectWholeUp,        },
        { Gfx_Key_End,       Gfx_KeyModifier_Shift | Gfx_KeyModifier_Control, CommandKind_SelectWholeDown,      },
        { Gfx_Key_Home,      Gfx_KeyModifier_Control,                         CommandKind_MoveWholeUp,          },
        { Gfx_Key_End,       Gfx_KeyModifier_Control,                         CommandKind_MoveWholeDown,        },
        { Gfx_Key_Backspace, Gfx_KeyModifier_Control,                         CommandKind_RemoveWord,           },
        { Gfx_Key_Delete,    Gfx_KeyModifier_Control,                         CommandKind_DeleteWord,           },
        { Gfx_Key_Backspace, 0,                                               CommandKind_RemoveCharacter,      },
        { Gfx_Key_Delete,    0,                                               CommandKind_DeleteCharacter,      },
        { Gfx_Key_A,         Gfx_KeyModifier_Control,                         CommandKind_SelectAll,            },
        { Gfx_Key_C,         Gfx_KeyModifier_Control,                         CommandKind_Copy,                 },
        { Gfx_Key_V,         Gfx_KeyModifier_Control,                         CommandKind_Paste,                },
        { Gfx_Key_X,         Gfx_KeyModifier_Control,                         CommandKind_Cut,                  },
        { Gfx_Key_Return,    0,                                               CommandKind_Accept,               },
        { Gfx_Key_Escape,    0,                                               CommandKind_Cancel,               },
    };

    for (Gfx_Event *event = graphics_events.first, *next = 0; event; event = next) {
        next = event->next;

        if (event->kind != Gfx_EventKind_KeyPress) {
            continue;
        }

        for (U64 i = 0; i < array_count(bindings); ++i) {
            Binding binding = bindings[i];
            if (event->key == binding.key && event->key_modifiers == binding.modifiers) {
                Window *window = window_from_gfx_handle(event->window);
                Panel  *panel  = panel_from_handle(window->active_panel);
                Tab    *tab    = tab_from_handle(panel->active_tab);
                push_command(
                    binding.command_kind,
                    .window = handle_from_window(window),
                    .panel  = handle_from_panel(panel),
                    .tab    = handle_from_tab(tab),
                );
                dll_remove(graphics_events.first, graphics_events.last, event);
                break;
            }
        }
    }



    // NOTE(simon): Consume events.
    for (Gfx_Event *event = graphics_events.first, *next = 0; event; event = next) {
        next = event->next;
        request_frame();

        Window *window = window_from_gfx_handle(event->window);
        Handle  handle = handle_from_window(window);

        B32 consume = false;

        if (event->kind == Gfx_EventKind_Quit) {
            consume = true;
            destroy_window(window);
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

            if (ui_event->kind != UI_EventKind_Null && !is_nil_window(window)) {
                ui_event_list_push_event(&window->ui_events, ui_event);
            }
        }

        if (drag_is_active() && event->kind == Gfx_EventKind_KeyRelease && event->key == Gfx_Key_MouseLeft) {
            state->drag_state = DragState_Dropping;
        }

        if (consume) {
            dll_remove(graphics_events.first, graphics_events.last, event);
        }
    }



    // NOTE(simon): Execute commands.
    if (depth == 0) {
        if (state->first_command) {
            request_frame();
        }

        for (Command *command = state->first_command; command; command = command->next) {
            push_context_internal(command->context);
            UI_Event *ui_event = 0;
            switch (command->kind) {
                case CommandKind_Null: {
                } break;
                case CommandKind_CloseTab: {
                    Panel *panel = panel_from_handle(top_context()->panel);
                    Tab   *tab   = tab_from_handle(top_context()->tab);

                    if (!is_nil_panel(panel) && !is_nil_tab(tab)) {
                        // NOTE(simon): Compute next active tab from this one.
                        Tab *next_active_tab = &nil_tab;
                        if (!is_nil_tab(tab->next)) {
                            next_active_tab = tab->next;
                        } else {
                            next_active_tab = tab->previous;
                        }

                        // NOTE(simon): Update the active tab if we are removing it.
                        if (tab_from_handle(panel->active_tab) == tab) {
                            panel->active_tab = handle_from_tab(next_active_tab);
                        }

                        remove_tab(panel, tab);
                        destroy_tab(tab);
                    }
                } break;
                case CommandKind_MoveTab: {
                    Window *window             = window_from_handle(top_context()->window);
                    Panel  *panel              = panel_from_handle(top_context()->panel);
                    Tab    *tab                = tab_from_handle(top_context()->tab);
                    Window *destination_window = window_from_handle(top_context()->destination_window);
                    Panel  *destination_panel  = panel_from_handle(top_context()->destination_panel);
                    Tab    *previous_tab       = tab_from_handle(top_context()->previous_tab);

                    if (!is_nil_panel(panel) && !is_nil_window(destination_window) && !is_nil_panel(destination_panel) && !is_nil_tab(tab) && tab != previous_tab) {
                        remove_tab(panel, tab);
                        insert_tab(destination_panel, previous_tab, tab);
                        destination_window->active_panel = handle_from_panel(destination_panel);

                        if (panel->child_count == 0 && panel != window->root_panel) {
                            push_command(CommandKind_ClosePanel, .panel = handle_from_panel(panel));
                        }
                    }
                } break;
                case CommandKind_NextTab: {
                    Panel *panel = panel_from_handle(top_context()->panel);
                    Tab   *tab   = tab_from_handle(top_context()->tab);
                    if (!is_nil_panel(panel) && !is_nil_tab(tab)) {
                        Tab *next_active_tab = panel->first_tab;
                        if (!is_nil_tab(tab->next)) {
                            next_active_tab = tab->next;
                        }
                        panel->active_tab = handle_from_tab(next_active_tab);
                    }
                } break;
                case CommandKind_PreviousTab: {
                    Panel *panel = panel_from_handle(top_context()->panel);
                    Tab   *tab   = tab_from_handle(top_context()->tab);
                    if (!is_nil_panel(panel) && !is_nil_tab(tab)) {
                        Tab *next_active_tab = panel->last_tab;
                        if (!is_nil_tab(tab->previous)) {
                            next_active_tab = tab->previous;
                        }
                        panel->active_tab = handle_from_tab(next_active_tab);
                    }
                } break;
                case CommandKind_FocusPanelLeft:
                case CommandKind_FocusPanelUp:
                case CommandKind_FocusPanelRight:
                case CommandKind_FocusPanelDown: {
                    // NOTE(simon): Extract direction and side from direction.
                    Axis2 movement_axis = Axis2_Invalid;
                    Side  movement_side = Side_Invalid;
                    if (command->kind == CommandKind_FocusPanelLeft || command->kind == CommandKind_FocusPanelRight) {
                        movement_axis = Axis2_X;
                    }
                    if (command->kind == CommandKind_FocusPanelUp || command->kind == CommandKind_FocusPanelDown) {
                        movement_axis = Axis2_Y;
                    }
                    if (command->kind == CommandKind_FocusPanelLeft || command->kind == CommandKind_FocusPanelUp) {
                        movement_side = Side_Min;
                    }
                    if (command->kind == CommandKind_FocusPanelRight || command->kind == CommandKind_FocusPanelDown) {
                        movement_side = Side_Max;
                    }

                    // TODO(simon): This should not be based on animation state
                    // as that is less predictable and not consistent unless
                    // you wait out the animations.

                    Window *window       = window_from_handle(top_context()->window);
                    Panel  *panel        = panel_from_handle(top_context()->panel);
                    V2F32   panel_center = r2f32_center(panel->animated_rectangle_percentage);
                    Panel  *sibling      = panel;

                    // NOTE(simon): Find the closest sibling along our movement axis.
                    if (!is_nil_panel(panel)) {
                        if (movement_side == Side_Min) {
                            while (!is_nil_panel(sibling->parent) && !(sibling->parent->split_axis == movement_axis && !is_nil_panel(sibling->previous))) {
                                sibling = sibling->parent;
                            }

                            sibling = sibling->previous;
                        } else if (movement_side == Side_Max) {
                            while (!is_nil_panel(sibling->parent) && !(sibling->parent->split_axis == movement_axis && !is_nil_panel(sibling->next))) {
                                sibling = sibling->parent;
                            }

                            sibling = sibling->next;
                        }
                    }

                    // NOTE(simon): Find the closest child in the selected sibling.
                    Panel *best_child = sibling;
                    F32 best_distance = f32_infinity();
                    for (Panel *child = sibling; !is_nil_panel(child); child = panel_iterator_depth_first_pre_order(child, sibling).next) {
                        if (!is_nil_panel(child->first)) {
                            continue;
                        }

                        V2F32 child_center = r2f32_center(child->animated_rectangle_percentage);
                        F32   distance     = f32_abs(panel_center.x - child_center.x) + f32_abs(panel_center.y - child_center.y);

                        if (distance < best_distance) {
                            best_distance = distance;
                            best_child    = child;
                        }
                    }

                    if (!is_nil_panel(best_child) && !is_nil_window(window)) {
                        window->active_panel = handle_from_panel(best_child);
                    }
                } break;
                case CommandKind_FocusPanel: {
                    Window *window = window_from_handle(top_context()->window);
                    if (!is_nil_window(window)) {
                        window->active_panel = top_context()->panel;
                    }
                } break;
                case CommandKind_ClosePanel: {
                    Window *window  = window_from_handle(top_context()->window);
                    Panel  *panel   = panel_from_handle(top_context()->panel);
                    Panel  *parent  = panel->parent;

                    if (!is_nil_panel(panel) && !is_nil_panel(parent)) {
                        // NOTE(simon): Update active panel, recursing into children if needed.
                        Panel *active_panel = panel_from_handle(window->active_panel);
                        if (active_panel == panel) {
                            if (!is_nil_panel(panel->next)) {
                                active_panel = panel->next;
                                while (!is_nil_panel(active_panel->first)) {
                                    active_panel = active_panel->first;
                                }
                            } else if (!is_nil_panel(panel->previous)) {
                                active_panel = panel->previous;
                                while (!is_nil_panel(active_panel->last)) {
                                    active_panel = active_panel->last;
                                }
                            } else {
                                active_panel = &nil_panel;
                            }
                            window->active_panel = handle_from_panel(active_panel);
                        }

                        if (parent->child_count == 2) {
                            // NOTE(simon): Merge the panel that we keep with our grandparents.
                            Panel *keep_child    = parent->first == panel ? parent->last : parent->first;
                            Panel *grandparent   = parent->parent;
                            Panel *previous      = parent->previous;

                            remove_panel(parent, keep_child);

                            // NOTE(simon): Insert the panel we are keeping into the tree.
                            keep_child->percentage_of_parent = parent->percentage_of_parent;
                            if (!is_nil_panel(grandparent)) {
                                remove_panel(grandparent, parent);
                                insert_panel(grandparent, previous, keep_child);
                            } else {
                                window->root_panel = keep_child;
                            }

                            destroy_panel(parent);

                            // NOTE(simon): If the split axis of keep_child and grandparent are the same, merge their children.
                            if (!is_nil_panel(grandparent) && !is_nil_panel(keep_child->first) && grandparent->split_axis == keep_child->split_axis) {
                                Panel *child_previous = keep_child->previous;
                                remove_panel(grandparent, keep_child);

                                for (Panel *child = keep_child->first, *next; !is_nil_panel(child); child = next) {
                                    next = child->next;

                                    remove_panel(keep_child, child);
                                    insert_panel(grandparent, child_previous, child);
                                    child_previous = child;
                                    child->percentage_of_parent *= keep_child->percentage_of_parent;
                                }

                                destroy_panel(keep_child);
                            }
                        } else {
                            // NOTE(simon): Remove panel and adjust children to fill the empty space.
                            remove_panel(parent, panel);

                            for (Panel *child = parent->first; !is_nil_panel(child); child = child->next) {
                                child->percentage_of_parent /= 1.0f - panel->percentage_of_parent;
                            }

                            destroy_panel(panel);
                        }
                    }
                } break;
                case CommandKind_SplitPanel: {
                    Side    split_side   = side_from_direction2(top_context()->direction);
                    Axis2   split_axis   = axis2_from_direction2(top_context()->direction);
                    Panel  *split_panel  = panel_from_handle(top_context()->destination_panel);
                    Window *split_window = window_from_handle(top_context()->window);
                    Panel  *move_panel   = panel_from_handle(top_context()->panel);
                    Tab    *move_tab     = tab_from_handle(top_context()->tab);

                    Panel *new_panel = &nil_panel;
                    if (!is_nil_panel(split_panel) && !is_nil_window(split_window) && top_context()->direction != Direction2_Invalid) {
                        Panel *parent = split_panel->parent;

                        if (!is_nil_panel(parent) && split_axis == parent->split_axis) {
                            Panel *next = create_panel();
                            insert_panel(parent, split_side == Side_Max ? split_panel : split_panel->previous, next);
                            next->percentage_of_parent = 1.0f / (F32) parent->child_count;
                            for (Panel *child = parent->first; !is_nil_panel(child); child = child->next) {
                                if (child != next) {
                                    child->percentage_of_parent *= (F32) (parent->child_count - 1) / (F32) parent->child_count;
                                }
                            }
                            new_panel = next;
                        } else {
                            Panel *previous_previous = split_panel->previous;
                            Panel *previous_parent   = parent;
                            Panel *new_parent        = create_panel();

                            new_parent->percentage_of_parent = split_panel->percentage_of_parent;
                            if (!is_nil_panel(previous_parent)) {
                                remove_panel(previous_parent, split_panel);
                                insert_panel(previous_parent, previous_previous, new_parent);
                            } else {
                                split_window->root_panel = new_parent;
                            }

                            new_panel = create_panel();
                            Panel *left  = split_panel;
                            Panel *right = new_panel;
                            if (split_side == Side_Min) {
                                swap(left, right, Panel *);
                            }

                            insert_panel(new_parent, &nil_panel, left);
                            insert_panel(new_parent, left, right);
                            new_parent->split_axis = split_axis;
                            left->percentage_of_parent  = 0.5f;
                            right->percentage_of_parent = 0.5f;
                        }

                        if (!is_nil_panel(new_panel->previous)) {
                            new_panel->animated_rectangle_percentage = new_panel->previous->animated_rectangle_percentage;
                            new_panel->animated_rectangle_percentage.min.values[split_axis] = new_panel->animated_rectangle_percentage.max.values[split_axis];
                        }
                        if (!is_nil_panel(new_panel->next)) {
                            new_panel->animated_rectangle_percentage = new_panel->next->animated_rectangle_percentage;
                            new_panel->animated_rectangle_percentage.max.values[split_axis] = new_panel->animated_rectangle_percentage.min.values[split_axis];
                        }
                    }

                    if (!is_nil_panel(new_panel) && !is_nil_panel(move_panel) && !is_nil_tab(move_tab)) {
                        split_window->active_panel = handle_from_panel(new_panel);
                        remove_tab(move_panel, move_tab);
                        insert_tab(new_panel, new_panel->last_tab, move_tab);

                        if (move_panel->child_count == 0 && move_panel != split_window->root_panel && move_panel != new_panel->next && move_panel != new_panel->previous) {
                            push_command(CommandKind_ClosePanel, .panel = handle_from_panel(move_panel));
                        }
                    }
                } break;
                case CommandKind_SelectWordLeft: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = -1;
                    ui_event->unit = UI_EventDeltaUnit_Word;
                    ui_event->flags = UI_EventFlag_KeepMark;
                } break;
                case CommandKind_SelectWordUp: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.y = -1;
                    ui_event->unit = UI_EventDeltaUnit_Word;
                    ui_event->flags = UI_EventFlag_KeepMark;
                } break;
                case CommandKind_SelectWordRight: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = 1;
                    ui_event->unit = UI_EventDeltaUnit_Word;
                    ui_event->flags = UI_EventFlag_KeepMark;
                } break;
                case CommandKind_SelectWordDown: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.y = 1;
                    ui_event->unit = UI_EventDeltaUnit_Word;
                    ui_event->flags = UI_EventFlag_KeepMark;
                } break;
                case CommandKind_SelectCharacterLeft: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = -1;
                    ui_event->unit = UI_EventDeltaUnit_Character;
                    ui_event->flags = UI_EventFlag_KeepMark;
                } break;
                case CommandKind_SelectCharacterUp: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.y = -1;
                    ui_event->unit = UI_EventDeltaUnit_Character;
                    ui_event->flags = UI_EventFlag_KeepMark;
                } break;
                case CommandKind_SelectCharacterRight: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = 1;
                    ui_event->unit = UI_EventDeltaUnit_Character;
                    ui_event->flags = UI_EventFlag_KeepMark;
                } break;
                case CommandKind_SelectCharacterDown: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.y = 1;
                    ui_event->unit = UI_EventDeltaUnit_Character;
                    ui_event->flags = UI_EventFlag_KeepMark;
                } break;
                case CommandKind_MoveWordLeft: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = -1;
                    ui_event->unit = UI_EventDeltaUnit_Word;
                    ui_event->flags = UI_EventFlag_PickSelectSide | UI_EventFlag_ZeroDeltaOnSelection;
                } break;
                case CommandKind_MoveWordUp: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.y = -1;
                    ui_event->unit = UI_EventDeltaUnit_Word;
                    ui_event->flags = UI_EventFlag_PickSelectSide | UI_EventFlag_ZeroDeltaOnSelection;
                } break;
                case CommandKind_MoveWordRight: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = 1;
                    ui_event->unit = UI_EventDeltaUnit_Word;
                    ui_event->flags = UI_EventFlag_PickSelectSide | UI_EventFlag_ZeroDeltaOnSelection;
                } break;
                case CommandKind_MoveWordDown: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.y = 1;
                    ui_event->unit = UI_EventDeltaUnit_Word;
                    ui_event->flags = UI_EventFlag_PickSelectSide | UI_EventFlag_ZeroDeltaOnSelection;
                } break;
                case CommandKind_MoveCharacterLeft: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = -1;
                    ui_event->unit = UI_EventDeltaUnit_Character;
                    ui_event->flags = UI_EventFlag_PickSelectSide | UI_EventFlag_ZeroDeltaOnSelection;
                } break;
                case CommandKind_MoveCharacterUp: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.y = -1;
                    ui_event->unit = UI_EventDeltaUnit_Character;
                    ui_event->flags = UI_EventFlag_PickSelectSide | UI_EventFlag_ZeroDeltaOnSelection;
                } break;
                case CommandKind_MoveCharacterRight: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = 1;
                    ui_event->unit = UI_EventDeltaUnit_Character;
                    ui_event->flags = UI_EventFlag_PickSelectSide | UI_EventFlag_ZeroDeltaOnSelection;
                } break;
                case CommandKind_MoveCharacterDown: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.y = 1;
                    ui_event->unit = UI_EventDeltaUnit_Character;
                    ui_event->flags = UI_EventFlag_PickSelectSide | UI_EventFlag_ZeroDeltaOnSelection;
                } break;
                case CommandKind_SelectHome: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = -1;
                    ui_event->unit = UI_EventDeltaUnit_Line;
                    ui_event->flags = UI_EventFlag_KeepMark;
                } break;
                case CommandKind_SelectEnd: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = 1;
                    ui_event->unit = UI_EventDeltaUnit_Line;
                    ui_event->flags = UI_EventFlag_KeepMark;
                } break;
                case CommandKind_MoveHome: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = -1;
                    ui_event->unit = UI_EventDeltaUnit_Line;
                    ui_event->flags = UI_EventFlag_PickSelectSide | UI_EventFlag_ZeroDeltaOnSelection;
                } break;
                case CommandKind_MoveEnd: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = 1;
                    ui_event->unit = UI_EventDeltaUnit_Line;
                    ui_event->flags = UI_EventFlag_PickSelectSide | UI_EventFlag_ZeroDeltaOnSelection;
                } break;
                case CommandKind_SelectPageUp: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.y = -1;
                    ui_event->unit = UI_EventDeltaUnit_Page;
                    ui_event->flags = UI_EventFlag_KeepMark;
                } break;
                case CommandKind_SelectPageDown: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.y = 1;
                    ui_event->unit = UI_EventDeltaUnit_Page;
                    ui_event->flags = UI_EventFlag_KeepMark;
                } break;
                case CommandKind_MovePageUp: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.y = -1;
                    ui_event->unit = UI_EventDeltaUnit_Page;
                    ui_event->flags = UI_EventFlag_PickSelectSide | UI_EventFlag_ZeroDeltaOnSelection;
                } break;
                case CommandKind_MovePageDown: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.y = 1;
                    ui_event->unit = UI_EventDeltaUnit_Page;
                    ui_event->flags = UI_EventFlag_PickSelectSide | UI_EventFlag_ZeroDeltaOnSelection;
                } break;
                case CommandKind_SelectWholeUp: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = -1;
                    ui_event->unit = UI_EventDeltaUnit_Whole;
                    ui_event->flags = UI_EventFlag_KeepMark;
                } break;
                case CommandKind_SelectWholeDown: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = 1;
                    ui_event->unit = UI_EventDeltaUnit_Whole;
                    ui_event->flags = UI_EventFlag_KeepMark;
                } break;
                case CommandKind_MoveWholeUp: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = -1;
                    ui_event->unit = UI_EventDeltaUnit_Whole;
                    ui_event->flags = UI_EventFlag_PickSelectSide | UI_EventFlag_ZeroDeltaOnSelection;
                } break;
                case CommandKind_MoveWholeDown: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->delta.x = 1;
                    ui_event->unit = UI_EventDeltaUnit_Whole;
                    ui_event->flags = UI_EventFlag_PickSelectSide | UI_EventFlag_ZeroDeltaOnSelection;
                } break;
                case CommandKind_RemoveWord: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Edit;
                    ui_event->delta.x = -1;
                    ui_event->unit = UI_EventDeltaUnit_Word;
                    ui_event->flags = UI_EventFlag_ZeroDeltaOnSelection | UI_EventFlag_Delete;
                } break;
                case CommandKind_DeleteWord: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Edit;
                    ui_event->delta.x = 1;
                    ui_event->unit = UI_EventDeltaUnit_Word;
                    ui_event->flags = UI_EventFlag_ZeroDeltaOnSelection | UI_EventFlag_Delete;
                } break;
                case CommandKind_RemoveCharacter: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Edit;
                    ui_event->delta.x = -1;
                    ui_event->unit = UI_EventDeltaUnit_Character;
                    ui_event->flags = UI_EventFlag_ZeroDeltaOnSelection | UI_EventFlag_Delete;
                } break;
                case CommandKind_DeleteCharacter: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Edit;
                    ui_event->delta.x = 1;
                    ui_event->unit = UI_EventDeltaUnit_Character;
                    ui_event->flags = UI_EventFlag_ZeroDeltaOnSelection | UI_EventFlag_Delete;
                } break;
                case CommandKind_SelectAll: {
                    UI_Event *move_start = arena_push_struct(ui_frame_arena(), UI_Event);
                    move_start->kind = UI_EventKind_Navigation;
                    move_start->unit = UI_EventDeltaUnit_Whole;
                    move_start->delta.x = -1;
                    Window *window = window_from_handle(top_context()->window);
                    if (!is_nil_window(window)) {
                        ui_event_list_push_event(&window->ui_events, move_start);
                    }

                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Navigation;
                    ui_event->flags = UI_EventFlag_KeepMark;
                    ui_event->unit = UI_EventDeltaUnit_Whole;
                    ui_event->delta.x = 1;
                } break;
                case CommandKind_Copy: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Edit;
                    ui_event->flags = UI_EventFlag_Copy | UI_EventFlag_KeepMark;
                } break;
                case CommandKind_Paste: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Text;
                    ui_event->text = gfx_get_clipboard_text(ui_frame_arena());
                } break;
                case CommandKind_Cut: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Edit;
                    ui_event->flags = UI_EventFlag_Copy | UI_EventFlag_Delete;
                } break;
                case CommandKind_Accept: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Accept;
                } break;
                case CommandKind_Cancel: {
                    ui_event = arena_push_struct(ui_frame_arena(), UI_Event);
                    ui_event->kind = UI_EventKind_Cancel;
                } break;
                case CommandKind_COUNT: {
                } break;
            }

            Window *window = window_from_handle(top_context()->window);
            if (!is_nil_window(window) && ui_event && ui_event->kind != UI_EventKind_Null) {
                ui_event_list_push_event(&window->ui_events, ui_event);
            }

            pop_context();
        }

        // NOTE(simon): Reset command state.
        arena_reset(state->command_arena);
        state->first_command = 0;
        state->last_command  = 0;
    }


    // NOTE(simon): Update window.
    draw_begin_frame();

    // NOTE(simon): Build palettes.
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

    for (Window *window = state->first_window; window; window = window->next) {
        push_context(.window = handle_from_window(window), .panel = window->active_panel, .tab = panel_from_handle(window->active_panel)->active_tab);

        V2U32 client_size      = gfx_client_area_from_window(window->window);
        R2F32 client_rectangle = r2f32(0.0f, 0.0f, (F32) client_size.x, (F32) client_size.y);

        ui_select_state(window->ui);
        ui_begin(window->window, &window->ui_events, &icon_info, 1.0f / 60.0f);
        ui_palette_push(palette_from_theme(ThemePalette_Base));
        ui_font_push(font_cache_font_from_static_data(&default_font));
        ui_font_size_push((U32) (state->font_size * gfx_dpi_from_window(window->window) / 72.0f));

        // NOTE(simon): Only build preview if we are actually dragging the
        // view. Otherwise, the tooltip will be clipped to the current window.
        if (state->drag_state == DragState_Dragging && state->drag_context_member == ContextMember_Tab) {
            push_context_internal(state->drag_context);
            Tab *tab = tab_from_handle(top_context()->tab);
            if (!is_nil_tab(tab)) {
                ui_tooltip(global_ui_null_key) {
                    ui_corner_radius_next(ui_size_ems(0.2f, 1.0f).value);
                    ui_width_next(ui_size_ems(60.0f, 1.0f));
                    ui_height_next(ui_size_ems(40.0f, 1.0f));
                    ui_layout_axis_next(Axis2_X);
                    UI_Box *preview_box = ui_create_box(UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder | UI_BoxFlag_DrawDropShadow);
                    ui_parent(preview_box)
                    ui_padding(ui_size_ems(0.5f, 1.0f))
                    ui_width(ui_size_fill())
                    ui_height(ui_size_fill())
                    ui_column()
                    ui_padding(ui_size_ems(0.5f, 1.0f)) {
                        ui_width_next(ui_size_text_content(0.0f, 1.0f));
                        ui_height_next(ui_size_text_content(0.0f, 1.0f));
                        ui_label(tab->name);

                        ui_spacer_sized(ui_size_ems(0.5f, 1.0f));

                        ui_width_next(ui_size_fill());
                        ui_height_next(ui_size_fill());
                        UI_Box *content_box = ui_create_box_from_string(UI_BoxFlag_DrawBorder | UI_BoxFlag_Clip, str8_literal("###drag_preview"));

                        ui_parent(content_box) {
                            tab->build(content_box->calculated_rectangle);
                        }
                    }
                }
            } else {
                drag_cancel();
            }
            pop_context();
        }

        R2F32 top_bar_rectangle = { 0 };
        F32 border_width = 0.0f;
        R2F32 content_rectangle = r2f32_pad(r2f32(client_rectangle.min.x, top_bar_rectangle.max.y, client_rectangle.max.x, client_rectangle.max.y), -border_width);
        V2F32 content_size      = r2f32_size(content_rectangle);

        F32 panel_pad = 2.0f;
        F32 drop_major_half_size = 3.5f * (F32) ui_font_size_top();
        F32 drop_minor_half_size = 2.5f * (F32) ui_font_size_top();
        F32 drop_padding         = 0.5f * (F32) ui_font_size_top();
        F32 drop_corner_radius   = 0.5f * (F32) ui_font_size_top();

        // NOTE(simon): Build non-leaf panel UI.
        prof_zone_begin(prof_bulid_non_leaf_ui, "non-leaf ui");
        // NOTE(simon): Build drop targets if we are dragging a tab.
        if (drag_is_active() && state->drag_context_member == ContextMember_Tab) {
            typedef struct DropTarget DropTarget;
            struct DropTarget {
                DropTarget *next;
                Direction2  split_direction;
                Panel      *split_panel;
            };

            DropTarget *first_drop_target = 0;
            DropTarget *last_drop_target  = 0;

            // NOTE(simon): Add extra drop targets for the root panel.
            DropTarget *min_target = arena_push_struct(ui_frame_arena(), DropTarget);
            min_target->split_direction = axis2_flip(window->root_panel->split_axis) == Axis2_X ? Direction2_Left : Direction2_Up;
            min_target->split_panel = window->root_panel;
            sll_queue_push(first_drop_target, last_drop_target, min_target);

            DropTarget *max_target = arena_push_struct(ui_frame_arena(), DropTarget);
            max_target->split_direction = axis2_flip(window->root_panel->split_axis) == Axis2_X ? Direction2_Right : Direction2_Down;
            max_target->split_panel = window->root_panel;
            sll_queue_push(first_drop_target, last_drop_target, max_target);

            // NOTE(simon): Sandwich all children between drop targets.
            for (Panel *panel = window->root_panel; !is_nil_panel(panel); panel = panel_iterator_depth_first_pre_order(panel, window->root_panel).next) {
                if (is_nil_panel(panel->first)) {
                    continue;
                }

                Axis2 split_axis = panel->split_axis;

                for (Panel *child = panel->first; !is_nil_panel(child); child = child->next) {
                    DropTarget *target      = arena_push_struct(ui_frame_arena(), DropTarget);
                    target->split_direction = split_axis == Axis2_X ? Direction2_Left : Direction2_Up;
                    target->split_panel     = child;
                    sll_queue_push(first_drop_target, last_drop_target, target);
                }

                // NOTE(simon): Extra drop target at the end.
                DropTarget *target      = arena_push_struct(ui_frame_arena(), DropTarget);
                target->split_direction = split_axis == Axis2_X ? Direction2_Right : Direction2_Down;
                target->split_panel     = panel->last;
                sll_queue_push(first_drop_target, last_drop_target, target);
            }

            // NOTE(simon): Build drop targets.
            ui_corner_radius(drop_corner_radius)
            for (DropTarget *drop_target = first_drop_target; drop_target; drop_target = drop_target->next) {
                // NOTE(simon): Unpack drop target parameters.
                Axis2 split_axis      = axis2_from_direction2(drop_target->split_direction);
                Side  split_side      = side_from_direction2(drop_target->split_direction);
                R2F32 split_rectangle = rectangle_from_panel(drop_target->split_panel, content_rectangle);
                V2F32 split_center    = r2f32_center(split_rectangle);

                // NOTE(simon): Construct drop target location.
                R2F32 drop_rectangle = { 0 };
                drop_rectangle.min.values[split_axis] = split_rectangle.values[split_side].values[split_axis] - drop_minor_half_size;
                drop_rectangle.max.values[split_axis] = split_rectangle.values[split_side].values[split_axis] + drop_minor_half_size;
                drop_rectangle.min.values[axis2_flip(split_axis)] = split_center.values[axis2_flip(split_axis)] - drop_major_half_size;
                drop_rectangle.max.values[axis2_flip(split_axis)] = split_center.values[axis2_flip(split_axis)] + drop_major_half_size;

                UI_Key ui_key = ui_key_from_string_format(global_ui_null_key, "drop_boundary_%p_%i", drop_target->split_panel, drop_target->split_direction);

                ui_fixed_position_next(drop_rectangle.min);
                ui_width_next(ui_size_pixels(r2f32_size(drop_rectangle).width, 1.0f));
                ui_height_next(ui_size_pixels(r2f32_size(drop_rectangle).height, 1.0f));
                ui_layout_axis_next(Axis2_Y);
                UI_Box *drop_site = ui_create_box_from_key(UI_BoxFlag_FloatingPosition | UI_BoxFlag_DropTarget, ui_key);

                // NOTE(simon): Build visualization.
                ui_parent(drop_site)
                ui_width(ui_size_fill())
                ui_height(ui_size_fill())
                ui_padding(ui_size_pixels(drop_padding, 1.0f))
                ui_row()
                ui_padding(ui_size_pixels(drop_padding, 1.0f)) {
                    ui_layout_axis_next(axis2_flip(split_axis));
                    UI_Box *visualization = ui_create_box(UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder | UI_BoxFlag_DrawDropShadow);
                    ui_parent(visualization)
                    ui_padding(ui_size_pixels(drop_padding, 1.0f))
                    ui_palette(palette_from_theme(ThemePalette_Button)) {
                        ui_layout_axis_next(split_axis);
                        UI_Box *row_or_column = ui_create_box(0);
                        ui_parent(row_or_column)
                        ui_padding(ui_size_pixels(drop_padding, 1.0f)) {
                            ui_create_box(UI_BoxFlag_DrawBorder);
                            ui_spacer_sized(ui_size_pixels(drop_padding, 1.0f));
                            ui_create_box(UI_BoxFlag_DrawBorder);
                        }
                    }
                }

                ui_input_from_box(drop_site);

                // NOTE(simon): Build preview if we hover the drop target.
                if (ui_keys_match(ui_key, ui_drop_hot_key())) {
                    R2F32 target_rectangle = drop_rectangle;
                    target_rectangle.min.values[split_axis] -= drop_major_half_size;
                    target_rectangle.max.values[split_axis] += drop_major_half_size;
                    target_rectangle.min.values[axis2_flip(split_axis)] = split_rectangle.min.values[axis2_flip(split_axis)] + drop_major_half_size;
                    target_rectangle.max.values[axis2_flip(split_axis)] = split_rectangle.max.values[axis2_flip(split_axis)] - drop_major_half_size;

                    R2F32 future_split_rectangle = r2f32(
                        ui_animate(ui_key_from_string(global_ui_null_key, str8_literal("tab_drop_min_x")), target_rectangle.min.x, .initial = drop_rectangle.min.x),
                        ui_animate(ui_key_from_string(global_ui_null_key, str8_literal("tab_drop_min_y")), target_rectangle.min.y, .initial = drop_rectangle.min.y),
                        ui_animate(ui_key_from_string(global_ui_null_key, str8_literal("tab_drop_max_x")), target_rectangle.max.x, .initial = drop_rectangle.max.x),
                        ui_animate(ui_key_from_string(global_ui_null_key, str8_literal("tab_drop_max_y")), target_rectangle.max.y, .initial = drop_rectangle.max.y)
                    );

                    ui_palette_next(palette_from_theme(ThemePalette_DropSiteOverlay));
                    ui_fixed_position_next(future_split_rectangle.min);
                    ui_width_next(ui_size_pixels(r2f32_size(future_split_rectangle).width, 1.0f));
                    ui_height_next(ui_size_pixels(r2f32_size(future_split_rectangle).height, 1.0f));
                    ui_create_box(UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder);
                }

                // NOTE(simon): Execute the drop.
                if (ui_keys_match(ui_key, ui_drop_hot_key()) && drag_drop()) {
                    push_command(
                        CommandKind_SplitPanel,
                        .panel             = state->drag_context->panel,
                        .tab               = state->drag_context->tab,
                        .destination_panel = handle_from_panel(drop_target->split_panel),
                        .direction         = drop_target->split_direction
                    );
                }
            }
        }

        // NOTE(simon): Build panel boundaries.
        for (Panel *panel = window->root_panel; !is_nil_panel(panel); panel = panel_iterator_depth_first_pre_order(panel, window->root_panel).next) {
            if (is_nil_panel(panel->first)) {
                continue;
            }

            R2F32 panel_rectangle      = rectangle_from_panel(panel, content_rectangle);
            V2F32 panel_rectangle_size = r2f32_size(panel_rectangle);

            for (Panel *child = panel->first; !is_nil_panel(child->next); child = child->next) {
                R2F32 child_rectangle = rectangle_from_panel(child, content_rectangle);
                R2F32 boundary_rectangle = child_rectangle;
                boundary_rectangle.min.values[panel->split_axis] = boundary_rectangle.max.values[panel->split_axis];
                boundary_rectangle.min.values[panel->split_axis] -= panel_pad;
                boundary_rectangle.max.values[panel->split_axis] += panel_pad;

                ui_fixed_position_next(boundary_rectangle.min);
                ui_width_next(ui_size_pixels(r2f32_size(boundary_rectangle).width, 1.0f));
                ui_height_next(ui_size_pixels(r2f32_size(boundary_rectangle).height, 1.0f));
                ui_hover_cursor_next(panel->split_axis == Axis2_X ? Gfx_Cursor_SizeWE : Gfx_Cursor_SizeNS);
                UI_Box *boundary_box = ui_create_box_from_string_format(UI_BoxFlag_Clickable, "###panel_boundary_%p", child);
                UI_Input input = ui_input_from_box(boundary_box);

                if (input.flags & UI_InputFlag_LeftDragging) {
                    Panel *min_child = child;
                    Panel *max_child = child->next;

                    if (input.flags & UI_InputFlag_LeftPressed) {
                        V2F32 drag_data = v2f32(min_child->percentage_of_parent, max_child->percentage_of_parent);
                        ui_set_drag_data(&drag_data);
                    }

                    V2F32 drag_data = *ui_get_drag_data(V2F32);

                    F32 min_child_percentage_pre_drag = drag_data.x;
                    F32 max_child_percentage_pre_drag = drag_data.y;
                    F32 min_child_pixels_pre_drag = min_child_percentage_pre_drag * panel_rectangle_size.values[panel->split_axis];
                    F32 max_child_pixels_pre_drag = max_child_percentage_pre_drag * panel_rectangle_size.values[panel->split_axis];

                    V2F32 both_drag_delta = ui_drag_delta();
                    F32 drag_delta = both_drag_delta.values[panel->split_axis];
                    F32 clamped_drag_delta = drag_delta;
                    if (drag_delta < 0.0f) {
                        clamped_drag_delta = -f32_min(-drag_delta, min_child_pixels_pre_drag - 2.0f * panel_pad);
                    } else {
                        clamped_drag_delta = f32_min(drag_delta, max_child_pixels_pre_drag - 2.0f * panel_pad);
                    }

                    F32 min_child_pixels_post_drag = min_child_pixels_pre_drag + clamped_drag_delta;
                    F32 max_child_pixels_post_drag = max_child_pixels_pre_drag - clamped_drag_delta;
                    F32 min_child_percentage_post_drag = min_child_pixels_post_drag / panel_rectangle_size.values[panel->split_axis];
                    F32 max_child_percentage_post_drag = max_child_pixels_post_drag / panel_rectangle_size.values[panel->split_axis];
                    min_child->percentage_of_parent = min_child_percentage_post_drag;
                    max_child->percentage_of_parent = max_child_percentage_post_drag;
                }
            }
        }
        prof_zone_end(prof_bulid_non_leaf_ui);

        // NOTE(simon): Animate panels.
        // TODO(simon): This shouldn't animate if we are resizing panels.
        for (Panel *panel = window->root_panel; !is_nil_panel(panel); panel = panel_iterator_depth_first_pre_order(panel, &nil_panel).next) {
            R2F32 target = rectangle_from_panel(panel, content_rectangle);
            R2F32 target_percentage = r2f32(
                target.min.x / content_size.width,
                target.min.y / content_size.height,
                target.max.x / content_size.width,
                target.max.y / content_size.height
            );

            B32 is_animating = false;
            is_animating |= f32_abs(target.min.x - panel->animated_rectangle_percentage.min.x * content_size.width)  > 0.5f;
            is_animating |= f32_abs(target.min.y - panel->animated_rectangle_percentage.min.y * content_size.height) > 0.5f;
            is_animating |= f32_abs(target.max.x - panel->animated_rectangle_percentage.max.x * content_size.width)  > 0.5f;
            is_animating |= f32_abs(target.max.y - panel->animated_rectangle_percentage.max.y * content_size.height) > 0.5f;

            if (is_animating) {
                F32 rate = ui_animation_fast_rate();
                panel->animated_rectangle_percentage.min.x += (target_percentage.min.x - panel->animated_rectangle_percentage.min.x) * rate;
                panel->animated_rectangle_percentage.min.y += (target_percentage.min.y - panel->animated_rectangle_percentage.min.y) * rate;
                panel->animated_rectangle_percentage.max.x += (target_percentage.max.x - panel->animated_rectangle_percentage.max.x) * rate;
                panel->animated_rectangle_percentage.max.y += (target_percentage.max.y - panel->animated_rectangle_percentage.max.y) * rate;
                request_frame();
            } else {
                panel->animated_rectangle_percentage = target_percentage;
            }
        }

        // NOTE(simon): Build leaf panel UI.
        prof_zone_begin(prof_build_leaf_ui, "leaf ui");
        for (Panel *panel = window->root_panel; !is_nil_panel(panel); panel = panel_iterator_depth_first_pre_order(panel, window->root_panel).next) {
            if (!is_nil_panel(panel->first)) {
                continue;
            }

            push_context(.panel = handle_from_panel(panel), .tab = panel->active_tab);
            ui_focus_push(panel == panel_from_handle(window->active_panel) ? UI_Focus_None : UI_Focus_Inactive);

            R2F32 panel_rectangle = r2f32_pad(
                r2f32(
                    panel->animated_rectangle_percentage.min.x * content_size.width,
                    panel->animated_rectangle_percentage.min.y * content_size.height,
                    panel->animated_rectangle_percentage.max.x * content_size.width,
                    panel->animated_rectangle_percentage.max.y * content_size.height
                ),
                -panel_pad
            );
            Handle next_active_tab = panel->active_tab;

            // NOTE(simon): Draw split boundaries if we are dragging a tab.
            if (drag_is_active() && state->drag_context_member == ContextMember_Tab) {
                V2F32 panel_center = r2f32_center(panel_rectangle);

                typedef struct DropTarget DropTarget;
                struct DropTarget {
                    UI_Key     ui_key;
                    Direction2 direction;
                    R2F32      rectangle;
                };
                DropTarget targets[] = {
                    {
                        ui_key_from_string_format(global_ui_null_key, "drop_right_%p", panel),
                        Direction2_Right,
                        r2f32(
                            panel_center.x + 2.0f * drop_major_half_size - drop_major_half_size, panel_center.y - drop_major_half_size,
                            panel_center.x + 2.0f * drop_major_half_size + drop_major_half_size, panel_center.y + drop_major_half_size
                        )
                    },
                    {
                        ui_key_from_string_format(global_ui_null_key, "drop_down_%p", panel),
                        Direction2_Down,
                        r2f32(
                            panel_center.x - drop_major_half_size, panel_center.y + 2.0f * drop_major_half_size - drop_major_half_size,
                            panel_center.x + drop_major_half_size, panel_center.y + 2.0f * drop_major_half_size + drop_major_half_size
                        )
                    },
                    {
                        ui_key_from_string_format(global_ui_null_key, "drop_center_%p", panel),
                        Direction2_Invalid,
                        r2f32(
                            panel_center.x - drop_major_half_size, panel_center.y - drop_major_half_size,
                            panel_center.x + drop_major_half_size, panel_center.y + drop_major_half_size
                        )
                    },
                    {
                        ui_key_from_string_format(global_ui_null_key, "drop_left_%p", panel),
                        Direction2_Left,
                        r2f32(
                            panel_center.x - 2.0f * drop_major_half_size - drop_major_half_size, panel_center.y - drop_major_half_size,
                            panel_center.x - 2.0f * drop_major_half_size + drop_major_half_size, panel_center.y + drop_major_half_size
                        )
                    },
                    {
                        ui_key_from_string_format(global_ui_null_key, "drop_up_%p", panel),
                        Direction2_Up,
                        r2f32(
                            panel_center.x - drop_major_half_size, panel_center.y - 2.0f * drop_major_half_size - drop_major_half_size,
                            panel_center.x + drop_major_half_size, panel_center.y - 2.0f * drop_major_half_size + drop_major_half_size
                        )
                    },
                };

                ui_corner_radius(drop_corner_radius)
                for (U64 i = 0; i < array_count(targets); ++i) {
                    Axis2 split_axis = axis2_from_direction2(targets[i].direction);
                    Side  split_side = side_from_direction2(targets[i].direction);

                    if (targets[i].direction != Direction2_Invalid && !is_nil_panel(panel->parent) && split_axis == panel->parent->split_axis) {
                        continue;
                    }

                    ui_fixed_position_next(targets[i].rectangle.min);
                    ui_width_next(ui_size_pixels(r2f32_size(targets[i].rectangle).width, 1.0f));
                    ui_height_next(ui_size_pixels(r2f32_size(targets[i].rectangle).height, 1.0f));
                    ui_layout_axis_next(Axis2_Y);
                    UI_Box *drop_site = ui_create_box_from_key(UI_BoxFlag_FloatingPosition | UI_BoxFlag_DropTarget, targets[i].ui_key);

                    ui_parent(drop_site)
                    ui_width(ui_size_fill())
                    ui_height(ui_size_fill())
                    ui_padding(ui_size_pixels(drop_padding, 1.0f))
                    ui_row()
                    ui_padding(ui_size_pixels(drop_padding, 1.0f)) {
                        ui_layout_axis_next(axis2_flip(split_axis));
                        UI_Box *visualization = ui_create_box(UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder | UI_BoxFlag_DrawDropShadow);
                        ui_parent(visualization)
                        ui_padding(ui_size_pixels(drop_padding, 1.0f)) {
                            ui_layout_axis_next(split_axis);
                            UI_Box *row_or_column = ui_create_box(0);

                            ui_parent(row_or_column)
                            ui_padding(ui_size_pixels(drop_padding, 1.0f))
                            ui_palette(palette_from_theme(ThemePalette_Button)) {
                                if (targets[i].direction != Direction2_Invalid) {
                                    ui_create_box((split_side == Side_Min ? UI_BoxFlag_DrawBackground : 0) | UI_BoxFlag_DrawBorder);
                                    ui_spacer_sized(ui_size_pixels(drop_padding, 1.0f));
                                    ui_create_box((split_side == Side_Max ? UI_BoxFlag_DrawBackground : 0) | UI_BoxFlag_DrawBorder);
                                } else {
                                    ui_create_box(UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder);
                                }
                            }
                        }
                    }

                    ui_input_from_box(drop_site);

                    if (ui_keys_match(targets[i].ui_key, ui_drop_hot_key()) && drag_drop()) {
                        if (targets[i].direction == Direction2_Invalid) {
                            push_command(
                                CommandKind_MoveTab,
                                .window             = state->drag_context->window,
                                .panel              = state->drag_context->panel,
                                .tab                = state->drag_context->tab,
                                .destination_window = handle_from_window(window),
                                .destination_panel  = handle_from_panel(panel),
                                .previous_tab       = panel->active_tab
                            );
                        } else {
                            push_command(
                                CommandKind_SplitPanel,
                                .panel             = state->drag_context->panel,
                                .tab               = state->drag_context->tab,
                                .destination_panel = handle_from_panel(panel),
                                .direction         = targets[i].direction
                            );
                        }
                    }
                }

                ui_corner_radius(drop_corner_radius)
                ui_palette(palette_from_theme(ThemePalette_DropSiteOverlay))
                for (U64 i = 0; i < array_count(targets); ++i) {
                    if (ui_keys_match(targets[i].ui_key, ui_drop_hot_key())) {
                        Axis2 split_axis = axis2_from_direction2(targets[i].direction);
                        Side  split_side = side_from_direction2(targets[i].direction);

                        R2F32 target_rectangle = r2f32_pad(panel_rectangle, -drop_major_half_size);
                        if (targets[i].direction != Direction2_Invalid) {
                            target_rectangle.values[side_flip(split_side)].values[split_axis] = panel_center.values[split_axis];
                        }

                        R2F32 future_split_rectangle = r2f32(
                            ui_animate(ui_key_from_string(global_ui_null_key, str8_literal("tab_drop_min_x")), target_rectangle.min.x, .initial = panel_center.x),
                            ui_animate(ui_key_from_string(global_ui_null_key, str8_literal("tab_drop_min_y")), target_rectangle.min.y, .initial = panel_center.y),
                            ui_animate(ui_key_from_string(global_ui_null_key, str8_literal("tab_drop_max_x")), target_rectangle.max.x, .initial = panel_center.x),
                            ui_animate(ui_key_from_string(global_ui_null_key, str8_literal("tab_drop_max_y")), target_rectangle.max.y, .initial = panel_center.y)
                        );

                        ui_fixed_position_next(future_split_rectangle.min);
                        ui_width_next(ui_size_pixels(r2f32_size(future_split_rectangle).width, 1.0f));
                        ui_height_next(ui_size_pixels(r2f32_size(future_split_rectangle).height, 1.0f));
                        ui_create_box(UI_BoxFlag_DrawBackground);
                    }
                }
            }

            // NOTE(simon): Focus panel if the user clicks inside of it.
            for (UI_Event *event = 0; ui_next_event(&event);) {
                if (
                    event->kind == UI_EventKind_KeyPress && (
                        event->key == Gfx_Key_MouseLeft ||
                        event->key == Gfx_Key_MouseMiddle ||
                        event->key == Gfx_Key_MouseRight
                    ) &&
                    r2f32_contains_v2f32(panel_rectangle, event->position)
                ) {
                    push_command(CommandKind_FocusPanel);
                    break;
                }
            }

            // NOTE(simon): Inactive panel overlay.
            if (panel != panel_from_handle(window->active_panel)) {
                UI_Palette overlay = ui_palette_top();
                overlay.background = color_from_theme(ThemeColor_InactivePanelOverlay);
                ui_palette_next(overlay);
                ui_fixed_position_next(panel_rectangle.min);
                ui_width_next(ui_size_pixels(r2f32_size(panel_rectangle).width, 1.0f));
                ui_height_next(ui_size_pixels(r2f32_size(panel_rectangle).height, 1.0f));
                ui_create_box(UI_BoxFlag_DrawBackground | UI_BoxFlag_FloatingPosition);
            }

            F32   tab_height              = ui_size_ems(2.0f, 1.0f).value;
            R2F32 tab_bar_rectangle       = r2f32(panel_rectangle.min.x, panel_rectangle.min.y, panel_rectangle.max.x, panel_rectangle.min.y + tab_height);
            R2F32 panel_content_rectangle = r2f32(panel_rectangle.min.x, panel_rectangle.min.y + tab_height, panel_rectangle.max.x, panel_rectangle.max.y);

            // NOTE(simon): Build tab bar.
            ui_fixed_position_next(tab_bar_rectangle.min);
            ui_width_next(ui_size_pixels(r2f32_size(tab_bar_rectangle).width, 1.0f));
            ui_height_next(ui_size_pixels(r2f32_size(tab_bar_rectangle).height, 1.0f));
            ui_layout_axis_next(Axis2_X);
            UI_Box *tab_bar_box = ui_create_box_from_string_format(
                UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder | UI_BoxFlag_Clickable | UI_BoxFlag_OverflowX | UI_BoxFlag_Clip,
                "###tab_bar_box_%p", panel
            );

            // NOTE(simon): Build tabs.
            ui_width(ui_size_children_sum(1.0f))
            ui_height(ui_size_pixels(tab_height, 1.0f))
            ui_layout_axis(Axis2_X)
            ui_parent(tab_bar_box)
            ui_corner_radius_00(ui_size_ems(0.75f, 1.0f).value)
            ui_corner_radius_01(ui_size_ems(0.75f, 1.0f).value)
            for (Tab *tab = panel->first_tab; !is_nil_tab(tab); tab = tab->next) {
                ui_palette_push(palette_from_theme(tab == tab_from_handle(panel->active_tab) ? ThemePalette_Tab : ThemePalette_InactiveTab));

                ui_hover_cursor_next(Gfx_Cursor_Hand);
                UI_Box *tab_box = ui_create_box_from_string_format(
                    UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder | UI_BoxFlag_Clickable | UI_BoxFlag_DrawHot | UI_BoxFlag_DrawActive | UI_BoxFlag_AnimateX,
                    "###tab_%p", tab
                );

                ui_parent(tab_box) {
                    ui_text_align_next(UI_TextAlign_Left);
                    ui_width_next(ui_size_text_content(5.0f, 1.0f));
                    ui_label(tab->name);

                    ui_width_next(ui_size_ems(1.5f, 1.0f));
                    ui_text_align_next(UI_TextAlign_Center);
                    ui_hover_cursor_next(Gfx_Cursor_Hand);
                    ui_font_next(ui_icon_font());
                    UI_Box *close_box = ui_create_box_from_string_format(
                        UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawText |
                        UI_BoxFlag_DrawHot | UI_BoxFlag_DrawActive |
                        UI_BoxFlag_Clickable,
                        "%.*s##_tab_%p", str8_expand(ui_icon_string_from_kind(UI_IconKind_Close)), tab
                    );

                    UI_Input close_input = ui_input_from_box(close_box);
                    if (close_input.flags & UI_InputFlag_LeftClicked) {
                        push_command(CommandKind_CloseTab, .tab = handle_from_tab(tab));
                    }
                }

                UI_Input input = ui_input_from_box(tab_box);
                if (input.flags & UI_InputFlag_LeftPressed) {
                    next_active_tab = handle_from_tab(tab);
                }

                if (input.flags & UI_InputFlag_LeftDragging && v2f32_length(ui_drag_delta()) > 10.0f) {
                    drag_begin(ContextMember_Tab);
                }

                // NOTE(simon): Spacer between tabs.
                if (!is_nil_tab(tab->next)) {
                    ui_spacer_sized(ui_size_ems(0.3f, 1.0f));
                }

                ui_palette_pop();
            }
            ui_input_from_box(tab_bar_box);

            ui_fixed_position_next(panel_content_rectangle.min);
            ui_width_next(ui_size_pixels(r2f32_size(panel_content_rectangle).width, 1.0f));
            ui_height_next(ui_size_pixels(r2f32_size(panel_content_rectangle).height, 1.0f));
            ui_focus_next(UI_Focus_Active);
            UI_Box *content_box = ui_create_box_from_string_format(
                UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder | UI_BoxFlag_Clip | UI_BoxFlag_DisableFocusOverlay | (panel == panel_from_handle(window->active_panel) ? 0 : UI_BoxFlag_DisableFocusBorder) |
                UI_BoxFlag_Clickable,
                "###panel_box_%p", panel
            );

            ui_parent(content_box) {
                Tab *tab = tab_from_handle(panel->active_tab);
                tab->build(panel_content_rectangle);
            }

            panel->active_tab = next_active_tab;
            ui_focus_pop();
            pop_context();
        }
        prof_zone_end(prof_build_leaf_ui);

        ui_font_size_pop();
        ui_font_pop();
        ui_palette_pop();
        ui_end();

        memory_zero_struct(&window->ui_events);

        if (ui_is_animating_from_context(window->ui)) {
            request_frame();
        }

        // NOTE(simon): Draw UI.
        window->draw_list = draw_list_create();
        draw_list_scope(window->draw_list) {
            // NOTE(simon): Draw background.
            draw_rectangle(client_rectangle, color_from_theme(ThemeColor_BaseBackground), 0, 0, 0);

            // NOTE(simon): Draw border.
            draw_rectangle(r2f32_pad(client_rectangle, 1.0f), color_from_theme(ThemeColor_TitleBarBorder), 0, 1.0f, 1.0f);

            for (UI_Box *box = window->ui->root; !ui_box_is_null(box);) {
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

        pop_context();
    }



    // NOTE(simon): Draw UI.
    prof_zone_begin(prof_zone_render, "render");
    render_begin();
    for (Window *window = state->first_window; window; window = window->next) {
        render_window_begin(window->window, window->render);
        draw_submit_list(window->window, window->render, window->draw_list);
        render_window_end(window->window, window->render);
    }
    render_end();
    prof_zone_end(prof_zone_render);

    // NOTE(simon): Cancle drag-and-drpo if nothing caught it.
    if (state->drag_state == DragState_Dropping) {
        drag_cancel();
    }

    if (state->frames_to_render > 0) {
        --state->frames_to_render;
    }

    if (PROFILE_BUILD) {
        request_frame();
    }

    if (depth == 0) {
        pipewire_tick();
    }

    ++state->frame_index;

    arena_end_temporary(scratch);
    prof_frame_done();
}
