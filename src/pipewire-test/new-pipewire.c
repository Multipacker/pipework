internal Str8 pipewire_string_from_object_kind(Pipewire_ObjectKind kind) {
    Str8 result = str8_literal("Unknown");

    switch (kind) {
        case Pipewire_ObjectKind_Null: {
            result = str8_literal("Null");
        } break;
        case Pipewire_ObjectKind_Client: {
            result = str8_literal("Client");
        } break;
        case Pipewire_ObjectKind_Device: {
            result = str8_literal("Device");
        } break;
        case Pipewire_ObjectKind_Factory: {
            result = str8_literal("Factory");
        } break;
        case Pipewire_ObjectKind_Link: {
            result = str8_literal("Link");
        } break;
        case Pipewire_ObjectKind_Metadata: {
            result = str8_literal("Metadata");
        } break;
        case Pipewire_ObjectKind_Module: {
            result = str8_literal("Module");
        } break;
        case Pipewire_ObjectKind_Node: {
            result = str8_literal("Node");
        } break;
        case Pipewire_ObjectKind_Port: {
            result = str8_literal("Port");
        } break;
        case Pipewire_ObjectKind_COUNT: {
        } break;
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

internal Void pipewire_parse_volume(struct spa_pod *props, Pipewire_Volume *volume) {
    bool mute                  = volume->mute;
    U32  channel_volumes_count = volume->channel_count;
    U32  channel_map_count     = volume->channel_count;

    struct spa_pod_prop *property = 0;
    SPA_POD_OBJECT_FOREACH((struct spa_pod_object *) props, property) {
        switch (property->key) {
            case SPA_PROP_mute: {
                spa_pod_get_bool(&property->value, &mute);
            } break;
            case SPA_PROP_channelVolumes: {
                channel_volumes_count = spa_pod_copy_array(&property->value, SPA_TYPE_Float, volume->channel_volumes, array_count(volume->channel_volumes));
            } break;
            case SPA_PROP_volumeBase: {
                spa_pod_get_float(&property->value, &volume->volume_base);
            } break;
            case SPA_PROP_volumeStep: {
                spa_pod_get_float(&property->value, &volume->volume_step);
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



// NOTE(simon): Events.

internal Pipewire_Event *pipewire_event_list_push(Arena *arena, Pipewire_EventList *list) {
    Pipewire_Event *event = arena_push_struct(arena, Pipewire_Event);
    sll_queue_push(list->first, list->last, event);
    ++list->count;
    return event;
}

internal Pipewire_Event *pipewire_event_list_push_properties(Arena *arena, Pipewire_EventList *list, U32 id, struct spa_dict *properties) {
    Pipewire_Event *event = pipewire_event_list_push(arena, list);
    event->kind           = Pipewire_EventKind_UpdateProperties;
    event->id             = id;
    event->property_count = properties->n_items;
    event->properties     = arena_push_array(arena, Pipewire_Property, event->property_count);
    for (U64 i = 0; i < properties->n_items; ++i) {
        event->properties[i].key   = str8_copy_cstr(arena, (U8 *) properties->items[i].key);
        event->properties[i].value = str8_copy_cstr(arena, (U8 *) properties->items[i].value);
    }
    return event;
}

internal Void str8_serial_push_data(Arena *arena, Str8List *list, Void *ptr, U64 size) {
    if (list->last && list->last->string.data + list->last->string.size == arena->memory + arena->position) {
        U8 *buffer = arena_push_array_no_zero(arena, U8, size);
        memory_copy(buffer, ptr, size);
        list->last->string.size += size;
    } else {
        Str8Node *node = arena_push_struct(arena, Str8Node);
        dll_push_back(list->first, list->last, node);
        ++list->node_count;
        list->last->string = str8_copy(arena, str8(ptr, size));
    }

    list->total_size += size;
}

internal U64 str8_deserial_read_data(Str8 string, U64 offset, Void *ptr, U64 size) {
    U64 bytes_left = string.size - u64_min(offset, string.size);
    U64 to_read = u64_min(size, bytes_left);
    memory_copy(ptr, string.data + offset, to_read);
    return to_read;
}

#define str8_serial_push_type(arena, list, ptr) str8_serial_push_data(arena, list, ptr, sizeof(*ptr))

#define str8_deserial_read_type(string, offset, ptr) str8_deserial_read_data(string, offset, ptr, sizeof(*ptr))

internal Str8 pipewire_serialized_string_from_event_list(Arena *arena, Pipewire_EventList events) {
    Arena_Temporary scratch = arena_get_scratch(&arena, 1);
    Str8List serialized = { 0 };

    str8_serial_push_type(scratch.arena, &serialized, &events.count);
    for (Pipewire_Event *event = events.first; event; event = event->next) {
        str8_serial_push_type(scratch.arena, &serialized, &event->kind);
        str8_serial_push_type(scratch.arena, &serialized, &event->id);
        str8_serial_push_type(scratch.arena, &serialized, &event->object_kind);
        str8_serial_push_type(scratch.arena, &serialized, &event->handle);

        // NOTE(simon): Write property list.
        str8_serial_push_type(scratch.arena, &serialized, &event->property_count);
        for (U64 i = 0; i < event->property_count; ++i) {
            Pipewire_Property *property = &event->properties[i];
            str8_serial_push_type(scratch.arena, &serialized, &property->key.size);
            str8_serial_push_data(scratch.arena, &serialized, property->key.data, property->key.size);
            str8_serial_push_type(scratch.arena, &serialized, &property->value.size);
            str8_serial_push_data(scratch.arena, &serialized, property->value.data, property->value.size);
        }

        // NOTE(simon): Write parameter list.
        str8_serial_push_type(scratch.arena, &serialized, &event->parameter_id);
        str8_serial_push_type(scratch.arena, &serialized, &event->parameter_flags);
        str8_serial_push_type(scratch.arena, &serialized, &event->parameter_sequence);
        str8_serial_push_type(scratch.arena, &serialized, &event->parameter_count);
        for (Pipewire_ParameterNode *node = event->first_parameter; node; node = node->next) {
            struct spa_pod *parameter = node->parameter;
            U64 size = SPA_POD_SIZE(parameter);
            str8_serial_push_type(scratch.arena, &serialized, &size);
            str8_serial_push_data(scratch.arena, &serialized, parameter, size);
        }

        // NOTE(simon): Write metadata.
        str8_serial_push_type(scratch.arena, &serialized, &event->metadata_issuer);
        str8_serial_push_type(scratch.arena, &serialized, &event->metadata_key.size);
        str8_serial_push_data(scratch.arena, &serialized, event->metadata_key.data, event->metadata_key.size);
        str8_serial_push_type(scratch.arena, &serialized, &event->metadata_type.size);
        str8_serial_push_data(scratch.arena, &serialized, event->metadata_type.data, event->metadata_type.size);
        str8_serial_push_type(scratch.arena, &serialized, &event->metadata_value.size);
        str8_serial_push_data(scratch.arena, &serialized, event->metadata_value.data, event->metadata_value.size);
    }

    Str8 result = str8_join(arena, serialized);
    arena_end_temporary(scratch);
    return result;
}

internal Pipewire_EventList pipewire_event_list_from_serialized_string(Arena *arena, Str8 string) {
    Pipewire_EventList events = { 0 };

    U64 read_offset = 0;
    read_offset += str8_deserial_read_type(string, read_offset, &events.count);
    for (U64 i = 0; i < events.count; ++i) {
        Pipewire_Event *event = arena_push_struct(arena, Pipewire_Event);
        sll_queue_push(events.first, events.last, event);

        read_offset += str8_deserial_read_type(string, read_offset, &event->kind);
        read_offset += str8_deserial_read_type(string, read_offset, &event->id);
        read_offset += str8_deserial_read_type(string, read_offset, &event->object_kind);
        read_offset += str8_deserial_read_type(string, read_offset, &event->handle);

        // NOTE(simon): Read property list.
        read_offset += str8_deserial_read_type(string, read_offset, &event->property_count);
        event->properties = arena_push_array_no_zero(arena, Pipewire_Property, event->property_count);
        for (U64 j = 0; j < event->property_count; ++j) {
            Pipewire_Property *property = &event->properties[j];
            read_offset += str8_deserial_read_type(string, read_offset, &property->key.size);
            property->key.data = arena_push_array_no_zero(arena, U8, property->key.size);
            read_offset += str8_deserial_read_data(string, read_offset, property->key.data, property->key.size);
            read_offset += str8_deserial_read_type(string, read_offset, &property->value.size);
            property->value.data = arena_push_array_no_zero(arena, U8, property->value.size);
            read_offset += str8_deserial_read_data(string, read_offset, property->value.data, property->value.size);
        }

        // NOTE(simon): Read parameter list.
        read_offset += str8_deserial_read_type(string, read_offset, &event->parameter_id);
        read_offset += str8_deserial_read_type(string, read_offset, &event->parameter_flags);
        read_offset += str8_deserial_read_type(string, read_offset, &event->parameter_sequence);
        read_offset += str8_deserial_read_type(string, read_offset, &event->parameter_count);
        for (U64 j = 0; j < event->parameter_count; ++j) {
            Pipewire_ParameterNode *node = arena_push_struct(arena, Pipewire_ParameterNode);
            sll_queue_push(event->first_parameter, event->last_parameter, node);
            U64 size = 0;
            read_offset += str8_deserial_read_type(string, read_offset, &size);
            node->parameter = arena_push_no_zero(arena, size, 16);
            read_offset += str8_deserial_read_data(string, read_offset, node->parameter, size);
        }

        // NOTE(simon): Read metadata.
        read_offset += str8_deserial_read_type(string, read_offset, &event->metadata_issuer);
        read_offset += str8_deserial_read_type(string, read_offset, &event->metadata_key.size);
        event->metadata_key.data = arena_push_array_no_zero(arena, U8, event->metadata_key.size);
        read_offset += str8_deserial_read_data(string, read_offset, event->metadata_key.data, event->metadata_key.size);
        read_offset += str8_deserial_read_type(string, read_offset, &event->metadata_type.size);
        event->metadata_type.data = arena_push_array_no_zero(arena, U8, event->metadata_type.size);
        read_offset += str8_deserial_read_data(string, read_offset, event->metadata_type.data, event->metadata_type.size);
        read_offset += str8_deserial_read_type(string, read_offset, &event->metadata_value.size);
        event->metadata_value.data = arena_push_array_no_zero(arena, U8, event->metadata_value.size);
        read_offset += str8_deserial_read_data(string, read_offset, event->metadata_value.data, event->metadata_value.size);
    }

    return events;
}

internal Void pipewire_apply_events(Pipewire_EventList events) {
    for (Pipewire_Event *event = events.first; event; event = event->next) {
        switch (event->kind) {
            case Pipewire_EventKind_Create: {
                // NOTE(simon): Allocate.
                Pipewire_Object *object = pipewire_state->object_freelist;
                U64 generation = 0;
                if (object) {
                    generation = object->generation + 1;
                    sll_stack_pop(pipewire_state->object_freelist);
                    memory_zero_struct(object);
                } else {
                    object = arena_push_struct(pipewire_state->object_arena, Pipewire_Object);
                }

                // NOTE(simon): Fill.
                object->id         = event->id;
                object->generation = generation;
                object->kind       = event->object_kind;
                object->entity     = event->handle;

                // NOTE(simon): Insert into id -> object map.
                U64 hash = u64_hash(object->id);
                Pipewire_ObjectList *object_bucket = &pipewire_state->object_map[hash & (pipewire_state->object_map_capacity - 1)];
                dll_push_back(object_bucket->first, object_bucket->last, object);
            } break;
            case Pipewire_EventKind_UpdateProperties: {
                Pipewire_Object *object = pipewire_object_from_id(event->id);

                if (!pipewire_object_is_nil(object)) {
                    // NOTE(simon): Free old properties.
                    for (U64 i = 0; i < object->property_count; ++i) {
                        pipewire_free_string(object->properties[i].key);
                        pipewire_free_string(object->properties[i].value);
                    }
                    pipewire_free(object->properties, object->property_count * sizeof(Pipewire_Property));

                    // NOTE(simon): Allocate new properties.
                    object->property_count = event->property_count;
                    object->properties = pipewire_allocate(object->property_count * sizeof(Pipewire_Property));
                    for (U64 i = 0; i < object->property_count; ++i) {
                        object->properties[i].key = pipewire_allocate_string(event->properties[i].key);
                        object->properties[i].value = pipewire_allocate_string(event->properties[i].value);
                    }
                }
            } break;
            case Pipewire_EventKind_UpdateParameter: {
                Pipewire_Object *object = pipewire_object_from_id(event->id);

                // NOTE(simon): Find old paramter.
                Pipewire_Parameter *parameter = 0;
                for (Pipewire_Parameter *candidate = object->first_parameter; candidate; candidate = candidate->next) {
                    if (candidate->id == event->parameter_id) {
                        parameter = candidate;
                        break;
                    }
                }

                // NOTE(simon): Free the old parameter.
                if (parameter) {
                    for (U64 i = 0; i < parameter->count; ++i) {
                        pipewire_free(parameter->parameters[i], SPA_POD_SIZE(parameter->parameters[i]));
                    }
                    pipewire_free(parameter->parameters, parameter->count * sizeof(*parameter->parameters));
                }

                // NOTE(simon): Fill the new data.
                if (!pipewire_object_is_nil(object)) {
                    // NOTE(simon): Allocate.
                    if (!parameter) {
                        parameter = pipewire_state->parameter_freelist;
                        if (parameter) {
                            sll_stack_pop(pipewire_state->parameter_freelist);
                            memory_zero_struct(parameter);
                        } else {
                            parameter = arena_push_struct(pipewire_state->object_arena, Pipewire_Parameter);
                        }

                        // NOTE(simon): Insert.
                        dll_push_back(object->first_parameter, object->last_parameter, parameter);
                    }

                    // NOTE(simon): Fill.
                    parameter->id    = event->parameter_id;
                    parameter->flags = event->parameter_flags;
                    parameter->count = 0;
                    parameter->parameters = pipewire_allocate(event->parameter_count * sizeof(*parameter->parameters));
                    for (Pipewire_ParameterNode *node = event->first_parameter; node; node = node->next, ++parameter->count) {
                        parameter->parameters[parameter->count] = pipewire_allocate(SPA_POD_SIZE(node->parameter));
                        memory_copy(parameter->parameters[parameter->count], node->parameter, SPA_POD_SIZE(node->parameter));
                    }
                }
            } break;
            case Pipewire_EventKind_AddMetadata: {
                Pipewire_Object *object = pipewire_object_from_id(event->id);

                // NOTE(simon): Find the associated metadata based on issuer and key.
                Pipewire_Metadata *metadata = 0;
                for (Pipewire_Metadata *candidate = object->first_metadata; candidate; candidate = candidate->next) {
                    if (candidate->issuer == event->metadata_issuer && str8_equal(candidate->key, event->metadata_key)) {
                        metadata = candidate;
                        break;
                    }
                }

                // NOTE(simon): Free the metadata if we found it.
                if (metadata) {
                    pipewire_free_string(metadata->key);
                    pipewire_free_string(metadata->type);
                    pipewire_free_string(metadata->value);
                }

                if (!pipewire_object_is_nil(object)) {
                    if (event->metadata_value.size) {
                        // NOTE(simon): Allocate.
                        if (!metadata) {
                            metadata = pipewire_state->metadata_freelist;
                            if (metadata) {
                                sll_stack_pop(pipewire_state->metadata_freelist);
                                memory_zero_struct(metadata);
                            } else {
                                metadata = arena_push_struct(pipewire_state->object_arena, Pipewire_Metadata);
                            }

                            // NOTE(simon): Add to object.
                            dll_push_back(object->first_metadata, object->last_metadata, metadata);
                        }

                        // NOTE(simon): Fill.
                        metadata->issuer = event->metadata_issuer;
                        metadata->key    = pipewire_allocate_string(event->metadata_key);
                        metadata->type   = pipewire_allocate_string(event->metadata_type);
                        metadata->value  = pipewire_allocate_string(event->metadata_value);
                    } else {
                        // NOTE(simon): Remove from node.
                        if (metadata) {
                            dll_remove(object->first_metadata, object->last_metadata, metadata);
                            sll_stack_push(pipewire_state->metadata_freelist, metadata);
                        }
                    }
                }
            } break;
            case Pipewire_EventKind_Destroy: {
                Pipewire_Object *object = pipewire_object_from_id(event->id);

                if (!pipewire_object_is_nil(object)) {
                    // TODO(simon): Free associated resources.

                    // NOTE(simon): Free metadata.
                    for (Pipewire_Metadata *metadata = object->first_metadata, *next = 0; metadata; metadata = next) {
                        next = metadata;
                        pipewire_free_string(metadata->key);
                        pipewire_free_string(metadata->type);
                        pipewire_free_string(metadata->value);
                        dll_remove(object->first_metadata, object->last_metadata, metadata);
                        sll_stack_push(pipewire_state->metadata_freelist, metadata);
                    }

                    // NOTE(simon): Free parameters.
                    for (Pipewire_Parameter *parameter = object->first_parameter, *next = 0; parameter; parameter = next) {
                        next = parameter->next;
                        for (U64 i = 0; i < parameter->count; ++i) {
                            pipewire_free(parameter->parameters[i], SPA_POD_SIZE(parameter->parameters[i]));
                        }
                        pipewire_free(parameter->parameters, parameter->count * sizeof(*parameter->parameters));
                        dll_remove(object->first_parameter, object->last_parameter, parameter);
                        sll_stack_push(pipewire_state->parameter_freelist, parameter);
                    }

                    // NOTE(simon): Free properties.
                    for (U64 i = 0; i < object->property_count; ++i) {
                        pipewire_free_string(object->properties[i].key);
                        pipewire_free_string(object->properties[i].value);
                    }
                    pipewire_free(object->properties, object->property_count * sizeof(Pipewire_Property));

                    // NOTE(simon): Remove from id -> object map.
                    U64 hash = u64_hash(object->id);
                    Pipewire_ObjectList *object_bucket = &pipewire_state->object_map[hash & (pipewire_state->object_map_capacity - 1)];
                    dll_remove(object_bucket->first, object_bucket->last, object);
                    sll_stack_push(pipewire_state->object_freelist, object);
                }
            } break;
        }
    }
}



// NOTE(simon): Commands.

internal Pipewire_Command *pipewire_command_list_push(Arena *arena, Pipewire_CommandList *list) {
    Pipewire_Command *command = arena_push_struct(arena, Pipewire_Command);
    sll_queue_push(list->first, list->last, command);
    ++list->count;
    return command;
}

internal Str8 pipewire_serialized_string_from_command_list(Arena *arena, Pipewire_CommandList commands) {
    Arena_Temporary scratch = arena_get_scratch(&arena, 1);
    Str8List serialized = { 0 };

    str8_serial_push_type(scratch.arena, &serialized, &commands.count);
    for (Pipewire_Command *command = commands.first; command; command = command->next) {
        str8_serial_push_type(scratch.arena, &serialized, &command->kind);
        str8_serial_push_type(scratch.arena, &serialized, &command->entity);

        // NOTE(simon): Write properties.
        str8_serial_push_type(scratch.arena, &serialized, &command->property_count);
        for (U64 i = 0; i < command->property_count; ++i) {
            Pipewire_Property *property = &command->properties[i];
            str8_serial_push_type(scratch.arena, &serialized, &property->key.size);
            str8_serial_push_data(scratch.arena, &serialized, property->key.data, property->key.size);
            str8_serial_push_type(scratch.arena, &serialized, &property->value.size);
            str8_serial_push_data(scratch.arena, &serialized, property->value.data, property->value.size);
        }

        // NOTE(simon): Write parameter.
        str8_serial_push_type(scratch.arena, &serialized, &command->parameter_id);
        U64 parameter_size = command->parameter ? SPA_POD_SIZE(command->parameter) : 0;
        str8_serial_push_type(scratch.arena, &serialized, &parameter_size);
        str8_serial_push_data(scratch.arena, &serialized, command->parameter, parameter_size);

        str8_serial_push_type(scratch.arena, &serialized, &command->factory_name.size);
        str8_serial_push_data(scratch.arena, &serialized, command->factory_name.data, command->factory_name.size);
        str8_serial_push_type(scratch.arena, &serialized, &command->object_type.size);
        str8_serial_push_data(scratch.arena, &serialized, command->object_type.data, command->object_type.size);
        str8_serial_push_type(scratch.arena, &serialized, &command->object_version);
    }

    Str8 result = str8_join(arena, serialized);
    arena_end_temporary(scratch);
    return result;
}

internal Pipewire_CommandList pipewire_command_list_from_serialized_string(Arena *arena, Str8 string) {
    Pipewire_CommandList commands = { 0 };

    U64 read_offset = 0;
    read_offset += str8_deserial_read_type(string, read_offset, &commands.count);
    for (U64 i = 0; i < commands.count; ++i) {
        Pipewire_Command *command = arena_push_struct(arena, Pipewire_Command);
        sll_queue_push(commands.first, commands.last, command);

        read_offset += str8_deserial_read_type(string, read_offset, &command->kind);
        read_offset += str8_deserial_read_type(string, read_offset, &command->entity);

        // NOTE(simon): Read properties.
        read_offset += str8_deserial_read_type(string, read_offset, &command->property_count);
        command->properties = arena_push_array_no_zero(arena, Pipewire_Property, command->property_count);
        for (U64 j = 0; j < command->property_count; ++j) {
            Pipewire_Property *property = &command->properties[j];
            read_offset += str8_deserial_read_type(string, read_offset, &property->key.size);
            property->key.data = arena_push_array_no_zero(arena, U8, property->key.size);
            read_offset += str8_deserial_read_data(string, read_offset, property->key.data, property->key.size);
            read_offset += str8_deserial_read_type(string, read_offset, &property->value.size);
            property->value.data = arena_push_array_no_zero(arena, U8, property->value.size);
            read_offset += str8_deserial_read_data(string, read_offset, property->value.data, property->value.size);
        }

        // NOTE(simon): Read parameter.
        read_offset += str8_deserial_read_type(string, read_offset, &command->parameter_id);
        U64 parameter_size = 0;
        read_offset += str8_deserial_read_type(string, read_offset, &parameter_size);
        command->parameter = arena_push_no_zero(arena, parameter_size, 16);
        read_offset += str8_deserial_read_data(string, read_offset, command->parameter, parameter_size);

        read_offset += str8_deserial_read_type(string, read_offset, &command->factory_name.size);
        command->factory_name.data = arena_push_array_no_zero(arena, U8, command->factory_name.size);
        read_offset += str8_deserial_read_data(string, read_offset, command->factory_name.data, command->factory_name.size);
        read_offset += str8_deserial_read_type(string, read_offset, &command->object_type.size);
        command->object_type.data = arena_push_array_no_zero(arena, U8, command->object_type.size);
        read_offset += str8_deserial_read_data(string, read_offset, command->object_type.data, command->object_type.size);
        read_offset += str8_deserial_read_type(string, read_offset, &command->object_version);
    }

    return commands;
}

internal Void pipewire_execute_commands(Pipewire_CommandList commands) {
    for (Pipewire_Command *command = commands.first; command; command = command->next) {
        switch (command->kind) {
            case Pipewire_CommandKind_Null: {
            } break;
            case Pipewire_CommandKind_SetParameter: {
                Pipewire_Entity *entity = pipewire_entity_from_handle(command->entity);
                if (!pipewire_entity_is_nil(entity)) {
                    switch (entity->kind) {
                        case Pipewire_ObjectKind_Null: {
                        } break;
                        case Pipewire_ObjectKind_Client: {
                        } break;
                        case Pipewire_ObjectKind_Device: {
                            pw_device_set_param((struct pw_device *) entity->proxy, command->parameter_id, 0, command->parameter);
                        } break;
                        case Pipewire_ObjectKind_Factory: {
                        } break;
                        case Pipewire_ObjectKind_Link: {
                        } break;
                        case Pipewire_ObjectKind_Metadata: {
                        } break;
                        case Pipewire_ObjectKind_Module: {
                        } break;
                        case Pipewire_ObjectKind_Node: {
                            pw_node_set_param((struct pw_node *) entity->proxy, command->parameter_id, 0, command->parameter);
                        } break;
                        case Pipewire_ObjectKind_Port: {
                        } break;
                        case Pipewire_ObjectKind_COUNT: {
                        } break;
                    }
                }
            } break;
            case Pipewire_CommandKind_Create: {
                Arena_Temporary scratch = arena_get_scratch(0, 0);
                CStr factory_name = cstr_from_str8(scratch.arena, command->factory_name);
                CStr object_type  = cstr_from_str8(scratch.arena, command->object_type);

                // NOTE(simon): Create properties.
                struct pw_properties *properties = pw_properties_new(NULL, NULL);
                for (U64 i = 0; i < command->property_count; ++i) {
                    CStr key = cstr_from_str8(scratch.arena, command->properties[i].key);
                    CStr value = cstr_from_str8(scratch.arena, command->properties[i].value);
                    pw_properties_set(properties, key, value);
                }

                pw_core_create_object(
                    pipewire_state->core,
                    factory_name,
                    object_type,
                    command->object_version,
                    &properties->dict,
                    0
                );

                pw_properties_free(properties);
                arena_end_temporary(scratch);
            } break;
            case Pipewire_CommandKind_Delete: {
                Pipewire_Entity *entity = pipewire_entity_from_handle(command->entity);
                if (!pipewire_entity_is_nil(entity)) {
                    pw_registry_destroy(pipewire_state->registry, entity->id);
                }
            } break;
            case Pipewire_CommandKind_COUNT: {
            } break;
        }
    }
}



// NOTE(simon): Command helpers.
internal Void pipewire_delete(Pipewire_Object *object) {
    Pipewire_Command *command = pipewire_command_list_push(pipewire_state->command_arena, &pipewire_state->commands);
    command->kind   = Pipewire_CommandKind_Delete;
    command->entity = object->entity;
}

internal Void pipewire_set_volume(Pipewire_Object *object, Pipewire_Volume volume) {
    // NOTE(simon): Query for card.
    Pipewire_Object *card = pipewire_object_from_property_name(object, str8_literal(PW_KEY_DEVICE_ID));
    if (!pipewire_object_is_card(card)) {
        card = &pipewire_nil_object;
    }

    // NOTE(simon): Query for active card port.
    U32 device_id  = SPA_ID_INVALID;
    U32 port_index = SPA_ID_INVALID;
    B32 device_has_volume = false;
    U64Decode device_decode = pipewire_u64_from_property_name(object, str8_literal("card.profile.device"));
    if (device_decode.size) {
        U32 device = (U32) device_decode.value;

        Pipewire_Parameter *route_parameter = pipewire_parameter_from_id(card, SPA_PARAM_Route);
        for (U64 i = 0; i < route_parameter->count; ++i) {
            struct spa_pod *parameter = route_parameter->parameters[i];

            const struct spa_pod_prop *index_prop  = spa_pod_find_prop(parameter, 0, SPA_PARAM_ROUTE_index);
            const struct spa_pod_prop *device_prop = spa_pod_find_prop(parameter, 0, SPA_PARAM_ROUTE_device);
            const struct spa_pod_prop *props_prop  = spa_pod_find_prop(parameter, 0, SPA_PARAM_ROUTE_props);

            if (!index_prop || !device_prop || !props_prop) {
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

        Pipewire_Command *command = pipewire_command_list_push(pipewire_state->command_arena, &pipewire_state->commands);
        command->kind = Pipewire_CommandKind_SetParameter;
        command->entity = card->entity;
        command->parameter_id = SPA_PARAM_Route;
        command->parameter = arena_push_no_zero(pipewire_state->command_arena, SPA_POD_SIZE(pod), 16);
        memory_copy(command->parameter, pod, SPA_POD_SIZE(pod));
    } else if (!pipewire_object_is_nil(object)) {
        struct spa_pod *pod = spa_pod_builder_add_object(
            &builder,
            SPA_TYPE_OBJECT_Props, SPA_PARAM_Props,
            SPA_PROP_mute, SPA_POD_Bool(volume.mute),
            SPA_PROP_channelVolumes, SPA_POD_Array(sizeof(F32), SPA_TYPE_Float, volume.channel_count, volume.channel_volumes)
        );

        Pipewire_Command *command = pipewire_command_list_push(pipewire_state->command_arena, &pipewire_state->commands);
        command->kind = Pipewire_CommandKind_SetParameter;
        command->entity = object->entity;
        command->parameter_id = SPA_PARAM_Props;
        command->parameter = arena_push_no_zero(pipewire_state->command_arena, SPA_POD_SIZE(pod), 16);
        memory_copy(command->parameter, pod, SPA_POD_SIZE(pod));
    }
#pragma clang diagnostic pop
}

internal Void pipewire_link(Pipewire_Object *output, Pipewire_Object *input) {
    B32 good = true;

    Pipewire_Property properties[3] = { 0 };
    U32 property_count = 0;

    properties[property_count].key   = str8_literal(PW_KEY_OBJECT_LINGER);
    properties[property_count].value = str8_literal("true");
    ++property_count;

    // NOTE(simon): Fill output object.
    if (output->kind == Pipewire_ObjectKind_Port) {
        properties[property_count].key   = str8_literal(PW_KEY_LINK_OUTPUT_PORT);
        properties[property_count].value = str8_format(pipewire_state->command_arena, "%u", output->id);
        ++property_count;
    } else if (output->kind == Pipewire_ObjectKind_Node) {
        properties[property_count].key   = str8_literal(PW_KEY_LINK_OUTPUT_NODE);
        properties[property_count].value = str8_format(pipewire_state->command_arena, "%u", output->id);
        ++property_count;
    } else {
        good = false;
    }

    // NOTE(simon): Fill input object.
    if (input->kind == Pipewire_ObjectKind_Port) {
        properties[property_count].key   = str8_literal(PW_KEY_LINK_INPUT_PORT);
        properties[property_count].value = str8_format(pipewire_state->command_arena, "%u", input->id);
        ++property_count;
    } else if (input->kind == Pipewire_ObjectKind_Node) {
        properties[property_count].key   = str8_literal(PW_KEY_LINK_INPUT_NODE);
        properties[property_count].value = str8_format(pipewire_state->command_arena, "%u", input->id);
        ++property_count;
    } else {
        good = false;
    }

    // NOTE(simon): Issue command if we could fill out all required properties.
    if (good) {
        Pipewire_Command *command = pipewire_command_list_push(pipewire_state->command_arena, &pipewire_state->commands);
        command->kind           = Pipewire_CommandKind_Create;
        command->factory_name   = str8_literal("link-factory");
        command->object_type    = str8_literal(PW_TYPE_INTERFACE_Link);
        command->object_version = PW_VERSION_LINK;
        command->property_count = property_count;
        command->properties     = arena_push_array_no_zero(pipewire_state->command_arena, Pipewire_Property, command->property_count);
        memory_copy(command->properties, properties, command->property_count * sizeof(Pipewire_Property));
    }
}



// NOTE(simon): Objects <-> handle.

internal B32 pipewire_object_is_nil(Pipewire_Object *object) {
    B32 result = !object || object == &pipewire_nil_object;
    return result;
}

internal Pipewire_Object *pipewire_object_from_handle(Pipewire_Handle handle) {
    Pipewire_Object *object = (Pipewire_Object *) pointer_from_integer(handle.u64[0]);
    if (!object || object->generation != handle.u64[1]) {
        object = &pipewire_nil_object;
    }
    return object;
}

internal Pipewire_Handle pipewire_handle_from_object(Pipewire_Object *object) {
    Pipewire_Handle handle = { 0 };

    if (!pipewire_object_is_nil(object)) {
        handle.u64[0] = integer_from_pointer(object);
        handle.u64[1] = object->generation;
    }

    return handle;
}

internal Pipewire_Object *pipewire_object_from_id(U32 id) {
    Pipewire_Object *result = &pipewire_nil_object;

    U64 hash = u64_hash(id);
    Pipewire_ObjectList *object_bucket = &pipewire_state->object_map[hash & (pipewire_state->object_map_capacity - 1)];
    for (Pipewire_Object *object = object_bucket->first; object; object = object->next) {
        if (object->id == id) {
            result = object;
            break;
        }
    }

    return result;
}



// NOTE(simon): Object alloction/freeing.

internal Void *pipewire_allocate(U64 size) {
    U64 chunk_index = pipewire_chunk_index_from_size(size);
    Pipewire_ChunkNode *chunk = 0;

    if (chunk_index == array_count(pipewire_chunk_sizes)) {
        Pipewire_ChunkNode *best_chunk_previous = 0;
        Pipewire_ChunkNode *best_chunk = 0;
        for (Pipewire_ChunkNode *candidate = pipewire_state->chunk_freelist[chunk_index - 1], *previous = 0; candidate; previous = candidate, candidate = candidate->next) {
            if (candidate->size >= size && (!best_chunk || candidate->size < best_chunk->size)) {
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
            chunk = (Pipewire_ChunkNode *) arena_push_no_zero(pipewire_state->object_arena, ceiled_size, 16);
        }
    } else if (chunk_index != 0) {
        chunk = pipewire_state->chunk_freelist[chunk_index - 1];
        if (chunk) {
            sll_stack_pop(pipewire_state->chunk_freelist[chunk_index - 1]);
        } else {
            chunk = (Pipewire_ChunkNode *) arena_push_no_zero(pipewire_state->object_arena, pipewire_chunk_sizes[chunk_index - 1], 16);
        }
    }

    Void *result = (Void *) chunk;
    return result;
}

internal Void pipewire_free(Void *data, U64 size) {
    U64 chunk_index = pipewire_chunk_index_from_size(size);
    if (data && chunk_index) {
        Pipewire_ChunkNode *chunk = (Pipewire_ChunkNode *) data;
        chunk->size = u64_ceil_to_power_of_2(size);
        sll_stack_push(pipewire_state->chunk_freelist[chunk_index - 1], chunk);
    }
}

internal Str8 pipewire_allocate_string(Str8 string) {
    U8 *data = pipewire_allocate(string.size);
    memory_copy(data, string.data, string.size);
    Str8 result = str8(data, string.size);
    return result;
}

internal Void pipewire_free_string(Str8 string) {
    pipewire_free(string.data, string.size);
}



internal Pipewire_ObjectArray pipewire_objects_from_kind(Arena *arena, Pipewire_ObjectKind kind) {
    // NOTE(simon): Count the number of objects.
    U64 count = 0;
    for (U64 i = 0; i < pipewire_state->object_map_capacity; ++i) {
        Pipewire_ObjectList *object_list = &pipewire_state->object_map[i];
        for (Pipewire_Object *object = object_list->first; object; object = object->next) {
            if (kind != Pipewire_ObjectKind_Null && object->kind != kind) {
                continue;
            }

            ++count;
        }
    }

    // NOTE(simon): Collect objects.
    Pipewire_ObjectArray result = { 0 };
    result.objects = arena_push_array(arena, Pipewire_Object *, count);
    for (U64 i = 0; i < pipewire_state->object_map_capacity; ++i) {
        Pipewire_ObjectList *object_list = &pipewire_state->object_map[i];
        for (Pipewire_Object *object = object_list->first; object; object = object->next) {
            if (kind != Pipewire_ObjectKind_Null && object->kind != kind) {
                continue;
            }

            result.objects[result.count++] = object;
        }
    }

    return result;
}



// NOTE(simon): Object helpers.

internal B32 pipewire_object_is_card(Pipewire_Object *object) {
    Str8 media_class = pipewire_string_from_property_name(object, str8_literal(PW_KEY_MEDIA_CLASS));
    B32  result      = object->kind == Pipewire_ObjectKind_Device && str8_equal(media_class, str8_literal("Audio/Device"));
    return result;
}

internal Pipewire_Volume pipewire_volume_from_object(Pipewire_Object *object) {
    Pipewire_Volume volume = { 0 };
    B32 has_volume = false;

    // NOTE(simon): Query for card.
    Pipewire_Object *card = pipewire_object_from_property_name(object, str8_literal(PW_KEY_DEVICE_ID));
    if (!pipewire_object_is_card(card)) {
        card = &pipewire_nil_object;
    }

    // NOTE(simon): Query for card device volume.
    U64Decode device_decode = pipewire_u64_from_property_name(object, str8_literal("card.profile.device"));
    if (device_decode.size) {
        U32 device = (U32) device_decode.value;

        Pipewire_Parameter *route_parameter = pipewire_parameter_from_id(card, SPA_PARAM_Route);
        for (U64 i = 0; i < route_parameter->count; ++i) {
            const struct spa_pod_prop *index_prop  = spa_pod_find_prop(route_parameter->parameters[i], 0, SPA_PARAM_ROUTE_index);
            const struct spa_pod_prop *device_prop = spa_pod_find_prop(route_parameter->parameters[i], 0, SPA_PARAM_ROUTE_device);
            const struct spa_pod_prop *props_prop  = spa_pod_find_prop(route_parameter->parameters[i], 0, SPA_PARAM_ROUTE_props);

            if (!index_prop || !device_prop || !props_prop) {
                continue;
            }

            S32 route_index  = 0;
            S32 route_device = 0;
            spa_pod_get_int(&index_prop->value,  &route_index);
            spa_pod_get_int(&device_prop->value, &route_device);

            if ((U32) route_device == device) {
                pipewire_parse_volume((struct spa_pod *) &props_prop->value, &volume);
                has_volume = true;
            }
        }
    }

    // NOTE(simon): Query the node for volume.
    if (!has_volume) {
        Pipewire_Parameter *props_parameter = pipewire_parameter_from_id(object, SPA_PARAM_Props);
        if (props_parameter->count >= 1) {
            pipewire_parse_volume(props_parameter->parameters[0], &volume);
            has_volume = true;
        }
    }

    return volume;
}



// NOTE(simon): Properties from objects.

internal B32 pipewire_property_is_nil(Pipewire_Property *property) {
    B32 result = !property || property == &pipewire_nil_property;
    return result;
}

internal Pipewire_Property *pipewire_property_from_name(Pipewire_Object *object, Str8 name) {
    Pipewire_Property *result = &pipewire_nil_property;
    for (U64 i = 0; i < object->property_count; ++i) {
        Pipewire_Property *property = &object->properties[i];
        if (str8_equal(property->key, name)) {
            result = property;
            break;
        }
    }
    return result;
}

internal Str8 pipewire_string_from_property_name(Pipewire_Object *object, Str8 name) {
    Pipewire_Property *property = pipewire_property_from_name(object, name);
    return property->value;
}

internal U64Decode pipewire_u64_from_property_name(Pipewire_Object *object, Str8 name) {
    Pipewire_Property *property = pipewire_property_from_name(object, name);
    U64Decode decode = u64_from_str8(property->value);
    return decode;
}

internal Pipewire_Object *pipewire_object_from_property_name(Pipewire_Object *object, Str8 name) {
    U64Decode decode = pipewire_u64_from_property_name(object, name);

    Pipewire_Object *result = &pipewire_nil_object;
    if (decode.size != 0) {
        result = pipewire_object_from_id((U32) decode.value);
    }

    return result;
}



// NOTE(simon): Parameters from objects.

internal B32 pipewire_parameter_is_nil(Pipewire_Parameter *parameter) {
    B32 result = !parameter || parameter == &pipewire_nil_parameter;
    return result;
}

internal Pipewire_Parameter *pipewire_parameter_from_id(Pipewire_Object *object, U32 id) {
    Pipewire_Parameter *result = &pipewire_nil_parameter;
    for (Pipewire_Parameter *parameter = object->first_parameter; parameter; parameter = parameter->next) {
        if (parameter->id == id) {
            result = parameter;
            break;
        }
    }
    return result;
}



// NOTE(simon): Entity allocation/freeing.

internal Pipewire_Entity *pipewire_entity_allocate(U32 id) {
    // NOTE(simon): Allocate.
    Pipewire_Entity *entity = pipewire_state->entity_freelist;
    U64 generation = 0;
    if (!pipewire_entity_is_nil(entity)) {
        generation = entity->generation + 1;
        sll_stack_pop(pipewire_state->entity_freelist);
        memory_zero_struct(entity);
    } else {
        entity = arena_push_struct(pipewire_state->arena, Pipewire_Entity);
    }

    // NOTE(simon): Fill.
    entity->id         = id;
    entity->generation = generation;

    // NOTE(simon): Insert into id -> entity map.
    U64 hash = u64_hash(id);
    Pipewire_EntityList *entity_bucket = &pipewire_state->entity_map[hash & (pipewire_state->entity_map_capacity - 1)];
    dll_push_back(entity_bucket->first, entity_bucket->last, entity);

    return entity;
}

internal Void pipewire_entity_free(Pipewire_Entity *entity) {
    if (!pipewire_entity_is_nil(entity)) {
        if (entity->info) {
            switch (entity->kind) {
                case Pipewire_ObjectKind_Null: {
                } break;
                case Pipewire_ObjectKind_Client: {
                    pw_client_info_free((struct pw_client_info *) entity->info);
                } break;
                case Pipewire_ObjectKind_Device: {
                    pw_device_info_free((struct pw_device_info *) entity->info);
                } break;
                case Pipewire_ObjectKind_Factory: {
                    pw_factory_info_free((struct pw_factory_info *) entity->info);
                } break;
                case Pipewire_ObjectKind_Link: {
                    pw_link_info_free((struct pw_link_info *) entity->info);
                } break;
                case Pipewire_ObjectKind_Metadata: {
                    // NOTE(simon): There is no info for metadata objects.
                } break;
                case Pipewire_ObjectKind_Module: {
                    pw_module_info_free((struct pw_module_info *) entity->info);
                } break;
                case Pipewire_ObjectKind_Node: {
                    pw_node_info_free((struct pw_node_info *) entity->info);
                } break;
                case Pipewire_ObjectKind_Port: {
                    pw_port_info_free((struct pw_port_info *) entity->info);
                } break;
                case Pipewire_ObjectKind_COUNT: {
                } break;
            }
        }

        // NOTE(simon): Unhook listeners.
        spa_hook_remove(&entity->object_listener);

        // NOTE(simon): Destroy handles.
        pw_proxy_destroy(entity->proxy);

        // NOTE(simon): Remove from id -> entity map.
        U64 hash = u64_hash(entity->id);
        Pipewire_EntityList *entity_bucket = &pipewire_state->entity_map[hash & (pipewire_state->entity_map_capacity - 1)];
        dll_remove(entity_bucket->first, entity_bucket->last, entity);

        // NOTE(simon): Add to freelist.
        sll_stack_push(pipewire_state->entity_freelist, entity);
    }
}



// NOTE(simon): Entity <-> handle.

internal Pipewire_Entity *pipewire_entity_from_handle(Pipewire_Handle handle) {
    Pipewire_Entity *result = &pipewire_entity_nil;

    U32 entity_id         = (U32) handle.u64[0];
    U64 entity_generation = handle.u64[1];

    U64 hash = u64_hash(entity_id);
    Pipewire_EntityList *entity_bucket = &pipewire_state->entity_map[hash & (pipewire_state->entity_map_capacity - 1)];
    for (Pipewire_Entity *entity = entity_bucket->first; entity; entity = entity->next) {
        if (entity->id == entity_id && entity->generation == entity_generation) {
            result = entity;
            break;
        }
    }

    return result;
}

internal Pipewire_Entity *pipewire_entity_from_id(U32 id) {
    Pipewire_Entity *result = &pipewire_entity_nil;

    U64 hash = u64_hash(id);
    Pipewire_EntityList *entity_bucket = &pipewire_state->entity_map[hash & (pipewire_state->entity_map_capacity - 1)];
    for (Pipewire_Entity *entity = entity_bucket->first; entity; entity = entity->next) {
        if (entity->id == id) {
            result = entity;
            break;
        }
    }

    return result;
}

internal Pipewire_Handle pipewire_handle_from_entity(Pipewire_Entity *entity) {
    Pipewire_Handle result = { 0 };
    if (!pipewire_entity_is_nil(entity)) {
        result.u64[0] = entity->id;
        result.u64[1] = entity->generation;
    }
    return result;
}

internal B32 pipewire_entity_is_nil(Pipewire_Entity *entity) {
    B32 result = !entity || entity == &pipewire_entity_nil;
    return result;
}



internal Void pipewire_synchronize(Void) {
    int result = pw_core_sync(pipewire_state->core, PW_ID_CORE, pipewire_state->core_sequence);
    if (SPA_RESULT_IS_ASYNC(result)) {
        pipewire_state->core_sequence = result;
    }
}



// NOTE(simon): Startup/shutdown.

internal Void pipewire_init(Void) {
    Arena *arena = arena_create();
    pipewire_state = arena_push_struct(arena, Pipewire_State);
    pipewire_state->arena = arena;
    pipewire_state->event_arena = arena_create();
    pipewire_state->command_arena = arena_create();

    pipewire_state->entity_map_capacity = 1024;
    pipewire_state->entity_map = arena_push_array(pipewire_state->arena, Pipewire_EntityList, pipewire_state->entity_map_capacity);

    pipewire_state->c2u_ring_mutex = os_mutex_create();
    pipewire_state->c2u_ring_condition_variable = os_condition_variable_create();
    pipewire_state->c2u_ring_size = megabytes(4);
    pipewire_state->c2u_ring_base = arena_push_array(pipewire_state->arena, U8, pipewire_state->c2u_ring_size);

    pipewire_state->u2c_ring_mutex = os_mutex_create();
    pipewire_state->u2c_ring_condition_variable = os_condition_variable_create();
    pipewire_state->u2c_ring_size = megabytes(1);
    pipewire_state->u2c_ring_base = arena_push_array(pipewire_state->arena, U8, pipewire_state->u2c_ring_size);

    pipewire_state->object_arena = arena_create();
    pipewire_state->object_map_capacity = 1024;
    pipewire_state->object_map = arena_push_array(pipewire_state->object_arena, Pipewire_ObjectList, pipewire_state->object_map_capacity);

    pw_init(0, 0);

    pipewire_state->loop     = pw_thread_loop_new("PipeWire CTRL", 0);
    pipewire_state->context  = pw_context_new(pw_thread_loop_get_loop(pipewire_state->loop), 0, 0);
    pipewire_state->core     = pw_context_connect(pipewire_state->context, 0, 0);
    pipewire_state->registry = pw_core_get_registry(pipewire_state->core, PW_VERSION_REGISTRY, 0);

    pw_core_add_listener(pipewire_state->core, &pipewire_state->core_listener, &pipewire_core_listener, 0);
    pw_registry_add_listener(pipewire_state->registry, &pipewire_state->registry_listener, &pipewire_registry_listener, 0);

    pipewire_state->wakeup_event = pw_loop_add_event(pw_thread_loop_get_loop(pipewire_state->loop), pipewire_wakeup_event, 0);

    pw_thread_loop_start(pipewire_state->loop);
}

internal Void pipewire_deinit(Void) {
    pw_thread_loop_stop(pipewire_state->loop);

    arena_destroy(pipewire_state->object_arena);

    // NOTE(simon): Destroy all entities.
    for (U64 i = 0; i < pipewire_state->entity_map_capacity; ++i) {
        Pipewire_EntityList *entity_bucket = &pipewire_state->entity_map[i];
        for (Pipewire_Entity *entity = entity_bucket->first, *next = 0; entity; entity = next) {
            next = entity->next;
            pipewire_entity_free(entity);
        }
    }

    // TODO(simon): Make sure to destroy the thread local scratch arenas on the control thread.

    // NOTE(simon): Unhook all listeners.
    spa_hook_remove(&pipewire_state->registry_listener);
    spa_hook_remove(&pipewire_state->core_listener);

    // NOTE(simon): Destroy all handles.
    pw_proxy_destroy((struct pw_proxy *) pipewire_state->registry);
    pw_core_disconnect(pipewire_state->core);
    pw_context_destroy(pipewire_state->context);
    pw_thread_loop_destroy(pipewire_state->loop);

    pw_deinit();

    // NOTE(simon): Destroy synchronization primitives for communication.
    os_condition_variable_destroy(pipewire_state->u2c_ring_condition_variable);
    os_mutex_destroy(pipewire_state->u2c_ring_mutex);

    os_condition_variable_destroy(pipewire_state->c2u_ring_condition_variable);
    os_mutex_destroy(pipewire_state->c2u_ring_mutex);

    arena_destroy(pipewire_state->arena);
}

internal Void pipewire_poll_events(Void) {
    Arena_Temporary scratch = arena_get_scratch(0, 0);
    Pipewire_EventList events = pipewire_c2u_pop_events(scratch.arena, os_now_nanoseconds() + 200 * 1000);
    pipewire_apply_events(events);
    arena_end_temporary(scratch);
}

internal Void pipewire_push_commands(Void) {
    pipewire_u2c_push_commands(pipewire_state->commands);
    memory_zero_struct(&pipewire_state->commands);
    arena_reset(pipewire_state->command_arena);
    pw_loop_signal_event(pw_thread_loop_get_loop(pipewire_state->loop), pipewire_state->wakeup_event);
}

internal Void pipewire_set_wakeup_hook(VoidFunction *wakeup_hook) {
    pipewire_state->wakeup_hook = wakeup_hook;
}



// NOTE(simon): Control to user thread communication.

internal Pipewire_EventList pipewire_c2u_pop_events(Arena *arena, U64 end_ns) {
    Arena_Temporary scratch = arena_get_scratch(&arena, 1);
    Str8 serialized_events = { 0 };
    os_mutex_scope(pipewire_state->c2u_ring_mutex)
    for (;;) {
        U64 unconsumed_size = pipewire_state->c2u_ring_write_position - pipewire_state->c2u_ring_read_position;
        if (unconsumed_size >= sizeof(U64)) {
            pipewire_state->c2u_ring_read_position += ring_read_type(pipewire_state->c2u_ring_base, pipewire_state->c2u_ring_size, pipewire_state->c2u_ring_read_position, &serialized_events.size);
            serialized_events.data = arena_push_array_no_zero(scratch.arena, U8, serialized_events.size);
            pipewire_state->c2u_ring_read_position += ring_read(pipewire_state->c2u_ring_base, pipewire_state->c2u_ring_size, pipewire_state->c2u_ring_read_position, serialized_events.data, serialized_events.size);
            break;
        }

        if (!os_condition_variable_wait(pipewire_state->c2u_ring_condition_variable, pipewire_state->c2u_ring_mutex, end_ns)) {
            break;
        }
    }

    if (serialized_events.size) {
        os_condition_variable_signal(pipewire_state->c2u_ring_condition_variable);
    }

    Pipewire_EventList events = pipewire_event_list_from_serialized_string(arena, serialized_events);
    arena_end_temporary(scratch);
    return events;
}

internal Void pipewire_c2u_push_events(Pipewire_EventList events) {
    if (!events.count) {
        return;
    }

    Arena_Temporary scratch = arena_get_scratch(0, 0);
    Str8 serialized_events = pipewire_serialized_string_from_event_list(scratch.arena, events);
    os_mutex_scope(pipewire_state->c2u_ring_mutex)
    for (;;) {
        U64 unconsumed_size = pipewire_state->c2u_ring_write_position - pipewire_state->c2u_ring_read_position;
        U64 available_size  = pipewire_state->c2u_ring_size - unconsumed_size;
        if (available_size >= sizeof(serialized_events.size) + serialized_events.size) {
            pipewire_state->c2u_ring_write_position += ring_write_type(pipewire_state->c2u_ring_base, pipewire_state->c2u_ring_size, pipewire_state->c2u_ring_write_position, &serialized_events.size);
            pipewire_state->c2u_ring_write_position += ring_write(pipewire_state->c2u_ring_base, pipewire_state->c2u_ring_size, pipewire_state->c2u_ring_write_position, serialized_events.data, serialized_events.size);
            break;
        }
        os_condition_variable_wait(pipewire_state->c2u_ring_condition_variable, pipewire_state->c2u_ring_mutex, U64_MAX);
    }
    os_condition_variable_signal(pipewire_state->c2u_ring_condition_variable);

    if (pipewire_state->wakeup_hook) {
        pipewire_state->wakeup_hook();
    }

    arena_end_temporary(scratch);
}



// NOTE(simon): User to control thread communication.

internal Pipewire_CommandList pipewire_u2c_pop_commands(Arena *arena, U64 end_ns) {
    Arena_Temporary scratch = arena_get_scratch(&arena, 1);
    Str8 serialized_commands = { 0 };
    os_mutex_scope(pipewire_state->u2c_ring_mutex)
    for (;;) {
        U64 unconsumed_size = pipewire_state->u2c_ring_write_position - pipewire_state->u2c_ring_read_position;
        if (unconsumed_size >= sizeof(U64)) {
            pipewire_state->u2c_ring_read_position += ring_read_type(pipewire_state->u2c_ring_base, pipewire_state->u2c_ring_size, pipewire_state->u2c_ring_read_position, &serialized_commands.size);
            serialized_commands.data = arena_push_array_no_zero(scratch.arena, U8, serialized_commands.size);
            pipewire_state->u2c_ring_read_position += ring_read(pipewire_state->u2c_ring_base, pipewire_state->u2c_ring_size, pipewire_state->u2c_ring_read_position, serialized_commands.data, serialized_commands.size);
            break;
        }

        if (!os_condition_variable_wait(pipewire_state->u2c_ring_condition_variable, pipewire_state->u2c_ring_mutex, end_ns)) {
            break;
        }
    }

    if (serialized_commands.size) {
        os_condition_variable_signal(pipewire_state->u2c_ring_condition_variable);
    }

    Pipewire_CommandList commands = pipewire_command_list_from_serialized_string(arena, serialized_commands);
    arena_end_temporary(scratch);
    return commands;
}

internal Void pipewire_u2c_push_commands(Pipewire_CommandList commands) {
    if (!commands.count) {
        return;
    }

    Arena_Temporary scratch = arena_get_scratch(0, 0);
    Str8 serialized_commands = pipewire_serialized_string_from_command_list(scratch.arena, commands);
    os_mutex_scope(pipewire_state->u2c_ring_mutex)
    for (;;) {
        U64 unconsumed_size = pipewire_state->u2c_ring_write_position - pipewire_state->u2c_ring_read_position;
        U64 available_size  = pipewire_state->u2c_ring_size - unconsumed_size;
        if (available_size >= sizeof(serialized_commands.size) + serialized_commands.size) {
            pipewire_state->u2c_ring_write_position += ring_write_type(pipewire_state->u2c_ring_base, pipewire_state->u2c_ring_size, pipewire_state->u2c_ring_write_position, &serialized_commands.size);
            pipewire_state->u2c_ring_write_position += ring_write(pipewire_state->u2c_ring_base, pipewire_state->u2c_ring_size, pipewire_state->u2c_ring_write_position, serialized_commands.data, serialized_commands.size);
            break;
        }
        os_condition_variable_wait(pipewire_state->u2c_ring_condition_variable, pipewire_state->u2c_ring_mutex, U64_MAX);
    }
    os_condition_variable_signal(pipewire_state->u2c_ring_condition_variable);
    arena_end_temporary(scratch);
}



internal Void pipewire_wakeup_event(Void *data, U64 count) {
    pipewire_synchronize();
}



// NOTE(simon): Client listeners.

internal Void pipewire_client_info(Void *data, const struct pw_client_info *info) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;
    info = entity->info = pw_client_info_merge(entity->info, info, !entity->changed);
    entity->changed |= info->change_mask != 0;

    if (info->change_mask & PW_CLIENT_CHANGE_MASK_PROPS) {
        pipewire_event_list_push_properties(pipewire_state->event_arena, &pipewire_state->events, entity->id, info->props);
    }
}

internal Void pipewire_client_permissions(Void *data, U32 index, U32 n_permissions, const struct pw_permission *permissions) {
}



// NOTE(simon): Core listeners.

internal Void pipewire_core_done(Void *data, U32 id, S32 sequence) {
    if (id != PW_ID_CORE || sequence != pipewire_state->core_sequence) {
        return;
    }

    Arena_Temporary scratch = arena_get_scratch(0, 0);

    // NOTE(simon): Generate change events.
    for (U64 i = 0; i < pipewire_state->entity_map_capacity; ++i) {
        Pipewire_EntityList *bucket = &pipewire_state->entity_map[i];
        for (Pipewire_Entity *entity = bucket->first; entity; entity = entity->next) {
            entity->changed = false;
        }
    }

    pipewire_c2u_push_events(pipewire_state->events);

    // NOTE(simon): Execute any commands that we have.
    Pipewire_CommandList commands = pipewire_u2c_pop_commands(scratch.arena, os_now_nanoseconds() + 200 * 1000);
    pipewire_execute_commands(commands);

    memory_zero_struct(&pipewire_state->events);
    arena_reset(pipewire_state->event_arena);

    arena_end_temporary(scratch);
}

internal Void pipewire_core_error(Void *data, U32 id, S32 sequence, S32 res, const char *message) {
    Pipewire_Entity *entity = pipewire_entity_from_id(id);
    Str8 kind = pipewire_string_from_object_kind(entity->kind);
    // TODO(simon): Should we forward this to the main thread or do we log it here and exit?
    fprintf(stderr, "ERROR: Fatal error on PipeWire object %u (%.*s): %s (%d)\n", id, str8_expand(kind), message, res);
}



// NOTE(simon): Device listeners.

internal Void pipewire_device_info(Void *data, const struct pw_device_info *info) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;
    info = entity->info = pw_device_info_merge(entity->info, info, !entity->changed);
    entity->changed |= info->change_mask != 0;

    if (info->change_mask & PW_DEVICE_CHANGE_MASK_PROPS) {
        pipewire_event_list_push_properties(pipewire_state->event_arena, &pipewire_state->events, entity->id, info->props);
    }

    if (info->change_mask & PW_DEVICE_CHANGE_MASK_PARAMS) {
        for (U32 i = 0; i < info->n_params; ++i) {
            U32 id = info->params[i].id;

            // NOTE(simon): Set by pw_device_info_merge for new/updated
            // parameters. Only enumerate paramters that have changed.
            if (info->params[i].user == 0) {
                continue;
            }

            Pipewire_Event *event = pipewire_event_list_push(pipewire_state->event_arena, &pipewire_state->events);
            event->kind = Pipewire_EventKind_UpdateParameter;
            event->id = entity->id;
            event->parameter_id = id;
            event->parameter_flags = info->params[i].flags;

            // NOTE(simon): No purpose in requesting parameters that we cannot read.
            if (info->params[i].flags & SPA_PARAM_INFO_READ) {
                int result = pw_device_enum_params((struct pw_device *) entity->proxy, ++info->params[i].seq, id, 0, U32_MAX, 0);
                if (SPA_RESULT_IS_ASYNC(result)) {
                    info->params[i].seq = result;
                }
                event->parameter_sequence = info->params[i].seq;
            }
        }
    }

    // TODO(simon): Unlikely that we change multiple times within one sync
    // period, but it is possible and this would generate extra churn.
    if (entity->changed) {
        pipewire_synchronize();
    }
}

// TODO(simon): Possibly fold pipewire_device_param, pipewire_node_param, pipewire_port_param and into one function.
internal Void pipewire_device_param(Void *data, S32 sequence, U32 id, U32 index, U32 next, const struct spa_pod *param) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;

    Pipewire_Event *event = 0;
    for (Pipewire_Event *candidate = pipewire_state->events.first; candidate; candidate = candidate->next) {
        if (
            candidate->kind == Pipewire_EventKind_UpdateParameter &&
            candidate->id == entity->id &&
            candidate->parameter_id == id &&
            candidate->parameter_sequence == sequence
        ) {
            event = candidate;
            break;
        }
    }

    if (event) {
        U64 size = SPA_POD_SIZE(param);
        Pipewire_ParameterNode *node = arena_push_struct(pipewire_state->event_arena, Pipewire_ParameterNode);
        node->parameter = (struct spa_pod *) arena_push_array(pipewire_state->event_arena, U8, size);
        memory_copy(node->parameter, param, size);
        sll_queue_push(event->first_parameter, event->last_parameter, node);
        ++event->parameter_count;
    }
}



// NOTE(simon): Factory listeners.

internal Void pipewire_factory_info(Void *data, const struct pw_factory_info *info) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;
    info = entity->info = pw_factory_info_merge(entity->info, info, !entity->changed);
    entity->changed |= info->change_mask != 0;

    if (info->change_mask & PW_FACTORY_CHANGE_MASK_PROPS) {
        pipewire_event_list_push_properties(pipewire_state->event_arena, &pipewire_state->events, entity->id, info->props);
    }
}



// NOTE(simon): Link listeners.

internal Void pipewire_link_info(Void *data, const struct pw_link_info *info) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;
    info = entity->info = pw_link_info_merge(entity->info, info, !entity->changed);
    entity->changed |= info->change_mask != 0;

    if (info->change_mask & PW_LINK_CHANGE_MASK_PROPS) {
        pipewire_event_list_push_properties(pipewire_state->event_arena, &pipewire_state->events, entity->id, info->props);
    }
}



// NOTE(simon): Metadata listeners.
internal S32 pipewire_metadata_property(Void *data, U32 subject, const char *key, const char *type, const char *value) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;

    Pipewire_Event *event = pipewire_event_list_push(pipewire_state->event_arena, &pipewire_state->events);
    event->kind            = Pipewire_EventKind_AddMetadata;
    event->id              = subject;
    event->metadata_issuer = entity->id;
    event->metadata_key    = str8_copy_cstr(pipewire_state->event_arena, (U8 *) key);
    event->metadata_type   = str8_copy_cstr(pipewire_state->event_arena, (U8 *) type);
    event->metadata_value  = str8_copy_cstr(pipewire_state->event_arena, (U8 *) value);

    return 0;
}



// NOTE(simon): Module listeners.

internal Void pipewire_module_info(Void *data, const struct pw_module_info *info) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;
    info = entity->info = pw_module_info_merge(entity->info, info, !entity->changed);
    entity->changed |= info->change_mask != 0;

    if (info->change_mask & PW_MODULE_CHANGE_MASK_PROPS) {
        pipewire_event_list_push_properties(pipewire_state->event_arena, &pipewire_state->events, entity->id, info->props);
    }
}



// NOTE(simon): Node listeners.

internal Void pipewire_node_info(Void *data, const struct pw_node_info *info) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;
    info = entity->info = pw_node_info_merge(entity->info, info, !entity->changed);
    entity->changed |= info->change_mask != 0;

    if (info->change_mask & PW_NODE_CHANGE_MASK_PROPS) {
        pipewire_event_list_push_properties(pipewire_state->event_arena, &pipewire_state->events, entity->id, info->props);
    }

    if (info->change_mask & PW_NODE_CHANGE_MASK_PARAMS) {
        for (U32 i = 0; i < info->n_params; ++i) {
            U32 id = info->params[i].id;

            // NOTE(simon): Set by pw_node_info_merge for new/updated
            // parameters. Only enumerate paramters that have changed.
            if (info->params[i].user == 0) {
                continue;
            }

            Pipewire_Event *event = pipewire_event_list_push(pipewire_state->event_arena, &pipewire_state->events);
            event->kind = Pipewire_EventKind_UpdateParameter;
            event->id = entity->id;
            event->parameter_id = id;
            event->parameter_flags = info->params[i].flags;

            // NOTE(simon): No purpose in requesting parameters that we cannot read.
            if (info->params[i].flags & SPA_PARAM_INFO_READ) {
                int result = pw_node_enum_params((struct pw_node *) entity->proxy, ++info->params[i].seq, id, 0, U32_MAX, 0);
                if (SPA_RESULT_IS_ASYNC(result)) {
                    info->params[i].seq = result;
                }
                event->parameter_sequence = info->params[i].seq;
            }
        }
    }

    // TODO(simon): Unlikely that we change multiple times within one sync
    // period, but it is possible and this would generate extra churn.
    if (entity->changed) {
        pipewire_synchronize();
    }
}

// TODO(simon): Possibly fold pipewire_device_param, pipewire_node_param, pipewire_port_param and into one function.
internal Void pipewire_node_param(Void *data, S32 sequence, U32 id, U32 index, U32 next, const struct spa_pod *param) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;

    Pipewire_Event *event = 0;
    for (Pipewire_Event *candidate = pipewire_state->events.first; candidate; candidate = candidate->next) {
        if (
            candidate->kind == Pipewire_EventKind_UpdateParameter &&
            candidate->id == entity->id &&
            candidate->parameter_id == id &&
            candidate->parameter_sequence == sequence
        ) {
            event = candidate;
            break;
        }
    }

    if (event) {
        U64 size = SPA_POD_SIZE(param);
        Pipewire_ParameterNode *node = arena_push_struct(pipewire_state->event_arena, Pipewire_ParameterNode);
        node->parameter = (struct spa_pod *) arena_push_array(pipewire_state->event_arena, U8, size);
        memory_copy(node->parameter, param, size);
        sll_queue_push(event->first_parameter, event->last_parameter, node);
        ++event->parameter_count;
    }
}



// NOTE(simon): Port listeners.

internal Void pipewire_port_info(Void *data, const struct pw_port_info *info) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;
    info = entity->info = pw_port_info_merge(entity->info, info, !entity->changed);
    entity->changed |= info->change_mask != 0;

    if (info->change_mask & PW_PORT_CHANGE_MASK_PROPS) {
        pipewire_event_list_push_properties(pipewire_state->event_arena, &pipewire_state->events, entity->id, info->props);
    }

    if (info->change_mask & PW_PORT_CHANGE_MASK_PARAMS) {
        for (U32 i = 0; i < info->n_params; ++i) {
            U32 id = info->params[i].id;

            // NOTE(simon): Set by pw_port_info_merge for new/updated
            // parameters. Only enumerate paramters that have changed.
            if (info->params[i].user == 0) {
                continue;
            }

            Pipewire_Event *event = pipewire_event_list_push(pipewire_state->event_arena, &pipewire_state->events);
            event->kind = Pipewire_EventKind_UpdateParameter;
            event->id = entity->id;
            event->parameter_id = id;
            event->parameter_flags = info->params[i].flags;

            // NOTE(simon): No purpose in requesting parameters that we cannot read.
            if (info->params[i].flags & SPA_PARAM_INFO_READ) {
                int result = pw_port_enum_params((struct pw_port *) entity->proxy, ++info->params[i].seq, id, 0, U32_MAX, 0);
                if (SPA_RESULT_IS_ASYNC(result)) {
                    info->params[i].seq = result;
                }
                event->parameter_sequence = info->params[i].seq;
            }
        }
    }

    // TODO(simon): Unlikely that we change multiple times within one sync
    // period, but it is possible and this would generate extra churn.
    if (entity->changed) {
        pipewire_synchronize();
    }
}

// TODO(simon): Possibly fold pipewire_device_param, pipewire_node_param, pipewire_port_param and into one function.
internal Void pipewire_port_param(Void *data, S32 sequence, U32 id, U32 index, U32 next, const struct spa_pod *param) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;

    Pipewire_Event *event = 0;
    for (Pipewire_Event *candidate = pipewire_state->events.first; candidate; candidate = candidate->next) {
        if (
            candidate->kind == Pipewire_EventKind_UpdateParameter &&
            candidate->id == entity->id &&
            candidate->parameter_id == id &&
            candidate->parameter_sequence == sequence
        ) {
            event = candidate;
            break;
        }
    }

    if (event) {
        U64 size = SPA_POD_SIZE(param);
        Pipewire_ParameterNode *node = arena_push_struct(pipewire_state->event_arena, Pipewire_ParameterNode);
        node->parameter = (struct spa_pod *) arena_push_array(pipewire_state->event_arena, U8, size);
        memory_copy(node->parameter, param, size);
        sll_queue_push(event->first_parameter, event->last_parameter, node);
        ++event->parameter_count;
    }
}



// NOTE(simon): Registry listeners.

internal Void pipewire_registry_global(Void *data, U32 id, U32 permissions, const char *type, U32 version, const struct spa_dict *props) {
    typedef struct Interface Interface;
    struct Interface {
        char *type;
        U32 version;
        Pipewire_ObjectKind kind;
        Void *listener;
    };

    Interface interfaces[] = {
        { PW_TYPE_INTERFACE_Client,   PW_VERSION_CLIENT,   Pipewire_ObjectKind_Client,   &pipewire_client_listener,   },
        { PW_TYPE_INTERFACE_Device,   PW_VERSION_DEVICE,   Pipewire_ObjectKind_Device,   &pipewire_device_listener,   },
        { PW_TYPE_INTERFACE_Factory,  PW_VERSION_FACTORY,  Pipewire_ObjectKind_Factory,  &pipewire_factory_listener,  },
        { PW_TYPE_INTERFACE_Link,     PW_VERSION_LINK,     Pipewire_ObjectKind_Link,     &pipewire_link_listener,     },
        { PW_TYPE_INTERFACE_Metadata, PW_VERSION_METADATA, Pipewire_ObjectKind_Metadata, &pipewire_metadata_listener, },
        { PW_TYPE_INTERFACE_Module,   PW_VERSION_MODULE,   Pipewire_ObjectKind_Module,   &pipewire_module_listener,   },
        { PW_TYPE_INTERFACE_Node,     PW_VERSION_NODE,     Pipewire_ObjectKind_Node,     &pipewire_node_listener,     },
        { PW_TYPE_INTERFACE_Port,     PW_VERSION_PORT,     Pipewire_ObjectKind_Port,     &pipewire_port_listener,     },
    };

    // NOTE(simon): Find a matching interface.
    Interface *interface = 0;
    for (U64 i = 0; i < array_count(interfaces); ++i) {
        if (strcmp(interfaces[i].type, type) == 0 && interfaces[i].version <= version) {
            interface = &interfaces[i];
            break;
        }
    }

    // NOTE(simon): If we found a matching interface, create the entity.
    if (interface) {
        Pipewire_Entity *entity = pipewire_entity_allocate(id);
        entity->kind = interface->kind;
        entity->proxy = pw_registry_bind(pipewire_state->registry, id, type, interface->version, 0);
        pw_proxy_add_object_listener(entity->proxy, &entity->object_listener, interface->listener, entity);

        // NOTE(simon): Send Create event.
        Pipewire_Event *create_event = pipewire_event_list_push(pipewire_state->event_arena, &pipewire_state->events);
        create_event->kind        = Pipewire_EventKind_Create;
        create_event->object_kind = interface->kind;
        create_event->id          = entity->id;
        create_event->handle      = pipewire_handle_from_entity(entity);

        // NOTE(simon): Send UpdateProperties event.
        // TODO(simon): How exactly should these properties be interpreted? Do they mix with the object info messages?
        pipewire_event_list_push_properties(pipewire_state->event_arena, &pipewire_state->events, entity->id, (struct spa_dict *) props);

        // NOTE(simon): Since we added a new listener, there could be new
        // events to collect.
        pipewire_synchronize();
    }
}

internal Void pipewire_registry_global_remove(Void *data, U32 id) {
    Pipewire_Entity *entity = pipewire_entity_from_id(id);

    if (!pipewire_entity_is_nil(entity)) {
        // NOTE(simon): Send Destroy event.
        Pipewire_Event *event = pipewire_event_list_push(pipewire_state->event_arena, &pipewire_state->events);
        event->kind   = Pipewire_EventKind_Destroy;
        event->id     = entity->id;
        event->handle = pipewire_handle_from_entity(entity);

        pipewire_entity_free(entity);
    }
}
