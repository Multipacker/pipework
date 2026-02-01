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



// NOTE(simon): Object store.

internal B32 pipewire_object_is_nil(Pipewire_Object *object) {
    B32 result = !object || object == &pipewire_object_nil;
    return result;
}

internal Pipewire_ObjectStore *pipewire_object_store_create(Void) {
    Arena *arena = arena_create();
    Pipewire_ObjectStore *store = arena_push_struct(arena, Pipewire_ObjectStore);
    store->arena = arena;

    store->object_map_capacity = 1024;
    store->object_map = arena_push_array(store->arena, Pipewire_ObjectList, store->object_map_capacity);

    return store;
}

internal Void pipewire_object_store_destroy(Pipewire_ObjectStore *store) {
    arena_destroy(store->arena);
}

internal Pipewire_Object *pipewire_object_store_object_from_id(Pipewire_ObjectStore *store, U32 id) {
    Pipewire_Object *result = &pipewire_object_nil;

    U64 hash = u64_hash(id);
    Pipewire_ObjectList *object_bucket = &store->object_map[hash & (store->object_map_capacity - 1)];
    for (Pipewire_Object *object = object_bucket->first; object; object = object->next) {
        if (object->id == id) {
            result = object;
            break;
        }
    }

    return result;
}

internal Void pipewire_object_store_apply_events(Pipewire_ObjectStore *store, Pipewire_EventList events) {
    for (Pipewire_Event *event = events.first; event; event = event->next) {
        switch (event->kind) {
            case Pipewire_EventKind_Create: {
                // NOTE(simon): Allocate.
                Pipewire_Object *object = store->object_freelist;
                U64 generation = 0;
                if (object) {
                    generation = object->generation + 1;
                    sll_stack_pop(store->object_freelist);
                    memory_zero_struct(object);
                } else {
                    object = arena_push_struct(store->arena, Pipewire_Object);
                }

                // NOTE(simon): Fill.
                object->id         = event->id;
                object->generation = generation;
                object->kind       = event->object_kind;
                object->entity     = event->handle;

                // NOTE(simon): Insert into id -> object map.
                U64 hash = u64_hash(object->id);
                Pipewire_ObjectList *object_bucket = &store->object_map[hash & (store->object_map_capacity - 1)];
                dll_push_back(object_bucket->first, object_bucket->last, object);
            } break;
            case Pipewire_EventKind_UpdateProperties: {
            } break;
            case Pipewire_EventKind_UpdateParameter: {
            } break;
            case Pipewire_EventKind_AddMetadata: {
            } break;
            case Pipewire_EventKind_Destroy: {
                Pipewire_Object *object = pipewire_object_store_object_from_id(store, event->id);

                if (!pipewire_object_is_nil(object)) {
                    // TODO(simon): Free associated resources.

                    // NOTE(simon): Remove from id -> object map.
                    U64 hash = u64_hash(object->id);
                    Pipewire_ObjectList *object_bucket = &store->object_map[hash & (store->object_map_capacity - 1)];
                    dll_remove(object_bucket->first, object_bucket->last, object);

                    // NOTE(simon): Add to freelist.
                    sll_stack_push(store->object_freelist, object);
                }
            } break;
        }
    }
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

    pipewire_state->entity_map_capacity = 1024;
    pipewire_state->entity_map = arena_push_array(pipewire_state->arena, Pipewire_EntityList, pipewire_state->entity_map_capacity);

    pipewire_state->store = pipewire_object_store_create();

    pw_init(0, 0);

    pipewire_state->loop     = pw_main_loop_new(0);
    pipewire_state->context  = pw_context_new(pw_main_loop_get_loop(pipewire_state->loop), 0, 0);
    pipewire_state->core     = pw_context_connect(pipewire_state->context, 0, 0);
    pipewire_state->registry = pw_core_get_registry(pipewire_state->core, PW_VERSION_REGISTRY, 0);

    pw_core_add_listener(pipewire_state->core, &pipewire_state->core_listener, &pipewire_core_listener, 0);
    pw_registry_add_listener(pipewire_state->registry, &pipewire_state->registry_listener, &pipewire_registry_listener, 0);

    pw_main_loop_run(pipewire_state->loop);
}

internal Void pipewire_deinit(Void) {
    pipewire_object_store_destroy(pipewire_state->store);

    // NOTE(simon): Destroy all entities.
    for (U64 i = 0; i < pipewire_state->entity_map_capacity; ++i) {
        Pipewire_EntityList *entity_bucket = &pipewire_state->entity_map[i];
        for (Pipewire_Entity *entity = entity_bucket->first, *next = 0; entity; entity = next) {
            next = entity->next;
            pipewire_entity_free(entity);
        }
    }

    // NOTE(simon): Unhook all listeners.
    spa_hook_remove(&pipewire_state->registry_listener);
    spa_hook_remove(&pipewire_state->core_listener);

    // NOTE(simon): Destroy all handles.
    pw_proxy_destroy((struct pw_proxy *) pipewire_state->registry);
    pw_core_disconnect(pipewire_state->core);
    pw_context_destroy(pipewire_state->context);
    pw_main_loop_destroy(pipewire_state->loop);

    pw_deinit();

    arena_destroy(pipewire_state->arena);
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

    // NOTE(simon): Generate change events.
    for (U64 i = 0; i < pipewire_state->entity_map_capacity; ++i) {
        Pipewire_EntityList *bucket = &pipewire_state->entity_map[i];
        for (Pipewire_Entity *entity = bucket->first; entity; entity = entity->next) {
            entity->changed = false;
        }
    }

    pipewire_object_store_apply_events(pipewire_state->store, pipewire_state->events);

    printf("Events:\n");
    for (Pipewire_Event *event = pipewire_state->events.first; event; event = event->next) {
        switch (event->kind) {
            case Pipewire_EventKind_Create: {
                Str8 kind = pipewire_string_from_object_kind(event->object_kind);
                printf("  %u.Create %.*s\n", event->id, str8_expand(kind));
            } break;
            case Pipewire_EventKind_UpdateProperties: {
                printf("  %u.UpdateProperties\n", event->id);
                for (U64 i = 0; i < event->property_count; ++i) {
                    printf("    %.*s = %.*s\n", str8_expand(event->properties[i].key), str8_expand(event->properties[i].value));
                }
            } break;
            case Pipewire_EventKind_UpdateParameter: {
                printf("  %u.UpdateParameter %u%s%s\n", event->id, event->parameter_id, event->parameter_flags & SPA_PARAM_INFO_READ ? " Read" : "", event->parameter_flags & SPA_PARAM_INFO_WRITE ? " Write" : "");
                for (Pipewire_ParameterNode *node = event->first_parameter; node; node = node->next) {
                    spa_debug_pod(4, 0, (const struct spa_pod *) node->parameter);
                }
            } break;
            case Pipewire_EventKind_AddMetadata: {
                printf(
                    "  %u.AddMetadata %u.%.*s: %.*s = %.*s \n",
                    event->id,
                    event->metadata_issuer,
                    str8_expand(event->metadata_key),
                    str8_expand(event->metadata_type),
                    str8_expand(event->metadata_value)
                );
            } break;
            case Pipewire_EventKind_Destroy: {
                printf("  %u.Destroy\n", event->id);
            } break;
        }
    }

    memory_zero_struct(&pipewire_state->events);
    arena_reset(pipewire_state->event_arena);
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
