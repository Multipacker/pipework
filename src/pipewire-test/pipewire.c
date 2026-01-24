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

    if (object->info) {
        switch (object->kind) {
            case Pipewire_Object_Null: {
            } break;
            case Pipewire_Object_Module: {
                pw_module_info_free(object->info);
            } break;
            case Pipewire_Object_Factory: {
                pw_factory_info_free(object->info);
            } break;
            case Pipewire_Object_Client: {
                pw_client_info_free(object->info);
            } break;
            case Pipewire_Object_Device: {
                pw_device_info_free(object->info);
            } break;
            case Pipewire_Object_Node: {
                pw_node_info_free(object->info);
            } break;
            case Pipewire_Object_Port: {
                pw_port_info_free(object->info);
            } break;
            case Pipewire_Object_Link: {
                pw_link_info_free(object->info);
            } break;
            case Pipewire_Object_COUNT: {
            } break;
        }
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
    Pipewire_Property *existing_property = pipewire_property_from_object_name(object, name);

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

internal Pipewire_Property *pipewire_property_from_object_name(Pipewire_Object *object, Str8 name) {
    Pipewire_Property *result = &pipewire_nil_property;
    for (Pipewire_Property *property = object->first_property; property; property = property->next) {
        if (str8_equal(property->name, name)) {
            result = property;
            break;
        }
    }
    return result;
}

internal Str8 pipewire_property_string_from_object_name(Pipewire_Object *object, Str8 name) {
    Pipewire_Property *property = pipewire_property_from_object_name(object, name);
    return property->value;
}

internal U32 pipewire_property_u32_from_object_name(Pipewire_Object *object, Str8 name) {
    Pipewire_Property *property = pipewire_property_from_object_name(object, name);
    U64Decode decode = u64_from_str8(property->value);
    return (U32) decode.value;
}

internal Pipewire_Object *pipewire_property_object_from_object_name(Pipewire_Object *object, Str8 name) {
    Pipewire_Property *property = pipewire_property_from_object_name(object, name);

    U64Decode decode = { 0 };
    if (!pipewire_property_is_nil(property)) {
        decode = u64_from_str8(property->value);
    }

    Pipewire_Object *result = &pipewire_nil_object;
    if (decode.size != 0) {
        result = pipewire_object_from_id((U32) decode.value);
    }

    return result;
}



internal B32 pipewire_parameter_is_nil(Pipewire_Parameter *parameter) {
    B32 result = !parameter || parameter == &pipewire_nil_parameter;
    return result;
}

internal Void pipewire_object_remove_parameter(Pipewire_Object *object, U32 id) {
    for (Pipewire_Parameter *parameter = object->first_parameter, *next = 0; !pipewire_parameter_is_nil(parameter); parameter = next) {
        next = parameter->next;

        if (parameter->id == id) {
            dll_remove(object->first_parameter, object->last_parameter, parameter);
            pipewire_spa_pod_free(parameter->param);
            sll_stack_push(pipewire_state->parameter_freelist, parameter);
        }
    }
}

internal Void pipewire_object_update_parameter(Pipewire_Object *object, S32 sequence, U32 id, struct spa_pod *param) {
    B32 should_add = param != 0;
    for (U32 i = 0; i < object->param_count; ++i) {
        if (object->params[i].id == id) {
            should_add &= object->params[i].seq == sequence;
            break;
        }
    }

    if (should_add) {
        Pipewire_Parameter *parameter = pipewire_state->parameter_freelist;
        if (parameter) {
            sll_stack_pop(pipewire_state->parameter_freelist);
            memory_zero_struct(parameter);
        } else {
            parameter = arena_push_struct(pipewire_state->arena, Pipewire_Parameter);
        }

        parameter->id       = id;
        parameter->param    = pipewire_spa_pod_allocate(param);

        dll_push_back(object->first_parameter, object->last_parameter, parameter);
    }
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

internal Void pipewire_remove(Pipewire_Handle handle) {
    Pipewire_Object *object = pipewire_object_from_handle(handle);
    if (!pipewire_object_is_nil(object)) {
        pw_registry_destroy(pipewire_state->registry, object->id);
    }
}



internal B32 pipewire_object_is_card(Pipewire_Object *object) {
    Str8 media_class = pipewire_property_string_from_object_name(object, str8_literal(PW_KEY_MEDIA_CLASS));
    B32  result      = object->kind == Pipewire_Object_Device && str8_equal(media_class, str8_literal("Audio/Device"));
    return result;
}



internal Void pipewire_parse_volume(struct spa_pod *props, Pipewire_Volume *volume) {
    bool mute                  = volume->mute;
    U32  channel_volumes_count = volume->channel_count;
    U32  channel_map_count     = volume->channel_count;

    struct spa_pod_prop *property = 0;
    SPA_POD_OBJECT_FOREACH((struct spa_pod_object *) props, property) {
        switch (property->key) {
            case SPA_PROP_mute: {
                spa_pod_get_bool(&property->value, &mute) >= 0;
            } break;
            case SPA_PROP_channelVolumes: {
                channel_volumes_count = spa_pod_copy_array(&property->value, SPA_TYPE_Float, volume->channel_volumes, array_count(volume->channel_volumes));
            } break;
            case SPA_PROP_volumeBase: {
                spa_pod_get_float(&property->value, &volume->volume_base) >= 0;
            } break;
            case SPA_PROP_volumeStep: {
                spa_pod_get_float(&property->value, &volume->volume_step) >= 0;
            } break;
            case SPA_PROP_channelMap: {
                channel_map_count = spa_pod_copy_array(&property->value, SPA_TYPE_Id, volume->channel_map, array_count(volume->channel_map));
            } break;
            //SPA_PROP_monitorMute,    SPA_POD_OPT_Bool(&monitor_mute),
            //SPA_PROP_monitorVolumes, ,
            //SPA_PROP_softMute,       SPA_POD_OPT_Bool(&soft_mute),
            //SPA_PROP_softVolumes, ,
            //SPA_PROP_volumeRampSamples, ,
            //SPA_PROP_volumeRampStepSamples, ,
            //SPA_PROP_volumeRampTime, ,
            //SPA_PROP_volumeRampStepTime, ,
            //SPA_PROP_volumeRampScale, ,
        }
    }

    volume->mute          = mute;
    volume->channel_count = u32_min(channel_volumes_count, channel_map_count);
}

internal Pipewire_Volume pipewire_volume_from_node(Pipewire_Object *object) {
    Pipewire_Volume volume = { 0 };
    B32 has_volume = false;

    // NOTE(simon): Query for card.
    Pipewire_Object *card = pipewire_property_object_from_object_name(object, str8_literal(PW_KEY_DEVICE_ID));
    if (!pipewire_object_is_card(card)) {
        card = &pipewire_nil_object;
    }

    // NOTE(simon): Query for card device volume.
    Pipewire_Property *card_profile_device_property = pipewire_property_from_object_name(object, str8_literal("card.profile.device"));
    if (!pipewire_property_is_nil(card_profile_device_property)) {
        U32 device = (U32) u64_from_str8(card_profile_device_property->value).value;

        for (Pipewire_Parameter *parameter = card->first_parameter; !pipewire_parameter_is_nil(parameter); parameter = parameter->next) {
            if (parameter->id != SPA_PARAM_Route) {
                continue;
            }

            const struct spa_pod_prop *index_prop  = spa_pod_find_prop(parameter->param, 0, SPA_PARAM_ROUTE_index);
            const struct spa_pod_prop *device_prop = spa_pod_find_prop(parameter->param, 0, SPA_PARAM_ROUTE_device);
            const struct spa_pod_prop *props_prop  = spa_pod_find_prop(parameter->param, 0, SPA_PARAM_ROUTE_props);

            if (!index_prop || !device_prop) {
                continue;
            }

            S32 route_index  = 0;
            S32 route_device = 0;
            spa_pod_get_int(&index_prop->value,  &route_index);
            spa_pod_get_int(&device_prop->value, &route_device);

            if ((U32) route_device != device) {
                continue;
            }

            if (props_prop) {
                pipewire_parse_volume((struct spa_pod *) &props_prop->value, &volume);
                has_volume = true;
            }
        }
    }

    // NOTE(simon): Query the node for volume.
    if (!has_volume) {
        for (Pipewire_Parameter *parameter = object->first_parameter; !pipewire_parameter_is_nil(parameter); parameter = parameter->next) {
            if (parameter->id == SPA_PARAM_Props) {
                pipewire_parse_volume(parameter->param, &volume);
                has_volume = true;
            }
        }
    }

    return volume;
}

internal Void pipewire_set_node_volume(Pipewire_Object *object, Pipewire_Volume volume) {
    // NOTE(simon): Query for card.
    Pipewire_Object *card = pipewire_property_object_from_object_name(object, str8_literal(PW_KEY_DEVICE_ID));
    if (!pipewire_object_is_card(card)) {
        card = &pipewire_nil_object;
    }

    // NOTE(simon): Query for active card port.
    U32 device_id  = SPA_ID_INVALID;
    U32 port_index = SPA_ID_INVALID;
    Pipewire_Property *card_profile_device_property = pipewire_property_from_object_name(object, str8_literal("card.profile.device"));
    if (!pipewire_property_is_nil(card_profile_device_property)) {
        U32 device = (U32) u64_from_str8(card_profile_device_property->value).value;

        for (Pipewire_Parameter *parameter = card->first_parameter; !pipewire_parameter_is_nil(parameter); parameter = parameter->next) {
            if (parameter->id != SPA_PARAM_Route) {
                continue;
            }

            const struct spa_pod_prop *index_prop  = spa_pod_find_prop(parameter->param, 0, SPA_PARAM_ROUTE_index);
            const struct spa_pod_prop *device_prop = spa_pod_find_prop(parameter->param, 0, SPA_PARAM_ROUTE_device);

            if (!index_prop || !device_prop) {
                continue;
            }

            S32 route_index  = 0;
            S32 route_device = 0;
            spa_pod_get_int(&index_prop->value,  &route_index);
            spa_pod_get_int(&device_prop->value, &route_device);

            if ((U32) route_device == device) {
                device_id  = (U32) route_device;
                port_index = (U32) route_index;
                break;
            }
        }
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
    U8 buffer[1024];
    struct spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    if (device_id != SPA_ID_INVALID && port_index != SPA_ID_INVALID) {
        struct spa_pod_frame frame = { 0 };
        spa_pod_builder_push_object(
            &builder, &frame,
            SPA_TYPE_OBJECT_ParamRoute, SPA_PARAM_Route
        );
        spa_pod_builder_add(
            &builder,
            SPA_PARAM_ROUTE_index,  SPA_POD_Int(port_index),
            SPA_PARAM_ROUTE_device, SPA_POD_Int(device_id),
            SPA_PARAM_ROUTE_save,   SPA_POD_Bool(true),
            0
        );
        spa_pod_builder_prop(&builder, SPA_PARAM_ROUTE_props, 0);
        spa_pod_builder_add_object(
            &builder,
            SPA_TYPE_OBJECT_Props,   SPA_PARAM_Props,
            SPA_PROP_mute,           SPA_POD_Bool(volume.mute),
            SPA_PROP_channelVolumes, SPA_POD_Array(sizeof(F32), SPA_TYPE_Float, volume.channel_count, volume.channel_volumes)
        );
        struct spa_pod *pod = spa_pod_builder_pop(&builder, &frame);

        pw_device_set_param((struct pw_device *) card->proxy, SPA_PARAM_Route, 0, pod);
    } else {
        struct spa_pod *pod = spa_pod_builder_add_object(
            &builder,
            SPA_TYPE_OBJECT_Props, SPA_PARAM_Props,
            SPA_PROP_mute, SPA_POD_Bool(volume.mute),
            SPA_PROP_channelVolumes, SPA_POD_Array(sizeof(F32), SPA_TYPE_Float, volume.channel_count, volume.channel_volumes)
        );
        pw_node_set_param((struct pw_node *) object->proxy, SPA_PARAM_Props, 0, pod);
    }
#pragma clang diagnostic pop
}



internal Void pipewire_module_info(Void *data, const struct pw_module_info *info) {
    Pipewire_Object *module = (Pipewire_Object *) data;

    info = module->info = pw_module_info_update(module->info, info);

    if (info->change_mask & PW_MODULE_CHANGE_MASK_PROPS) {
        const struct spa_dict_item *item = 0;
        spa_dict_for_each(item, info->props) {
            pipewire_object_update_property(module, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
        }
    }
}



internal Void pipewire_factory_info(Void *data, const struct pw_factory_info *info) {
    Pipewire_Object *factory = (Pipewire_Object *) data;

    info = factory->info = pw_factory_info_update(factory->info, info);

    if (info->change_mask & PW_FACTORY_CHANGE_MASK_PROPS) {
        const struct spa_dict_item *item = 0;
        spa_dict_for_each(item, info->props) {
            pipewire_object_update_property(factory, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
        }
    }
}



internal Void pipewire_client_info(Void *data, const struct pw_client_info *info) {
    Pipewire_Object *client = (Pipewire_Object *) data;

    info = client->info = pw_client_info_update(client->info, info);

    if (info->change_mask & PW_CLIENT_CHANGE_MASK_PROPS) {
        const struct spa_dict_item *item = 0;
        spa_dict_for_each(item, info->props) {
            pipewire_object_update_property(client, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
        }
    }
}



internal Void pipewire_node_info(Void *data, const struct pw_node_info *info) {
    Pipewire_Object *node = (Pipewire_Object *) data;
    B32 changed = false;

    info = node->info = pw_node_info_merge(node->info, info, !node->changed);
    node->params = info->params;
    node->param_count = info->n_params;
    if (info->change_mask & PW_NODE_CHANGE_MASK_PARAMS) {
        for (U32 i = 0; i < info->n_params; ++i) {
            U32 id = info->params[i].id;

            // NOTE(simon): Set by pw_node_info_merge for new/updated
            // parameters. Only enumerate paramters that have changed.
            if (info->params[i].user == 0) {
                continue;
            }

            changed = true;

            // NOTE(simon): Clear all previous pending parameters for this ID.
            pipewire_object_remove_parameter(node, id);

            // NOTE(simon): No purpose in requesting parameters that we cannot read.
            if (info->params[i].flags & SPA_PARAM_INFO_READ) {
                info->params[i].seq = pw_node_enum_params((struct pw_node *) node->proxy, ++info->params[i].seq, id, 0, U32_MAX, 0);
            }
        }
    }

    if (info->change_mask & PW_NODE_CHANGE_MASK_PROPS) {
        changed = true;
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

    if (changed) {
        node->changed = true;
        pipewire_synchronize();
    }
}

internal Void pipewire_node_param(Void *data, S32 seq, U32 id, U32 index, U32 next, const struct spa_pod *param) {
    Pipewire_Object *node = (Pipewire_Object *) data;

    pipewire_object_update_parameter(node, seq, id, (struct spa_pod *) param);
}



internal Void pipewire_port_info(Void *data, const struct pw_port_info *info) {
    Pipewire_Object *port = (Pipewire_Object *) data;
    B32 changed = false;

    info = port->info = pw_port_info_merge(port->info, info, !port->changed);
    port->params = info->params;
    port->param_count = info->n_params;
    if (info->change_mask & PW_PORT_CHANGE_MASK_PARAMS) {
        for (U32 i = 0; i < info->n_params; ++i) {
            U32 id = info->params[i].id;

            // NOTE(simon): Set by pw_port_info_merge for new/updated
            // parameters. Only enumerate paramters that have changed.
            if (info->params[i].user == 0) {
                continue;
            }

            changed = true;

            // NOTE(simon): Clear all previous pending parameters for this ID.
            pipewire_object_remove_parameter(port, id);

            // NOTE(simon): No purpose in requesting parameters that we cannot read.
            if (info->params[i].flags & SPA_PARAM_INFO_READ) {
                info->params[i].seq = pw_port_enum_params((struct pw_port *) port->proxy, ++info->params[i].seq, id, 0, U32_MAX, 0);
            }

        }
    }

    if (info->change_mask & PW_PORT_CHANGE_MASK_PROPS) {
        changed = true;
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

    if (changed) {
        port->changed = true;
        pipewire_synchronize();
    }
}

internal Void pipewire_port_param(Void *data, S32 seq, U32 id, U32 index, U32 next, const struct spa_pod *param) {
    Pipewire_Object *port = (Pipewire_Object *) data;

    pipewire_object_update_parameter(port, seq, id, (struct spa_pod *) param);
}



internal Void pipewire_device_info(Void *data, const struct pw_device_info *info) {
    Pipewire_Object *device = data;
    B32 changed = false;

    info = device->info = pw_device_info_merge(device->info, info, !device->changed);
    device->params = info->params;
    device->param_count = info->n_params;
    if (info->change_mask & PW_DEVICE_CHANGE_MASK_PARAMS) {
        for (U32 i = 0; i < info->n_params; ++i) {
            U32 id = info->params[i].id;

            // NOTE(simon): Set by pw_device_info_merge for new/updated
            // parameters. Only enumerate paramters that have changed.
            if (info->params[i].user == 0) {
                continue;
            }

            changed = true;

            // NOTE(simon): Clear all previous pending parameters for this ID.
            pipewire_object_remove_parameter(device, id);

            // NOTE(simon): No purpose in requesting parameters that we cannot read.
            if (info->params[i].flags & SPA_PARAM_INFO_READ) {
                info->params[i].seq = pw_device_enum_params((struct pw_device *) device->proxy, ++info->params[i].seq, id, 0, U32_MAX, 0);
            }
        }
    }

    if (info->change_mask & PW_DEVICE_CHANGE_MASK_PROPS) {
        changed = true;
        const struct spa_dict_item *item = 0;
        spa_dict_for_each(item, info->props) {
            pipewire_object_update_property(device, str8_cstr((CStr) item->key), str8_cstr((CStr) item->value));
        }
    }

    if (changed) {
        device->changed = true;
        pipewire_synchronize();
    }
}

internal Void pipewire_device_param(Void *data, S32 seq, U32 id, U32 index, U32 next, const struct spa_pod *param) {
    Pipewire_Object *device = (Pipewire_Object *) data;

    pipewire_object_update_parameter(device, seq, id, (struct spa_pod *) param);
}



internal Void pipewire_link_info(Void *data, const struct pw_link_info *info) {
    Pipewire_Object *link = (Pipewire_Object *) data;

    info = link->info = pw_link_info_update(link->info, info);

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

    pipewire_synchronize();
}

internal Void pipewire_registry_global_remove(Void *data, U32 id) {
    Pipewire_Object *object = pipewire_object_from_id(id);
    if (!pipewire_object_is_nil(object)) {
        pipewire_destroy_object(object);
    }
}



internal Void pipewire_core_done(Void *data, U32 id, S32 seq) {
    if (id == PW_ID_CORE && seq == pipewire_state->core_sequence) {
        for (Pipewire_Object *object = pipewire_state->first_object; object; object = object->all_next) {
            object->changed = false;
        }

        pw_main_loop_quit(pipewire_state->loop);
    }
}



internal Void pipewire_synchronize(Void) {
    pipewire_state->core_sequence = pw_core_sync(pipewire_state->core, PW_ID_CORE, pipewire_state->core_sequence);
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
    
    pw_core_add_listener(pipewire_state->core, &pipewire_state->core_listener, &pipewire_core_roundtrip_events, 0);
    pw_registry_add_listener(pipewire_state->registry, &pipewire_state->registry_listener, &registry_events, 0);

    pipewire_synchronize();
    int error = pw_main_loop_run(pipewire_state->loop);
    if (error < 0) {
        fprintf(stderr, "main_loop_run error: %d\n", error);
    }
}

internal Void pipewire_tick(Void) {
    pipewire_synchronize();

    int error = pw_main_loop_run(pipewire_state->loop);
    if (error < 0) {
        fprintf(stderr, "main_loop_run error: %d\n", error);
    }
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
