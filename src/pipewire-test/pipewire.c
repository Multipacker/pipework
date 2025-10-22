global Pipewire_State *pipewire_state;

internal B32 pipewire_object_is_nil(Pipewire_Object *object) {
    B32 result = !object || object == &pipewire_nil_object;
    return result;
}

internal Void pipewire_add_child(Pipewire_Object *parent, Pipewire_Object *child) {
    child->parent = parent;
    dll_insert_next_previous_zero(parent->first, parent->last, parent->last, child, next, previous, &pipewire_nil_object);
}

internal Void pipewire_remove_child(Pipewire_Object *parent, Pipewire_Object *child) {
    child->parent = &pipewire_nil_object;
    dll_remove_next_previous_zero(parent->first, parent->last, child, next, previous, &pipewire_nil_object);
}

internal Pipewire_Object *pipewire_create_object(U32 id) {
    Pipewire_Object *object = pipewire_state->object_freelist;
    if (object) {
        sll_stack_pop(object);
    } else {
        object = arena_push_struct(pipewire_state->arena, Pipewire_Object);
    }

    dll_insert_next_previous_zero(pipewire_state->first_object, pipewire_state->last_object, pipewire_state->last_object, object, all_next, all_previous, 0);
    object->id = id;

    object->parent   = &pipewire_nil_object;
    object->first    = &pipewire_nil_object;
    object->last     = &pipewire_nil_object;
    object->next     = &pipewire_nil_object;
    object->previous = &pipewire_nil_object;

    return object;
}

internal Void pipewire_destroy_object(Pipewire_Object *object) {
    if (!pipewire_object_is_nil(object->parent)) {
        pipewire_remove_child(object->parent, object);
    }

    if (!pipewire_object_is_nil(object->first)) {
        for (Pipewire_Object *child = object->first, *next = &pipewire_nil_object; !pipewire_object_is_nil(child); child = next) {
            next = child->next;
            pipewire_remove_child(object, child);
        }
    }

    for (Pipewire_Property *property = object->first_property, *next = 0; property; property = next) {
        next = property->next;
        dll_remove(object->first_property, object->last_property, property);
        pipewire_string_free(property->name);
        pipewire_string_free(property->value);
        sll_stack_push(pipewire_state->property_freelist, property);
    }

    pw_proxy_destroy(object->proxy);

    ++object->generation;
    dll_remove_next_previous_zero(pipewire_state->first_object, pipewire_state->last_object, object, all_next, all_previous, 0);
    sll_stack_push(pipewire_state->object_freelist, object);
}



internal B32 pipewire_property_is_nil(Pipewire_Property *property) {
    B32 result = !property || property == &pipewire_nil_property;
    return result;
}

internal Void pipewire_object_update_property(Pipewire_Object *object, Str8 name, Str8 value) {
    Pipewire_Property *existing_property = pipewire_object_property_from_name(object, name);

    if (!pipewire_property_is_nil(existing_property)) {
        pipewire_string_free(existing_property->value);
        existing_property->value = pipewire_string_allocate(value);
    } else {
        Pipewire_Property *property = pipewire_state->property_freelist;
        if (property) {
            sll_stack_pop(pipewire_state->property_freelist);
        } else {
            property = arena_push_struct(pipewire_state->arena, Pipewire_Property);
        }

        property->name = pipewire_string_allocate(name);
        property->value = pipewire_string_allocate(value);
        dll_push_back(object->first_property, object->last_property, property);
    }
}

internal Pipewire_Property *pipewire_object_property_from_name(Pipewire_Object *object, Str8 name) {
    Pipewire_Property *result = &pipewire_nil_property;
    for (Pipewire_Property *property = object->first_property; property; property = property->next) {
        if (str8_equal(property->name, name)) {
            result = property;
            break;
        }
    }
    return result;
}

internal Str8 pipewire_object_property_string_from_name(Pipewire_Object *object, Str8 name) {
    Pipewire_Property *property = pipewire_object_property_from_name(object, name);
    return property->value;
}

internal U32 pipewire_object_property_u32_from_name(Pipewire_Object *object, Str8 name) {
    Pipewire_Property *property = pipewire_object_property_from_name(object, name);
    U64Decode decode = u64_from_str8(property->value);
    return (U32) decode.value;
}



internal U64 pipewire_string_chunk_index_from_size(U64 size) {
    U64 chunk_index = 0;
    if (size) {
        for (U64 i = 0; i < array_count(pipewire_string_chunk_sizes); ++i) {
            if (size <= pipewire_string_chunk_sizes[i]) {
                chunk_index = 1 + i;
                break;
            }
        }
    }
    return chunk_index;
}

internal Str8 pipewire_string_allocate(Str8 string) {
    U64 chunk_index = pipewire_string_chunk_index_from_size(string.size);
    Pipewire_StringChunkNode *chunk = 0;
    if (chunk_index == array_count(pipewire_string_chunk_sizes)) {
        Pipewire_StringChunkNode *best_chunk_previous = 0;
        Pipewire_StringChunkNode *best_chunk = 0;
        for (Pipewire_StringChunkNode *candidate = pipewire_state->string_chunk_freelist[chunk_index - 1], *previous = 0; candidate; previous = candidate, candidate = candidate->next) {
            if (string.size <= candidate->size && (!best_chunk || candidate->size < best_chunk->size)) {
                best_chunk_previous = previous;
                best_chunk = candidate;
            }
        }

        if (best_chunk) {
            chunk = best_chunk;
            if (best_chunk_previous) {
                best_chunk_previous->next = best_chunk->next;
            } else {
                pipewire_state->string_chunk_freelist[chunk_index - 1] = best_chunk->next;
            }
        } else {
            U64 size = u64_ceil_to_power_of_2(string.size);
            chunk = (Pipewire_StringChunkNode *) arena_push_array_no_zero(pipewire_state->arena, U8, size);
        }
    } else if (array_count(pipewire_string_chunk_sizes)) {
        chunk = pipewire_state->string_chunk_freelist[chunk_index];
        if (chunk) {
            sll_stack_pop(pipewire_state->string_chunk_freelist[chunk_index - 1]);
        } else {
            chunk = (Pipewire_StringChunkNode *) arena_push_array_no_zero(pipewire_state->arena, U8, pipewire_string_chunk_sizes[chunk_index - 1]);
        }
    }

    Str8 result = { 0 };
    if (chunk) {
        result.data = (U8 *) chunk;
        result.size = string.size;
        memory_copy(result.data, string.data, string.size);
    }
    return result;
}

internal Void pipewire_string_free(Str8 string) {
    U64 chunk_index = pipewire_string_chunk_index_from_size(string.size);
    if (chunk_index) {
        Pipewire_StringChunkNode *chunk = (Pipewire_StringChunkNode *) string.data;
        chunk->size = u64_ceil_to_power_of_2(string.size);
        sll_stack_push(pipewire_state->string_chunk_freelist[chunk_index - 1], chunk);
    }
}



internal Pipewire_Handle pipewire_handle_from_object(Pipewire_Object *object) {
    Pipewire_Handle handle = { 0 };
    handle.u64[0] = integer_from_pointer(object);
    handle.u64[1] = object->generation;
    return handle;
}

internal Pipewire_Object *pipewire_object_from_handle(Pipewire_Handle handle) {
    Pipewire_Object *object = (Pipewire_Object *) pointer_from_integer(handle.u64[0]);
    if (!object || object->generation != handle.u64[1]) {
        object = &pipewire_nil_object;
    }
    return object;
}

internal Pipewire_Object *pipewire_object_from_id(U32 id) {
    Pipewire_Object *result = &pipewire_nil_object;
    for (Pipewire_Object *object = pipewire_state->first_object; object; object = object->all_next) {
        if (object->id == id) {
            result = object;
            break;
        }
    }
    return result;
}



internal Void pipewire_module_info(Void *data, const struct pw_module_info *info) {
    Pipewire_Object *module = (Pipewire_Object *) data;

    printf("module: id:%u\n", info->id);
    printf("  filename:  %s\n", info->filename);
    printf("  arguments: %s\n", info->args);
    printf("  change mask:");
    if (info->change_mask & PW_MODULE_CHANGE_MASK_PROPS)  printf(" PROPS");
    printf("\n");
    printf("  props:\n");
    const struct spa_dict_item *item = 0;
    spa_dict_for_each(item, info->props) {
        pipewire_object_update_property(module, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
        printf("    %s: \"%s\"\n", item->key, item->value);
    }
}



internal Void pipewire_factory_info(Void *data, const struct pw_factory_info *info) {
    Pipewire_Object *factory = (Pipewire_Object *) data;

    printf("factory: id:%u\n", info->id);
    printf("  type: %s\n", info->type);
    printf("  change mask:");
    if (info->change_mask & PW_FACTORY_CHANGE_MASK_PROPS)  printf(" PROPS");
    printf("\n");
    printf("  props:\n");
    const struct spa_dict_item *item = 0;
    spa_dict_for_each(item, info->props) {
        pipewire_object_update_property(factory, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
        printf("    %s: \"%s\"\n", item->key, item->value);
    }
}



internal Void pipewire_client_info(Void *data, const struct pw_client_info *info) {
    Pipewire_Object *client = (Pipewire_Object *) data;

    printf("client: id:%u\n", info->id);
    printf("  change mask:");
    if (info->change_mask & PW_CLIENT_CHANGE_MASK_PROPS)  printf(" PROPS");
    printf("\n");
    printf("  props:\n");
    const struct spa_dict_item *item = 0;
    spa_dict_for_each(item, info->props) {
        pipewire_object_update_property(client, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
        printf("    %s: \"%s\"\n", item->key, item->value);
    }
}



internal Void pipewire_node_info(Void *data, const struct pw_node_info *info) {
    Pipewire_Object *node = (Pipewire_Object *) data;

    printf("node: id:%u\n", info->id);
    printf("  max input ports:  %u\n", info->max_input_ports);
    printf("  max output ports: %u\n", info->max_output_ports);
    printf("  change mask:");
    if (info->change_mask & PW_NODE_CHANGE_MASK_INPUT_PORTS)  printf(" INPUT_PORTS");
    if (info->change_mask & PW_NODE_CHANGE_MASK_OUTPUT_PORTS) printf(" OUTPUT_PORTS");
    if (info->change_mask & PW_NODE_CHANGE_MASK_STATE)        printf(" STATE");
    if (info->change_mask & PW_NODE_CHANGE_MASK_PROPS)        printf(" PROPS");
    if (info->change_mask & PW_NODE_CHANGE_MASK_PARAMS)       printf(" PARAMS");
    printf("\n");
    printf("  n input ports:    %u\n", info->n_input_ports);
    printf("  n output ports:   %u\n", info->n_output_ports);
    printf("  state:            %s\n", pw_node_state_as_string(info->state));
    printf("  props:\n");
    const struct spa_dict_item *item = 0;
    spa_dict_for_each(item, info->props) {
        pipewire_object_update_property(node, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
        printf("    %s: \"%s\"\n", item->key, item->value);
    }
    printf("  params:\n");
    Arena_Temporary scratch = arena_get_scratch(0, 0);
    U32 *ids = arena_push_array(scratch.arena, U32, info->n_params);
    for (U32 i = 0; i < info->n_params; ++i) {
        ids[i] = info->params[i].id;
    }
    pw_node_subscribe_params((struct pw_node *) node->proxy, ids, info->n_params);
    arena_end_temporary(scratch);
    //pw_node_enum_params((struct pw_node *) node->proxy, 0, PW_ID_ANY, 0, info->n_params, 0);
    for (U32 i = 0; i < info->n_params; ++i) {
        CStr param_id_string = "";
        switch (info->params[i].id) {
            case SPA_PARAM_Invalid:        param_id_string = "Invalid";        break;
            case SPA_PARAM_PropInfo:       param_id_string = "PropInfo";       break;
            case SPA_PARAM_Props:          param_id_string = "Props";          break;
            case SPA_PARAM_EnumFormat:     param_id_string = "EnumFormat";     break;
            case SPA_PARAM_Format:         param_id_string = "Format";         break;
            case SPA_PARAM_Buffers:        param_id_string = "Buffers";        break;
            case SPA_PARAM_Meta:           param_id_string = "Meta";           break;
            case SPA_PARAM_IO:             param_id_string = "IO";             break;
            case SPA_PARAM_EnumProfile:    param_id_string = "EnumProfile";    break;
            case SPA_PARAM_Profile:        param_id_string = "Profile";        break;
            case SPA_PARAM_EnumPortConfig: param_id_string = "EnumPortConfig"; break;
            case SPA_PARAM_PortConfig:     param_id_string = "PortConfig";     break;
            case SPA_PARAM_EnumRoute:      param_id_string = "EnumRoute";      break;
            case SPA_PARAM_Route:          param_id_string = "Route";          break;
            case SPA_PARAM_Control:        param_id_string = "Control";        break;
            case SPA_PARAM_Latency:        param_id_string = "Latency";        break;
            case SPA_PARAM_ProcessLatency: param_id_string = "ProcessLatency"; break;
            case SPA_PARAM_Tag:            param_id_string = "Tag";            break;
        }
        printf("    id: %s\n", param_id_string);
    }

    const struct spa_dict_item *device_id_item = spa_dict_lookup_item(info->props, PW_KEY_DEVICE_ID);
    const struct spa_dict_item *client_id_item = spa_dict_lookup_item(info->props, PW_KEY_CLIENT_ID);

    if (node->parent) {
        pipewire_remove_child(node->parent, node);
    }

    if (device_id_item) {
        U32 device_id = 0;
        if (spa_atou32(device_id_item->value, &device_id, 10)) {
            Pipewire_Object *device = pipewire_object_from_id(device_id);
            if (!pipewire_object_is_nil(device)) {
                pipewire_add_child(device, node);
                printf("attached node:%u to device:%u\n", node->id, device->id);
            }
        }
    } else if (client_id_item) {
        U32 client_id = 0;
        if (spa_atou32(client_id_item->value, &client_id, 10)) {
            Pipewire_Object *client = pipewire_object_from_id(client_id);
            if (!pipewire_object_is_nil(client)) {
                pipewire_add_child(client, node);
                printf("attached node:%u to client:%u\n", node->id, client->id);
            }
        }
    }
}

internal Void pipewire_node_param(Void *data, S32 seq, U32 id, U32 index, U32 next, const struct spa_pod *param) {
    Pipewire_Object *node = (Pipewire_Object *) data;
    printf("node: id:%u\n", node->id);
    printf("  param %u:\n", index);
    CStr param_type_string = "";
    switch (id) {
        case SPA_PARAM_Invalid:        param_type_string = "Invalid";        break;
        case SPA_PARAM_PropInfo:       param_type_string = "PropInfo";       break;
        case SPA_PARAM_Props:          param_type_string = "Props";          break;
        case SPA_PARAM_EnumFormat:     param_type_string = "EnumFormat";     break;
        case SPA_PARAM_Format:         param_type_string = "Format";         break;
        case SPA_PARAM_Buffers:        param_type_string = "Buffers";        break;
        case SPA_PARAM_Meta:           param_type_string = "Meta";           break;
        case SPA_PARAM_IO:             param_type_string = "IO";             break;
        case SPA_PARAM_EnumProfile:    param_type_string = "EnumProfile";    break;
        case SPA_PARAM_Profile:        param_type_string = "Profile";        break;
        case SPA_PARAM_EnumPortConfig: param_type_string = "EnumPortConfig"; break;
        case SPA_PARAM_PortConfig:     param_type_string = "PortConfig";     break;
        case SPA_PARAM_EnumRoute:      param_type_string = "EnumRoute";      break;
        case SPA_PARAM_Route:          param_type_string = "Route";          break;
        case SPA_PARAM_Control:        param_type_string = "Control";        break;
        case SPA_PARAM_Latency:        param_type_string = "Latency";        break;
        case SPA_PARAM_ProcessLatency: param_type_string = "ProcessLatency"; break;
        case SPA_PARAM_Tag:            param_type_string = "Tag";            break;
    }
    printf("  id: %s\n", param_type_string);
}



internal Void pipewire_port_info(Void *data, const struct pw_port_info *info) {
    Pipewire_Object *port = (Pipewire_Object *) data;

    const char *direction = "";
    switch (info->direction) {
        case PW_DIRECTION_INPUT:  direction = "input";  break;
        case PW_DIRECTION_OUTPUT: direction = "output"; break;
    }
    printf("port: id:%u\n", info->id);
    printf("  direction:            %s\n", direction);
    printf("  change mask:");
    if (info->change_mask & PW_PORT_CHANGE_MASK_PROPS)  printf(" PROPS");
    if (info->change_mask & PW_PORT_CHANGE_MASK_PARAMS) printf(" PARAMS");
    printf("\n");
    printf("  props:\n");
    const struct spa_dict_item *item = 0;
    spa_dict_for_each(item, info->props) {
        pipewire_object_update_property(port, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
        printf("    %s: \"%s\"\n", item->key, item->value);
    }

    const struct spa_dict_item *node_id_item = spa_dict_lookup_item(info->props, PW_KEY_NODE_ID);

    if (port->parent) {
        pipewire_remove_child(port->parent, port);
    }

    if (node_id_item) {
        U32 node_id = 0;
        if (spa_atou32(node_id_item->value, &node_id, 10)) {
            Pipewire_Object *node = pipewire_object_from_id(node_id);
            if (!pipewire_object_is_nil(node)) {
                pipewire_add_child(node, port);
                printf("attached port:%u to node:%u\n", port->id, node->id);
            }
        }
    }
}

internal Void pipewire_port_param(Void *data, S32 seq, U32 id, U32 index, U32 next, const struct spa_pod *param) {
}



internal Void pipewire_device_info(Void *data, const struct pw_device_info *info) {
    Pipewire_Object *device = data;

    printf("device: id:%u\n", info->id);
    printf("  change mask:");
    if (info->change_mask & PW_DEVICE_CHANGE_MASK_PROPS)  printf(" PROPS");
    if (info->change_mask & PW_DEVICE_CHANGE_MASK_PARAMS) printf(" PARAMS");
    printf("\n");
    printf("  props:\n");
    const struct spa_dict_item *item = 0;
    spa_dict_for_each(item, info->props) {
        pipewire_object_update_property(device, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
        printf("    %s: \"%s\"\n", item->key, item->value);
    }
}

internal Void pipewire_device_param(Void *data, S32 seq, U32 id, U32 index, U32 next, const struct spa_pod *param) {
}



internal Void pipewire_link_info(Void *data, const struct pw_link_info *info) {
    Pipewire_Object *link = (Pipewire_Object *) data;

    printf("link: id:%u\n", info->id);
    printf("  output node id: %u\n", info->output_node_id);
    printf("  output port id: %u\n", info->output_port_id);
    printf("  input node id:  %u\n", info->input_node_id);
    printf("  input port id:  %u\n", info->input_port_id);
    printf("  change mask:");
    if (info->change_mask & PW_LINK_CHANGE_MASK_STATE)  printf(" STATE");
    if (info->change_mask & PW_LINK_CHANGE_MASK_FORMAT) printf(" FORMAT");
    if (info->change_mask & PW_LINK_CHANGE_MASK_PROPS)  printf(" PROPS");
    printf("\n");
    printf("  state:          %s\n", pw_link_state_as_string(info->state));
    printf("  props:\n");
    const struct spa_dict_item *item = 0;
    spa_dict_for_each(item, info->props) {
        pipewire_object_update_property(link, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
        printf("    %s: \"%s\"\n", item->key, item->value);
    }
}



internal Void pipewire_registry_global(Void *data, U32 id, U32 permissions, const char *type, U32 version, const struct spa_dict *props) {
    if (strcmp(type, PW_TYPE_INTERFACE_Module) == 0) {
        Pipewire_Object *object = pipewire_create_object(id);
        object->kind = Pipewire_Object_Module;
        object->proxy = pw_registry_bind(pipewire_state->registry, id, type, PW_VERSION_MODULE, 0);
        pw_module_add_listener((struct pw_module *) object->proxy, &object->listener, &module_events, object);
    } else if (strcmp(type, PW_TYPE_INTERFACE_Factory) == 0) {
        Pipewire_Object *object = pipewire_create_object(id);
        object->kind = Pipewire_Object_Factory;
        object->proxy = pw_registry_bind(pipewire_state->registry, id, type, PW_VERSION_FACTORY, 0);
        pw_factory_add_listener((struct pw_factory *) object->proxy, &object->listener, &factory_events, object);
    } else if (strcmp(type, PW_TYPE_INTERFACE_Client) == 0) {
        Pipewire_Object *object = pipewire_create_object(id);
        object->kind = Pipewire_Object_Client;
        object->proxy = pw_registry_bind(pipewire_state->registry, id, type, PW_VERSION_CLIENT, 0);
        pw_client_add_listener((struct pw_client *) object->proxy, &object->listener, &client_events, object);
    } else if (strcmp(type, PW_TYPE_INTERFACE_Device) == 0) {
        Pipewire_Object *object = pipewire_create_object(id);
        object->kind = Pipewire_Object_Device;
        object->proxy = pw_registry_bind(pipewire_state->registry, id, type, PW_VERSION_DEVICE, 0);
        pw_device_add_listener((struct pw_device *) object->proxy, &object->listener, &device_events, object);
    } else if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
        Pipewire_Object *object = pipewire_create_object(id);
        object->kind = Pipewire_Object_Node;
        object->proxy = pw_registry_bind(pipewire_state->registry, id, type, PW_VERSION_NODE, 0);
        pw_node_add_listener((struct pw_node *) object->proxy, &object->listener, &node_events, object);
    } else if (strcmp(type, PW_TYPE_INTERFACE_Port) == 0) {
        Pipewire_Object *object = pipewire_create_object(id);
        object->kind = Pipewire_Object_Port;
        object->proxy = pw_registry_bind(pipewire_state->registry, id, type, PW_VERSION_PORT, 0);
        pw_port_add_listener((struct pw_port *) object->proxy, &object->listener, &port_events, object);
    } else if (strcmp(type, PW_TYPE_INTERFACE_Link) == 0) {
        Pipewire_Object *object = pipewire_create_object(id);
        object->kind = Pipewire_Object_Link;
        object->proxy = pw_registry_bind(pipewire_state->registry, id, type, PW_VERSION_LINK, 0);
        pw_link_add_listener((struct pw_link *) object->proxy, &object->listener, &link_events, object);
    } else {
        printf("object: id:%u type:%s/%u\n", id, type, version);
        //for (U32 i = 0; i < props->n_items; ++i) {
            //const struct spa_dict_item *item = &props->items[i];
            //printf("  %s: %s\n", item->key, item->value);
        //}
    }
}

internal Void pipewire_registry_global_remove(Void *data, U32 id) {
    Pipewire_Object *object = pipewire_object_from_id(id);
    if (!pipewire_object_is_nil(object)) {
        printf("remove object id:%u\n", id);
        pipewire_destroy_object(object);
    }
}



internal Void pipewire_core_roundtrip_done(Void *data, U32 id, S32 seq) {
    Pipewire_Roundtrip *roundtrip = (Pipewire_Roundtrip *) data;
    if (id == PW_ID_CORE && seq == roundtrip->pending) {
        pw_main_loop_quit(roundtrip->loop);
    }
}



internal Void pipewire_roundtrip(struct pw_core *core, struct pw_main_loop *loop) {
    Pipewire_Roundtrip roundtrip = { 0 };
    struct spa_hook core_listener = { 0 };

    pw_core_add_listener(core, &core_listener, &pipewire_core_roundtrip_events, &roundtrip);

    roundtrip.loop    = loop;
    roundtrip.pending = pw_core_sync(core, PW_ID_CORE, 0);

    int error = pw_main_loop_run(loop);
    if (error < 0) {
        fprintf(stderr, "main_loop_run error: %d\n", error);
    }

    spa_hook_remove(&core_listener);
}



internal Void pipewire_init(Void) {
    Arena *arena = arena_create();
    pipewire_state = arena_push_struct(arena, Pipewire_State);
    pipewire_state->arena = arena;

    pw_init(0, 0);

    pipewire_state->loop     = pw_main_loop_new(0);
    pipewire_state->context  = pw_context_new(pw_main_loop_get_loop(pipewire_state->loop), 0, 0);
    pipewire_state->core     = pw_context_connect(pipewire_state->context, 0, 0);
    pipewire_state->registry = pw_core_get_registry(pipewire_state->core, PW_VERSION_REGISTRY, 0);
    
    pw_registry_add_listener(pipewire_state->registry, &pipewire_state->registry_listener, &registry_events, 0);

    // NOTE(simon): Register all globals.
    pipewire_roundtrip(pipewire_state->core, pipewire_state->loop);

    // NOTE(simon): Gather all events from the globals.
    pipewire_roundtrip(pipewire_state->core, pipewire_state->loop);

    pw_proxy_destroy((struct pw_proxy *) pipewire_state->registry);
    pw_core_disconnect(pipewire_state->core);
    pw_context_destroy(pipewire_state->context);
    pw_main_loop_destroy(pipewire_state->loop);

    pw_deinit();
}
