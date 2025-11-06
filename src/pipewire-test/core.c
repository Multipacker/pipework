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

    return tab;
}

internal Void destroy_tab(Tab *tab) {
    if (!is_nil_tab(tab)) {
        ++tab->generation;
        sll_stack_push(state->tab_freelist, tab);
    }
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

internal Void build_view(R2F32 client_rectangle) {
    V2F32 client_size = r2f32_size(client_rectangle);
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

    F32 row_height = 2.0f * (F32) ui_font_size_top();

    // NOTE(simon): Build panel for selected object.
    {
        Pipewire_Object *selected_object = pipewire_object_from_handle(state->selected_object);

        // NOTE(simon): Collect all properties.
        S64 property_count = 0;
        Pipewire_Property **properties = 0;
        {
            for (Pipewire_Property *property = selected_object->first_property; property; property = property->next) {
                ++property_count;
            }
            properties = arena_push_array(frame_arena(), Pipewire_Property *, (U64) property_count);
            for (Pipewire_Property *property = selected_object->first_property, **propertie_ptr = properties; property; property = property->next) {
                *propertie_ptr++ = property;
            }
        }

        UI_Key panel_key    = ui_key_from_string(ui_active_seed_key(), str8_literal("object_panel"));
        V2F32  panel_size   = v2f32(40.0f * (F32) ui_font_size_top(), client_size.height - 2.0f * row_height);
        F32    panel_offset = ui_animate(panel_key, !pipewire_object_is_nil(selected_object) * panel_size.width, .initial = 0);

        ui_width_next(ui_size_pixels(panel_size.width, 1.0f));
        ui_height_next(ui_size_pixels(panel_size.height, 1.0f));
        ui_fixed_x_next(client_rectangle.max.x - panel_offset);
        ui_fixed_y_next((client_rectangle.max.y - panel_size.y) * 0.5f);
        ui_layout_axis_next(Axis2_Y);
        UI_Box *panel = ui_create_box_from_key(
            UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder | UI_BoxFlag_DrawDropShadow | UI_BoxFlag_Clip |
            UI_BoxFlag_Scrollable | UI_BoxFlag_Clickable,
            panel_key
        );

        ui_parent(panel)
        ui_text_x_padding(5.0f) {
            ui_height(ui_size_pixels(row_height, 1.0f))
            ui_row()
            ui_width(ui_size_text_content(0, 1.0f)) {
                ui_font_next(ui_icon_font());
                ui_text_x_padding_next(0.0f);
                ui_text_align_next(UI_TextAlign_Center);
                ui_width_next(ui_size_ems(1.5f, 1.0f));
                UI_Input close_input = ui_button_format("%.*s###close_panel", str8_expand(ui_icon_string_from_kind(UI_IconKind_Close)));
                if (close_input.flags & UI_InputFlag_Clicked) {
                    state->selected_object_next = pipewire_handle_from_object(0);
                }
                ui_label(kind_from_object(selected_object));
                ui_spacer_sized(ui_size_ems(0.5f, 1.0f));
                ui_label(name_from_object(selected_object));
            }

            if (selected_object->kind == Pipewire_Object_Module) {
                ui_width(ui_size_parent_percent(1.0f, 1.0f))
                ui_height(ui_size_pixels(row_height, 1.0f)) {
                    Str8 name        = pipewire_object_property_string_from_name(selected_object, str8_literal("module.name"));
                    Str8 author      = pipewire_object_property_string_from_name(selected_object, str8_literal("module.author"));
                    Str8 description = pipewire_object_property_string_from_name(selected_object, str8_literal("module.description"));
                    Str8 usage       = pipewire_object_property_string_from_name(selected_object, str8_literal("module.usage"));
                    Str8 version     = pipewire_object_property_string_from_name(selected_object, str8_literal("module.version"));

                    ui_label_format("%.*s v%.*s", str8_expand(name), str8_expand(version));

                    if (author.size) {
                        ui_spacer_sized(ui_size_ems(1.0f, 1.0f));
                        ui_label(str8_literal("Author"));
                        ui_label(author);
                    }

                    if (description.size) {
                        ui_spacer_sized(ui_size_ems(1.0f, 1.0f));
                        ui_label(str8_literal("Description"));
                        ui_label(description);
                    }

                    if (usage.size) {
                        ui_spacer_sized(ui_size_ems(1.0f, 1.0f));
                        ui_label(str8_literal("Usage"));
                        ui_font_next(font_cache_font_from_static_data(&mono_font));
                        ui_label(usage);
                    }
                }
            } else if (selected_object->kind == Pipewire_Object_Factory) {
                ui_width(ui_size_parent_percent(1.0f, 1.0f))
                ui_height(ui_size_pixels(row_height, 1.0f)) {
                    Str8 name         = pipewire_object_property_string_from_name(selected_object, str8_literal("factory.name"));
                    Str8 type_name    = pipewire_object_property_string_from_name(selected_object, str8_literal("factory.type.name"));
                    Str8 type_version = pipewire_object_property_string_from_name(selected_object, str8_literal("factory.type.version"));
                    Str8 usage        = pipewire_object_property_string_from_name(selected_object, str8_literal("factory.usage"));
                    U32  module_id    = pipewire_object_property_u32_from_name(selected_object, str8_literal("module.id"));

                    ui_label(name);

                    ui_spacer_sized(ui_size_ems(1.0f, 1.0f));
                    ui_label(str8_literal("Creates"));
                    ui_label_format("%.*s v%.*s", str8_expand(type_name), str8_expand(type_version));

                    Pipewire_Object *module = pipewire_object_from_id(module_id);
                    if (!pipewire_object_is_nil(module)) {
                        ui_spacer_sized(ui_size_ems(1.0f, 1.0f));
                        ui_row()
                        ui_width(ui_size_parent_percent(0.5f, 1.0f)) {
                            ui_label(str8_literal("Module"));
                            ui_palette_next(palette_from_theme(ThemePalette_Button));
                            object_button(module);
                        }
                    }

                    if (usage.size) {
                        ui_spacer_sized(ui_size_ems(1.0f, 1.0f));
                        ui_label(str8_literal("Usage"));
                        ui_font_next(font_cache_font_from_static_data(&mono_font));
                        ui_label(usage);
                    }
                }
            } else if (selected_object->kind == Pipewire_Object_Link) {
                ui_width(ui_size_parent_percent(1.0f, 1.0f))
                ui_height(ui_size_pixels(row_height, 1.0f)) {
                    U32 output_node_id = pipewire_object_property_u32_from_name(selected_object, str8_literal("link.output.node"));
                    U32 output_port_id = pipewire_object_property_u32_from_name(selected_object, str8_literal("link.output.port"));
                    U32 input_node_id  = pipewire_object_property_u32_from_name(selected_object, str8_literal("link.input.node"));
                    U32 input_port_id  = pipewire_object_property_u32_from_name(selected_object, str8_literal("link.input.port"));
                    U32 factory_id     = pipewire_object_property_u32_from_name(selected_object, str8_literal("factory.id"));
                    U32 client_id      = pipewire_object_property_u32_from_name(selected_object, str8_literal("client.id"));

                    Pipewire_Object *output_node = pipewire_object_from_id(output_node_id);
                    Pipewire_Object *output_port = pipewire_object_from_id(output_port_id);
                    Pipewire_Object *input_node  = pipewire_object_from_id(input_node_id);
                    Pipewire_Object *input_port  = pipewire_object_from_id(input_port_id);
                    Pipewire_Object *factory     = pipewire_object_from_id(factory_id);
                    Pipewire_Object *client      = pipewire_object_from_id(client_id);

                    ui_label(str8_literal("Ownership"));
                    ui_row()
                    ui_width(ui_size_parent_percent(0.5f, 1.0f)) {
                        ui_label(str8_literal("Client"));
                        ui_palette_next(palette_from_theme(ThemePalette_Button));
                        object_button(client);
                    }
                    ui_row()
                    ui_width(ui_size_parent_percent(0.5f, 1.0f)) {
                        ui_label(str8_literal("Factory"));
                        ui_palette_next(palette_from_theme(ThemePalette_Button));
                        object_button(factory);
                    }

                    ui_spacer_sized(ui_size_ems(1.0f, 1.0f));
                    ui_label(str8_literal("Output"));
                    ui_row()
                    ui_width(ui_size_parent_percent(0.5f, 1.0f)) {
                        ui_label(str8_literal("Node"));
                        ui_palette_next(palette_from_theme(ThemePalette_Button));
                        object_button(output_node);
                    }
                    ui_row()
                    ui_width(ui_size_parent_percent(0.5f, 1.0f)) {
                        ui_label(str8_literal("Port"));
                        ui_palette_next(palette_from_theme(ThemePalette_Button));
                        object_button(output_port);
                    }

                    ui_spacer_sized(ui_size_ems(1.0f, 1.0f));
                    ui_label(str8_literal("Input"));
                    ui_row()
                    ui_width(ui_size_parent_percent(0.5f, 1.0f)) {
                        ui_label(str8_literal("Node"));
                        ui_palette_next(palette_from_theme(ThemePalette_Button));
                        object_button(input_node);
                    }
                    ui_row()
                    ui_width(ui_size_parent_percent(0.5f, 1.0f)) {
                        ui_label(str8_literal("Port"));
                        ui_palette_next(palette_from_theme(ThemePalette_Button));
                        object_button(input_port);
                    }
                }
            } else {
                R1S64 visible_range = { 0 };
                local UI_ScrollPosition scroll_position = { 0 };
                ui_palette(palette_from_theme(ThemePalette_Button))
                ui_scroll_region(v2f32(panel_size.x, panel_size.y - row_height), row_height, property_count, &visible_range, 0, &scroll_position) {
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
                        ui_row()
                        ui_width(ui_size_parent_percent(0.5f, 1.0f)) {
                            ui_label(property->name);

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
        ui_input_from_box(panel);
    }

    ui_text_x_padding(5.0f)
    ui_width(ui_size_fill())
    ui_height(ui_size_fill())
    ui_row() {
        // NOTE(simon): Draw list of all objects.
        V2F32 list_size = v2f32(25.0f * (F32) ui_font_size_top(), client_size.height);
        local UI_ScrollPosition scroll_position = { 0 };
        R1S64 visible_range = { 0 };
        ui_palette(palette_from_theme(ThemePalette_Button))
        ui_scroll_region(list_size, row_height, object_count, &visible_range, 0, &scroll_position) {
            ui_width(ui_size_fill())
            ui_height(ui_size_pixels(row_height, 1.0f))
            for (S64 i = visible_range.min; i < visible_range.max; ++i) {
                Pipewire_Object *object = objects[i];
                object_button(object);
            }
        }

        // NOTE(simon): Build graph.
        ui_width(ui_size_pixels(client_size.width - list_size.width, 1.0f));
        ui_height(ui_size_pixels(client_size.height, 1.0f));
        UI_Box *node_graph_box = ui_create_box_from_string(UI_BoxFlag_Clip | UI_BoxFlag_Clickable | UI_BoxFlag_Scrollable, str8_literal("###node_graph"));
        ui_parent(node_graph_box) {
            V2F32 no_ports_offset     = v2f32(0.0f * 20.0f * (F32) ui_font_size_top(), 0.0f);
            V2F32 only_output_offset  = v2f32(1.0f * 20.0f * (F32) ui_font_size_top(), 0.0f);
            V2F32 input_output_offset = v2f32(2.0f * 20.0f * (F32) ui_font_size_top(), 0.0f);
            V2F32 only_inputs_offset  = v2f32(3.0f * 20.0f * (F32) ui_font_size_top(), 0.0f);

            typedef struct PortNode PortNode;
            struct PortNode {
                PortNode        *next;
                PortNode        *previous;
                UI_Box          *box;
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
                for (GraphNode *candidate = state->first_node; candidate; candidate = candidate->next) {
                    if (pipewire_object_from_handle(candidate->handle) == node) {
                        graph_node = candidate;
                        break;
                    }
                }
                if (!graph_node) {
                    graph_node = state->node_freelist;
                    if (graph_node) {
                        sll_stack_pop(state->node_freelist);
                        memory_zero_struct(graph_node);
                    } else {
                        graph_node = arena_push_struct(state->arena, GraphNode);
                    }
                    graph_node->handle = pipewire_handle_from_object(node);
                    graph_node->position = default_position;
                    dll_push_back(state->first_node, state->last_node, graph_node);
                }
                graph_node->last_frame_used = state->frame_index;

                if (pipewire_object_from_handle(state->hovered_object) == node) {
                    UI_Palette palette = ui_palette_top();
                    palette.background = color_from_theme(ThemeColor_Focus);
                    ui_palette_next(palette);
                }

                // NOTE(simon): Build node.
                ui_corner_radius_next(5.0f);
                ui_fixed_position_next(v2f32_subtract(graph_node->position, state->graph_offset));
                ui_width_next(ui_size_pixels(node_width, 1.0f));
                ui_height_next(ui_size_pixels(node_height, 1.0f));
                ui_layout_axis_next(Axis2_Y);
                UI_Box *node_box = ui_create_box_from_string_format(UI_BoxFlag_DrawBorder | UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawDropShadow | UI_BoxFlag_Clickable, "###node_%u", node->id);
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
                    for (Pipewire_Object *child = node->first; !pipewire_object_is_nil(child); child = child->next) {
                        PortNode *port_node = arena_push_struct(frame_arena(), PortNode);
                        port_node->port = child;

                        Str8 direction = pipewire_object_property_string_from_name(child, str8_literal("port.direction"));
                        Str8 port_name = pipewire_object_property_string_from_name(child, str8_literal("port.name"));

                        UI_Input input = { 0 };
                        if (str8_equal(direction, str8_literal("in"))) {
                            ui_parent_next(input_column);
                            ui_text_align_next(UI_TextAlign_Left);
                            input = ui_button_format("%.*s###port_%p", str8_expand(port_name), child);
                        } else if (str8_equal(direction, str8_literal("out"))) {
                            ui_parent_next(output_column);
                            ui_text_align_next(UI_TextAlign_Right);
                            input = ui_button_format("%.*s###port_%p", str8_expand(port_name), child);
                        }

                        port_node->box = input.box;
                        if (input.flags & UI_InputFlag_Clicked) {
                            if (!pipewire_object_is_nil(pipewire_object_from_handle(state->selected_port))) {
                                pipewire_link(state->selected_port, pipewire_handle_from_object(child));
                                state->selected_port = pipewire_handle_from_object(&pipewire_nil_object);
                            } else {
                                state->selected_port = pipewire_handle_from_object(child);
                            }
                        }

                        if (port_node->box) {
                            dll_push_back(first_port, last_port, port_node);
                        }
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
                PortNode *output_node = 0;
                PortNode *input_node  = 0;
                U32 output_port_id = pipewire_object_property_u32_from_name(node, str8_literal("link.output.port"));
                U32 input_port_id  = pipewire_object_property_u32_from_name(node, str8_literal("link.input.port"));
                for (PortNode *port_node = first_port; port_node; port_node = port_node->next) {
                    if (port_node->port->id == output_port_id) {
                        output_node = port_node;
                    } else if (port_node->port->id == input_port_id) {
                        input_node = port_node;
                    }
                }

                // NOTE(simon): Draw two quadratic beziers to approximmate the
                // look of a cubic beizer between ports.
                if (output_node && input_node) {
                    R2F32 absolute_output_rectangle = output_node->box->calculated_rectangle;
                    R2F32 absolute_input_rectangle  = input_node->box->calculated_rectangle;
                    V2F32 output_point = v2f32_subtract(v2f32(absolute_output_rectangle.max.x, 0.5f * (absolute_output_rectangle.min.y + absolute_output_rectangle.max.y)), node_graph_box->calculated_rectangle.min);
                    V2F32 input_point  = v2f32_subtract(v2f32(absolute_input_rectangle.min.x,  0.5f * (absolute_input_rectangle.min.y  + absolute_input_rectangle.max.y)),  node_graph_box->calculated_rectangle.min);
                    V2F32 middle       = v2f32_scale(v2f32_add(input_point, output_point), 0.5f);
                    V2F32 c0_control   = v2f32(output_point.x + f32_abs(input_point.x - output_point.x) * 0.25f, output_point.y);
                    V2F32 c1_control   = v2f32(input_point.x  - f32_abs(input_point.x - output_point.x) * 0.25f, input_point.y);
                    draw_bezier(output_point, c0_control, middle,      color_from_theme(ThemeColor_Text), 2.0f, 1.0f, 1.0f);
                    draw_bezier(middle,       c1_control, input_point, color_from_theme(ThemeColor_Text), 2.0f, 1.0f, 1.0f);
                }
            }
            ui_box_set_draw_list(node_graph_box, connections);
        }
        UI_Input node_graph_input = ui_input_from_box(node_graph_box);
        state->graph_offset = v2f32_subtract(state->graph_offset, v2f32_scale(node_graph_input.scroll, row_height));
        if (node_graph_input.flags & UI_InputFlag_RightDragging) {
            if (node_graph_input.flags & UI_InputFlag_RightPressed) {
                V2F32 drag_data = state->graph_offset;
                ui_set_drag_data(&drag_data);
            }

            V2F32 position_pre_drag = *ui_get_drag_data(V2F32);
            V2F32 position_post_drag = v2f32_subtract(position_pre_drag, ui_drag_delta());
            state->graph_offset = position_post_drag;
        }
    }
}

internal Void update(Void) {
    local U32 depth = 0;

    Arena_Temporary scratch = arena_get_scratch(0, 0);
    arena_reset(frame_arena());

    state->selected_object = state->selected_object_next;
    state->hovered_object  = state->hovered_object_next;
    memory_zero_struct(&state->hovered_object_next);

    Gfx_EventList graphics_events = { 0 };
    if (depth == 0) {
        ++depth;
        graphics_events = gfx_get_events(scratch.arena, state->frames_to_render == 0);
        --depth;
    }



    // NOTE(simon): Consume events.
    for (Gfx_Event *event = graphics_events.first, *next = 0; event; event = next) {
        next = event->next;
        request_frame();

        Window *window = window_from_gfx_handle(event->window);
        Handle  handle = handle_from_window(window);

        B32 consume = false;

        if (event->kind == Gfx_EventKind_Quit) {
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

        if (consume) {
            dll_remove(graphics_events.first, graphics_events.last, event);
        }
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
        V2U32 client_size      = gfx_client_area_from_window(window->window);
        R2F32 client_rectangle = r2f32(0.0f, 0.0f, (F32) client_size.x, (F32) client_size.y);

        ui_select_state(window->ui);
        ui_begin(window->window, &window->ui_events, &icon_info, 1.0f / 60.0f);
        ui_palette_push(palette_from_theme(ThemePalette_Base));
        ui_font_push(font_cache_font_from_static_data(&default_font));
        ui_font_size_push((U32) (state->font_size * gfx_dpi_from_window(window->window) / 72.0f));

        R2F32 top_bar_rectangle = { 0 };
        F32 border_width = 0.0f;
        R2F32 content_rectangle = r2f32_pad(r2f32(client_rectangle.min.x, top_bar_rectangle.max.y, client_rectangle.max.x, client_rectangle.max.y), -border_width);
        V2F32 content_size      = r2f32_size(content_rectangle);

        F32 panel_pad = 2.0f;

        // NOTE(simon): Build non-leaf panel UI.
        prof_zone_begin(prof_bulid_non_leaf_ui, "non-leaf ui");
        for (Panel *panel = window->root_panel; !is_nil_panel(panel); panel = panel_iterator_depth_first_pre_order(panel, window->root_panel).next) {
            if (is_nil_panel(panel->first)) {
                continue;
            }

            R2F32 panel_rectangle      = rectangle_from_panel(panel, content_rectangle);
            V2F32 panel_rectangle_size = r2f32_size(panel_rectangle);
            V2F32 panel_center         = r2f32_center(panel_rectangle);

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

        // NOTE(simon): Build leaf panel UI.
        prof_zone_begin(prof_build_leaf_ui, "leaf ui");
        for (Panel *panel = window->root_panel; !is_nil_panel(panel); panel = panel_iterator_depth_first_pre_order(panel, window->root_panel).next) {
            if (!is_nil_panel(panel->first)) {
                continue;
            }

            R2F32 panel_rectangle = r2f32_pad(rectangle_from_panel(panel, client_rectangle), -panel_pad);
            Handle next_active_tab = panel->active_tab;

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
                    }
                }

                UI_Input input = ui_input_from_box(tab_box);
                if (input.flags & UI_InputFlag_LeftPressed) {
                    next_active_tab = handle_from_tab(tab);
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
                UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder | UI_BoxFlag_Clip | UI_BoxFlag_DisableFocusOverlay |
                UI_BoxFlag_Clickable,
                "###panel_box_%p", panel
            );

            ui_parent(content_box) {
                build_view(panel_content_rectangle);
            }

            panel->active_tab = next_active_tab;
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

    // NOTE(simon): Evict untouched entries from graph node cache.
    for (GraphNode *node = state->first_node, *next = 0; node; node = next) {
        next = node->next;

        if (node->last_frame_used != state->frame_index) {
            dll_remove(state->first_node, state->last_node, node);
            sll_stack_push(state->node_freelist, node);
        }
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
