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
        sll_stack_pop(pipewire_state->object_freelist);

        U64 generation = object->generation;
        memory_zero_struct(object);
        object->generation = generation;
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

    for (Pipewire_Parameter *parameter = object->first_parameter, *next = 0; parameter; parameter = next) {
        next = parameter->next;
        dll_remove(object->first_parameter, object->last_parameter, parameter);
        pipewire_spa_pod_free(parameter->param);
        sll_stack_push(pipewire_state->parameter_freelist, parameter);
    }

    spa_hook_remove(&object->listener);
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
    } else {
        Pipewire_Property *property = pipewire_state->property_freelist;
        if (property) {
            sll_stack_pop(pipewire_state->property_freelist);
            memory_zero_struct(property);
        } else {
            property = arena_push_struct(pipewire_state->arena, Pipewire_Property);
        }

        property->name = pipewire_string_allocate(name);
        dll_push_back(object->first_property, object->last_property, property);

        existing_property = property;
    }

    existing_property->value = pipewire_string_allocate(value);
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



internal B32 pipewire_parameter_is_nil(Pipewire_Parameter *parameter) {
    B32 result = !parameter || parameter == &pipewire_nil_parameter;
    return result;
}

internal Void pipewire_object_update_parameter(Pipewire_Object *object, U32 id, struct spa_pod *param) {
    Pipewire_Parameter *existing_parameter = pipewire_object_parameter_from_id(object, id);

    if (id == SPA_PARAM_Props) {
        printf("Props: %p\n", (Void *) object);
        Str8 key = { 0 };
        struct spa_pod_prop *prop = 0;
        struct spa_pod_object *obj = (struct spa_pod_object *) param;
        SPA_POD_OBJECT_FOREACH(obj, prop) {
            switch (prop->key) {
                case SPA_PROP_unknown: key = str8_literal("SPA_PROP_unknown"); break;
                case SPA_PROP_device: key = str8_literal("SPA_PROP_device"); break;
                case SPA_PROP_deviceName: key = str8_literal("SPA_PROP_deviceName"); break;
                case SPA_PROP_deviceFd: key = str8_literal("SPA_PROP_deviceFd"); break;
                case SPA_PROP_card: key = str8_literal("SPA_PROP_card"); break;
                case SPA_PROP_cardName: key = str8_literal("SPA_PROP_cardName"); break;
                case SPA_PROP_minLatency: key = str8_literal("SPA_PROP_minLatency"); break;
                case SPA_PROP_maxLatency: key = str8_literal("SPA_PROP_maxLatency"); break;
                case SPA_PROP_periods: key = str8_literal("SPA_PROP_periods"); break;
                case SPA_PROP_periodSize: key = str8_literal("SPA_PROP_periodSize"); break;
                case SPA_PROP_periodEvent: key = str8_literal("SPA_PROP_periodEvent"); break;
                case SPA_PROP_live: key = str8_literal("SPA_PROP_live"); break;
                case SPA_PROP_rate: key = str8_literal("SPA_PROP_rate"); break;
                case SPA_PROP_quality: key = str8_literal("SPA_PROP_quality"); break;
                case SPA_PROP_bluetoothAudioCodec: key = str8_literal("SPA_PROP_bluetoothAudioCodec"); break;
                case SPA_PROP_bluetoothOffloadActive: key = str8_literal("SPA_PROP_bluetoothOffloadActive"); break;
                case SPA_PROP_waveType: key = str8_literal("SPA_PROP_waveType"); break;
                case SPA_PROP_frequency: key = str8_literal("SPA_PROP_frequency"); break;
                case SPA_PROP_volume: key = str8_literal("SPA_PROP_volume"); break;
                case SPA_PROP_mute: key = str8_literal("SPA_PROP_mute"); break;
                case SPA_PROP_patternType: key = str8_literal("SPA_PROP_patternType"); break;
                case SPA_PROP_ditherType: key = str8_literal("SPA_PROP_ditherType"); break;
                case SPA_PROP_truncate: key = str8_literal("SPA_PROP_truncate"); break;
                case SPA_PROP_channelVolumes: key = str8_literal("SPA_PROP_channelVolumes"); break;
                case SPA_PROP_volumeBase: key = str8_literal("SPA_PROP_volumeBase"); break;
                case SPA_PROP_volumeStep: key = str8_literal("SPA_PROP_volumeStep"); break;
                case SPA_PROP_channelMap: key = str8_literal("SPA_PROP_channelMap"); break;
                case SPA_PROP_monitorMute: key = str8_literal("SPA_PROP_monitorMute"); break;
                case SPA_PROP_monitorVolumes: key = str8_literal("SPA_PROP_monitorVolumes"); break;
                case SPA_PROP_latencyOffsetNsec: key = str8_literal("SPA_PROP_latencyOffsetNsec"); break;
                case SPA_PROP_softMute: key = str8_literal("SPA_PROP_softMute"); break;
                case SPA_PROP_softVolumes: key = str8_literal("SPA_PROP_softVolumes"); break;
                case SPA_PROP_iec958Codecs: key = str8_literal("SPA_PROP_iec958Codecs"); break;
                case SPA_PROP_volumeRampSamples: key = str8_literal("SPA_PROP_volumeRampSamples"); break;
                case SPA_PROP_volumeRampStepSamples: key = str8_literal("SPA_PROP_volumeRampStepSamples"); break;
                case SPA_PROP_volumeRampTime: key = str8_literal("SPA_PROP_volumeRampTime"); break;
                case SPA_PROP_volumeRampStepTime: key = str8_literal("SPA_PROP_volumeRampStepTime"); break;
                case SPA_PROP_volumeRampScale: key = str8_literal("SPA_PROP_volumeRampScale"); break;
                case SPA_PROP_brightness: key = str8_literal("SPA_PROP_brightness"); break;
                case SPA_PROP_contrast: key = str8_literal("SPA_PROP_contrast"); break;
                case SPA_PROP_saturation: key = str8_literal("SPA_PROP_saturation"); break;
                case SPA_PROP_hue: key = str8_literal("SPA_PROP_hue"); break;
                case SPA_PROP_gamma: key = str8_literal("SPA_PROP_gamma"); break;
                case SPA_PROP_exposure: key = str8_literal("SPA_PROP_exposure"); break;
                case SPA_PROP_gain: key = str8_literal("SPA_PROP_gain"); break;
                case SPA_PROP_sharpness: key = str8_literal("SPA_PROP_sharpness"); break;
                case SPA_PROP_params: key = str8_literal("SPA_PROP_params"); break;
                default: { } break;
            }
            printf("\t%.*s\n", str8_expand(key));
        }
    }

    if (!pipewire_parameter_is_nil(existing_parameter)) {
        pipewire_spa_pod_free(existing_parameter->param);
    } else {
        Pipewire_Parameter *parameter = pipewire_state->parameter_freelist;
        if (parameter) {
            sll_stack_pop(pipewire_state->parameter_freelist);
            memory_zero_struct(parameter);
        } else {
            parameter = arena_push_struct(pipewire_state->arena, Pipewire_Parameter);
        }

        parameter->id = id;
        dll_push_back(object->first_parameter, object->last_parameter, parameter);

        existing_parameter = parameter;
    }

    existing_parameter->param = pipewire_spa_pod_allocate(param);
}

internal Pipewire_Parameter *pipewire_object_parameter_from_id(Pipewire_Object *object, U32 id) {
    Pipewire_Parameter *result = &pipewire_nil_parameter;
    for (Pipewire_Parameter *parameter = object->first_parameter; parameter; parameter = parameter->next) {
        if (parameter->id == id) {
            result = parameter;
            break;
        }
    }
    return result;
}



internal U64 pipewire_chunk_index_from_size(U64 size) {
    U64 chunk_index = 0;
    if (size) {
        for (U64 i = 0; i < array_count(pipewire_chunk_sizes); ++i) {
            if (size <= pipewire_chunk_sizes[i]) {
                chunk_index = 1 + i;
                break;
            }
        }
    }
    return chunk_index;
}

internal U8 *pipewire_allocate(U64 size) {
    U64 chunk_index = pipewire_chunk_index_from_size(size);
    Pipewire_ChunkNode *chunk = 0;
    if (chunk_index == array_count(pipewire_chunk_sizes)) {
        Pipewire_ChunkNode *best_chunk_previous = 0;
        Pipewire_ChunkNode *best_chunk = 0;
        for (Pipewire_ChunkNode *candidate = pipewire_state->chunk_freelist[chunk_index - 1], *previous = 0; candidate; previous = candidate, candidate = candidate->next) {
            if (size <= candidate->size && (!best_chunk || candidate->size < best_chunk->size)) {
                best_chunk_previous = previous;
                best_chunk = candidate;
            }
        }

        if (best_chunk) {
            chunk = best_chunk;
            if (best_chunk_previous) {
                best_chunk_previous->next = best_chunk->next;
            } else {
                pipewire_state->chunk_freelist[chunk_index - 1] = best_chunk->next;
            }
        } else {
            U64 ceiled_size = u64_ceil_to_power_of_2(size);
            chunk = (Pipewire_ChunkNode *) arena_push_array_no_zero(pipewire_state->arena, U8, ceiled_size);
        }
    } else if (chunk_index != 0) {
        chunk = pipewire_state->chunk_freelist[chunk_index - 1];
        if (chunk) {
            sll_stack_pop(pipewire_state->chunk_freelist[chunk_index - 1]);
        } else {
            chunk = (Pipewire_ChunkNode *) arena_push_array_no_zero(pipewire_state->arena, U8, pipewire_chunk_sizes[chunk_index - 1]);
        }
    }

    U8 *result = (U8 *) chunk;
    return result;
}

internal Void pipewire_free(U8 *data, U64 size) {
    U64 chunk_index = pipewire_chunk_index_from_size(size);
    if (chunk_index) {
        Pipewire_ChunkNode *chunk = (Pipewire_ChunkNode *) data;
        chunk->size = u64_ceil_to_power_of_2(size);
        sll_stack_push(pipewire_state->chunk_freelist[chunk_index - 1], chunk);
    }
}

internal Str8 pipewire_string_allocate(Str8 string) {
    U8 *buffer = pipewire_allocate(string.size);

    Str8 result = { 0 };
    if (buffer) {
        result.data = buffer;
        result.size = string.size;
        memory_copy(result.data, string.data, string.size);
    }

    return result;
}

internal Void pipewire_string_free(Str8 string) {
    pipewire_free(string.data, string.size);
}

internal struct spa_pod *pipewire_spa_pod_allocate(struct spa_pod *pod) {
    U64 size = SPA_POD_SIZE(pod);
    U8 *buffer = pipewire_allocate(size);

    struct spa_pod *result = 0;
    if (buffer) {
        result = (struct spa_pod *) buffer;
        memory_copy(result, pod, size);
    }
    return result;
}

internal Void pipewire_spa_pod_free(struct spa_pod *pod) {
    pipewire_free((U8 *) pod, SPA_POD_SIZE(pod));
}



internal Pipewire_Handle pipewire_handle_from_object(Pipewire_Object *object) {
    Pipewire_Handle handle = { 0 };
    if (!pipewire_object_is_nil(object)) {
        handle.u64[0] = integer_from_pointer(object);
        handle.u64[1] = object->generation;
    }
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



internal Void pipewire_link(Pipewire_Handle output_handle, Pipewire_Handle input_handle) {
    Pipewire_Object *output = pipewire_object_from_handle(output_handle);
    Pipewire_Object *input  = pipewire_object_from_handle(input_handle);

    B32 good = true;

    struct pw_properties *properties = pw_properties_new(
        PW_KEY_OBJECT_LINGER, "true",
        NULL
    );

    // NOTE(simon): Fill output object.
    if (output->kind == Pipewire_Object_Port) {
        pw_properties_setf(properties, PW_KEY_LINK_OUTPUT_PORT, "%u", output->id);
    } else if (output->kind == Pipewire_Object_Node) {
        pw_properties_setf(properties, PW_KEY_LINK_OUTPUT_NODE, "%u", output->id);
    } else {
        good = false;
    }

    // NOTE(simon): Fill input object.
    if (input->kind == Pipewire_Object_Port) {
        pw_properties_setf(properties, PW_KEY_LINK_INPUT_PORT, "%u", input->id);
    } else if (input->kind == Pipewire_Object_Node) {
        pw_properties_setf(properties, PW_KEY_LINK_INPUT_NODE, "%u", input->id);
    } else {
        good = false;
    }

    // NOTE(simon): Create the link if we could fill out all required
    // properties.
    if (good) {
        pw_core_create_object(
            pipewire_state->core,
            "link-factory",
            PW_TYPE_INTERFACE_Link,
            PW_VERSION_LINK,
            &properties->dict, 0
        );
    }

    pw_properties_free(properties);
}



internal Void pipewire_module_info(Void *data, const struct pw_module_info *info) {
    Pipewire_Object *module = (Pipewire_Object *) data;

    module->module_info = pw_module_info_update(module->module_info, info);

    if (info->change_mask & PW_MODULE_CHANGE_MASK_PROPS) {
        const struct spa_dict_item *item = 0;
        spa_dict_for_each(item, info->props) {
            pipewire_object_update_property(module, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
        }
    }
}



internal Void pipewire_factory_info(Void *data, const struct pw_factory_info *info) {
    Pipewire_Object *factory = (Pipewire_Object *) data;

    factory->factory_info = pw_factory_info_update(factory->factory_info, info);

    if (info->change_mask & PW_FACTORY_CHANGE_MASK_PROPS) {
        const struct spa_dict_item *item = 0;
        spa_dict_for_each(item, info->props) {
            pipewire_object_update_property(factory, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
        }
    }
}



internal Void pipewire_client_info(Void *data, const struct pw_client_info *info) {
    Pipewire_Object *client = (Pipewire_Object *) data;

    client->client_info = pw_client_info_update(client->client_info, info);

    if (info->change_mask & PW_CLIENT_CHANGE_MASK_PROPS) {
        const struct spa_dict_item *item = 0;
        spa_dict_for_each(item, info->props) {
            pipewire_object_update_property(client, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
        }
    }
}



internal Void pipewire_node_info(Void *data, const struct pw_node_info *info) {
    Pipewire_Object *node = (Pipewire_Object *) data;

    node->node_info = pw_node_info_update(node->node_info, info);
    if (node->node_info->change_mask & PW_NODE_CHANGE_MASK_PARAMS) {
        for (U32 i = 0; i < node->node_info->n_params; ++i) {
            // NOTE(simon): Only enumerate paramters that have changed.
            if (node->node_info->params[i].user == 0) {
                continue;
            }
            node->node_info->params[i].user = 0;

            // NOTE(simon): No purpose in requesting parameters that we cannot read.
            if (!(node->node_info->params[i].flags & SPA_PARAM_INFO_READ)) {
                continue;
            }

            // TODO(simon): Keep track of sequence numbers to know if we have the most up to date information.
            pw_node_enum_params((struct pw_node *) node->proxy, ++node->node_info->params[i].seq, node->node_info->params[i].id, 0, U32_MAX, 0);
        }
    }

    if (info->change_mask & PW_NODE_CHANGE_MASK_PROPS) {
        const struct spa_dict_item *item = 0;
        spa_dict_for_each(item, info->props) {
            pipewire_object_update_property(node, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
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
                }
            }
        } else if (client_id_item) {
            U32 client_id = 0;
            if (spa_atou32(client_id_item->value, &client_id, 10)) {
                Pipewire_Object *client = pipewire_object_from_id(client_id);
                if (!pipewire_object_is_nil(client)) {
                    pipewire_add_child(client, node);
                }
            }
        }
    }
}

internal Void pipewire_node_param(Void *data, S32 seq, U32 id, U32 index, U32 next, const struct spa_pod *param) {
    Pipewire_Object *node = (Pipewire_Object *) data;

    Str8 parameter_type = { 0 };
    switch (id) {
        case SPA_PARAM_Invalid:        parameter_type = str8_literal("Invalid");        break;
        case SPA_PARAM_PropInfo:       parameter_type = str8_literal("PropInfo");       break;
        case SPA_PARAM_Props:          parameter_type = str8_literal("Props");          break;
        case SPA_PARAM_EnumFormat:     parameter_type = str8_literal("EnumFormat");     break;
        case SPA_PARAM_Format:         parameter_type = str8_literal("Format");         break;
        case SPA_PARAM_Buffers:        parameter_type = str8_literal("Buffers");        break;
        case SPA_PARAM_Meta:           parameter_type = str8_literal("Meta");           break;
        case SPA_PARAM_IO:             parameter_type = str8_literal("IO");             break;
        case SPA_PARAM_EnumProfile:    parameter_type = str8_literal("EnumProfile");    break;
        case SPA_PARAM_Profile:        parameter_type = str8_literal("Profile");        break;
        case SPA_PARAM_EnumPortConfig: parameter_type = str8_literal("EnumPortConfig"); break;
        case SPA_PARAM_PortConfig:     parameter_type = str8_literal("PortConfig");     break;
        case SPA_PARAM_EnumRoute:      parameter_type = str8_literal("EnumRoute");      break;
        case SPA_PARAM_Route:          parameter_type = str8_literal("Route");          break;
        case SPA_PARAM_Control:        parameter_type = str8_literal("Control");        break;
        case SPA_PARAM_Latency:        parameter_type = str8_literal("Latency");        break;
        case SPA_PARAM_ProcessLatency: parameter_type = str8_literal("ProcessLatency"); break;
        case SPA_PARAM_Tag:            parameter_type = str8_literal("Tag");            break;
    }
    //printf("%p, %u, %.*s\n", data, seq, str8_expand(parameter_type));

    pipewire_object_update_parameter(node, id, (struct spa_pod *) param);
}



internal Void pipewire_port_info(Void *data, const struct pw_port_info *info) {
    Pipewire_Object *port = (Pipewire_Object *) data;

    port->port_info = pw_port_info_update(port->port_info, info);
    if (info->change_mask & PW_PORT_CHANGE_MASK_PARAMS) {
        for (U32 i = 0; i < info->n_params; ++i) {
            // NOTE(simon): Only enumerate paramters that have changed.
            if (info->params[i].user == 0) {
                continue;
            }
            info->params[i].user = 0;

            // NOTE(simon): No purpose in requesting parameters that we cannot read.
            if (!(info->params[i].flags & SPA_PARAM_INFO_READ)) {
                continue;
            }

            // TODO(simon): Keep track of sequence numbers to know if we have the most up to date information.
            pw_port_enum_params((struct pw_port *) port->proxy, 0, info->params[i].id, 0, U32_MAX, 0);
        }
    }

    if (info->change_mask & PW_PORT_CHANGE_MASK_PROPS) {
        const struct spa_dict_item *item = 0;
        spa_dict_for_each(item, info->props) {
            pipewire_object_update_property(port, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
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
                }
            }
        }
    }
}

internal Void pipewire_port_param(Void *data, S32 seq, U32 id, U32 index, U32 next, const struct spa_pod *param) {
    Pipewire_Object *port = (Pipewire_Object *) data;

    pipewire_object_update_parameter(port, id, (struct spa_pod *) param);
}



internal Void pipewire_device_info(Void *data, const struct pw_device_info *info) {
    Pipewire_Object *device = data;

    device->device_info = pw_device_info_update(device->device_info, info);
    if (info->change_mask & PW_DEVICE_CHANGE_MASK_PARAMS) {
        for (U32 i = 0; i < info->n_params; ++i) {
            // NOTE(simon): Only enumerate paramters that have changed.
            if (info->params[i].user == 0) {
                continue;
            }
            info->params[i].user = 0;

            // NOTE(simon): No purpose in requesting parameters that we cannot read.
            if (!(info->params[i].flags & SPA_PARAM_INFO_READ)) {
                continue;
            }

            // TODO(simon): Keep track of sequence numbers to know if we have the most up to date information.
            pw_device_enum_params((struct pw_device *) device->proxy, 0, info->params[i].id, 0, U32_MAX, 0);
        }
    }

    const struct spa_dict_item *item = 0;
    spa_dict_for_each(item, info->props) {
        pipewire_object_update_property(device, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
    }
}

internal Void pipewire_device_param(Void *data, S32 seq, U32 id, U32 index, U32 next, const struct spa_pod *param) {
    Pipewire_Object *device = (Pipewire_Object *) data;

    pipewire_object_update_parameter(device, id, (struct spa_pod *) param);
}



internal Void pipewire_link_info(Void *data, const struct pw_link_info *info) {
    Pipewire_Object *link = (Pipewire_Object *) data;

    link->link_info = pw_link_info_update(link->link_info, info);

    if (info->change_mask & PW_LINK_CHANGE_MASK_PROPS) {
        const struct spa_dict_item *item = 0;
        spa_dict_for_each(item, info->props) {
            pipewire_object_update_property(link, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
        }
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
}

internal Void pipewire_tick(Void) {
    pipewire_roundtrip(pipewire_state->core, pipewire_state->loop);
}

internal Void pipewire_deinit(Void) {
    pw_proxy_destroy((struct pw_proxy *) pipewire_state->registry);
    pw_core_disconnect(pipewire_state->core);
    pw_context_destroy(pipewire_state->context);
    pw_main_loop_destroy(pipewire_state->loop);

    pw_deinit();
}

internal U64 pipewire_spa_pod_min_type_size(U32 type) {
    U64 size = 0;
    switch (type) {
        case SPA_TYPE_None: {
            size = 0;
        } break;
        case SPA_TYPE_Bool: {
            size = sizeof(S32);
        } break;
        case SPA_TYPE_Id: {
            size = sizeof(U32);
        } break;
        case SPA_TYPE_Int: {
            size = sizeof(S32);
        } break;
        case SPA_TYPE_Long: {
            size = sizeof(S64);
        } break;
        case SPA_TYPE_Float: {
            size = sizeof(F32);
        } break;
        case SPA_TYPE_Double: {
            size = sizeof(F64);
        } break;
        case SPA_TYPE_String: {
            size = sizeof(U8);
        } break;
        case SPA_TYPE_Bytes: {
            size = 0;
        } break;
        case SPA_TYPE_Rectangle: {
            size = sizeof(struct spa_rectangle);
        } break;
        case SPA_TYPE_Fraction: {
            size = sizeof(struct spa_fraction);
        } break;
        case SPA_TYPE_Bitmap: {
            size = sizeof(U8);
        } break;
        case SPA_TYPE_Array: {
            size = sizeof(struct spa_pod_array_body);
        } break;
        case SPA_TYPE_Struct: {
            size = 0;
        } break;
        case SPA_TYPE_Object: {
            size = sizeof(struct spa_pod_object_body);
        } break;
        case SPA_TYPE_Sequence: {
            size = sizeof(struct spa_pod_choice_body);
        } break;
        case SPA_TYPE_Pointer: {
            size = sizeof(struct spa_pod_pointer_body);
        } break;
        case SPA_TYPE_Fd: {
            size = sizeof(S64);
        } break;
        case SPA_TYPE_Choice: {
            size = sizeof(struct spa_pod_choice_body);
        } break;
        case SPA_TYPE_Pod: {
            size = 0;
        } break;
    }

    return size;
}
