internal Pipewire_Event *pipewire_event_list_push(Arena *arena, Pipewire_EventList *list) {
    Pipewire_Event *event = arena_push_struct(arena, Pipewire_Event);
    sll_queue_push(list->first, list->last, event);
    ++list->count;
    return event;
}

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
    entity->created    = true;

    // NOTE(simon): Insert into id -> entity map.
    U64 hash = u64_hash(id);
    Pipewire_EntityList *entity_bucket = &pipewire_state->entity_map[hash & (pipewire_state->entity_map_capacity - 1)];
    dll_insert_next_previous_zero(entity_bucket->first, entity_bucket->last, entity_bucket->last, entity, hash_next, hash_previous, 0);
    dll_push_back(pipewire_state->first_entity, pipewire_state->last_entity, entity);

    return entity;
}

internal Void pipewire_entity_free(Pipewire_Entity *entity) {
    if (!pipewire_entity_is_nil(entity)) {
        if (entity->info) {
            free(entity->info);
        }

        // NOTE(simon): Unhook listeners.
        spa_hook_remove(&entity->object_listener);

        // NOTE(simon): Destroy handles.
        pw_proxy_destroy(entity->proxy);

        // NOTE(simon): Remove from id -> entity map.
        U64 hash = u64_hash(entity->id);
        Pipewire_EntityList *entity_bucket = &pipewire_state->entity_map[hash & (pipewire_state->entity_map_capacity - 1)];
        dll_remove_next_previous_zero(entity_bucket->first, entity_bucket->last, entity, hash_next, hash_previous, 0);
        dll_remove(pipewire_state->first_entity, pipewire_state->last_entity, entity);

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

    for (Pipewire_Entity *entity = entity_bucket->first; entity; entity = entity->hash_next) {
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

    for (Pipewire_Entity *entity = entity_bucket->first; entity; entity = entity->hash_next) {
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

    pipewire_state->entity_map_capacity = 1024;
    pipewire_state->entity_map = arena_push_array(pipewire_state->arena, Pipewire_EntityList, pipewire_state->entity_map_capacity);

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
}

internal Void pipewire_client_permissions(Void *data, U32 index, U32 n_permissions, const struct pw_permission *permissions) {
}



// NOTE(simon): Core listeners.

internal Void pipewire_core_done(Void *data, U32 id, S32 sequence) {
    if (id != PW_ID_CORE || sequence != pipewire_state->core_sequence) {
        return;
    }

    Arena_Temporary scratch = arena_get_scratch(0, 0);
    Pipewire_EventList event_list = { 0 };

    // NOTE(simon): Generate destroy events.
    for (Pipewire_Entity *entity = pipewire_state->first_entity, *next = 0; entity; entity = next) {
        next = entity->next;

        if (!entity->deleted) {
            continue;
        }

        Pipewire_Event *event = pipewire_event_list_push(scratch.arena, &event_list);
        event->kind   = Pipewire_EventKind_Destroy;
        event->id     = entity->id;
        event->handle = pipewire_handle_from_entity(entity);;

        pipewire_entity_free(entity);
    }

    // NOTE(simon): Generate create events.
    for (Pipewire_Entity *entity = pipewire_state->first_entity, *next = 0; entity; entity = next) {
        if (!entity->created) {
            continue;
        }

        Pipewire_Event *event = pipewire_event_list_push(scratch.arena, &event_list);
        event->kind        = Pipewire_EventKind_Create;
        event->object_kind = entity->kind;
        event->id          = entity->id;
        event->handle      = pipewire_handle_from_entity(entity);;

        entity->created = false;
    }

    // NOTE(simon): Generate change events.
    for (Pipewire_Entity *entity = pipewire_state->first_entity, *next = 0; entity; entity = next) {
        if (!entity->changed) {
            continue;
        }

        // TODO(simon): Generate change events.
        entity->changed = false;
    }

    printf("Events:\n");
    for (Pipewire_Event *event = event_list.first; event; event = event->next) {
        Str8 kind = pipewire_string_from_object_kind(event->object_kind);
        switch (event->kind) {
            case Pipewire_EventKind_Create: {
                printf("\tCreate %u %.*s\n", event->id, str8_expand(kind));
            } break;
            case Pipewire_EventKind_Destroy: {
                printf("\tDestroy %u %.*s\n", event->id, str8_expand(kind));
            } break;
        }
    }

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

    if (info->change_mask & PW_DEVICE_CHANGE_MASK_PARAMS) {
        for (U32 i = 0; i < info->n_params; ++i) {
            U32 id = info->params[i].id;

            // NOTE(simon): Set by pw_device_info_merge for new/updated
            // parameters. Only enumerate paramters that have changed.
            if (info->params[i].user == 0) {
                continue;
            }

            // TODO(simon): Clear previous parameters with this id.

            // NOTE(simon): No purpose in requesting parameters that we cannot read.
            if (info->params[i].flags & SPA_PARAM_INFO_READ) {
                int result = pw_device_enum_params((struct pw_device *) entity->proxy, ++info->params[i].seq, id, 0, U32_MAX, 0);
                if (SPA_RESULT_IS_ASYNC(result)) {
                    info->params[i].seq = result;
                }
            }
        }
    }

    // TODO(simon): Unlikely that we change multiple times within one sync
    // period, but it is possible and this would generate extra churn.
    if (entity->changed) {
        pipewire_synchronize();
    }
}

internal Void pipewire_device_param(Void *data, S32 sequence, U32 id, U32 index, U32 next, const struct spa_pod *param) {
    // TODO(simon): Register the parameter.
}



// NOTE(simon): Factory listeners.

internal Void pipewire_factory_info(Void *data, const struct pw_factory_info *info) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;
    info = entity->info = pw_factory_info_merge(entity->info, info, !entity->changed);
    entity->changed |= info->change_mask != 0;
}



// NOTE(simon): Link listeners.

internal Void pipewire_link_info(Void *data, const struct pw_link_info *info) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;
    info = entity->info = pw_link_info_merge(entity->info, info, !entity->changed);
    entity->changed |= info->change_mask != 0;
}



// NOTE(simon): Metadata listeners.
internal S32 pipewire_metadata_property(Void *data, U32 subject, const char *key, const char *type, const char *value) {
    // TODO(simon): Do we store the metadata on the metadata object or do we broadcast it to the subject?
    return 0;
}



// NOTE(simon): Module listeners.

internal Void pipewire_module_info(Void *data, const struct pw_module_info *info) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;
    info = entity->info = pw_module_info_merge(entity->info, info, !entity->changed);
    entity->changed |= info->change_mask != 0;
}



// NOTE(simon): Node listeners.

internal Void pipewire_node_info(Void *data, const struct pw_node_info *info) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;
    info = entity->info = pw_node_info_merge(entity->info, info, !entity->changed);
    entity->changed |= info->change_mask != 0;

    if (info->change_mask & PW_NODE_CHANGE_MASK_PARAMS) {
        for (U32 i = 0; i < info->n_params; ++i) {
            U32 id = info->params[i].id;

            // NOTE(simon): Set by pw_node_info_merge for new/updated
            // parameters. Only enumerate paramters that have changed.
            if (info->params[i].user == 0) {
                continue;
            }

            // TODO(simon): Clear previous parameters with this id.

            // NOTE(simon): No purpose in requesting parameters that we cannot read.
            if (info->params[i].flags & SPA_PARAM_INFO_READ) {
                int result = pw_node_enum_params((struct pw_node *) entity->proxy, ++info->params[i].seq, id, 0, U32_MAX, 0);
                if (SPA_RESULT_IS_ASYNC(result)) {
                    info->params[i].seq = result;
                }
            }
        }
    }

    // TODO(simon): Unlikely that we change multiple times within one sync
    // period, but it is possible and this would generate extra churn.
    if (entity->changed) {
        pipewire_synchronize();
    }
}

internal Void pipewire_node_param(Void *data, S32 sequence, U32 id, U32 index, U32 next, const struct spa_pod *param) {
    // TODO(simon): Register the parameter.
}



// NOTE(simon): Port listeners.

internal Void pipewire_port_info(Void *data, const struct pw_port_info *info) {
    Pipewire_Entity *entity = (Pipewire_Entity *) data;
    info = entity->info = pw_port_info_merge(entity->info, info, !entity->changed);
    entity->changed |= info->change_mask != 0;

    if (info->change_mask & PW_PORT_CHANGE_MASK_PARAMS) {
        for (U32 i = 0; i < info->n_params; ++i) {
            U32 id = info->params[i].id;

            // NOTE(simon): Set by pw_port_info_merge for new/updated
            // parameters. Only enumerate paramters that have changed.
            if (info->params[i].user == 0) {
                continue;
            }

            // TODO(simon): Clear previous parameters with this id.

            // NOTE(simon): No purpose in requesting parameters that we cannot read.
            if (info->params[i].flags & SPA_PARAM_INFO_READ) {
                int result = pw_port_enum_params((struct pw_port *) entity->proxy, ++info->params[i].seq, id, 0, U32_MAX, 0);
                if (SPA_RESULT_IS_ASYNC(result)) {
                    info->params[i].seq = result;
                }
            }
        }
    }

    // TODO(simon): Unlikely that we change multiple times within one sync
    // period, but it is possible and this would generate extra churn.
    if (entity->changed) {
        pipewire_synchronize();
    }
}

internal Void pipewire_port_param(Void *data, S32 sequence, U32 id, U32 index, U32 next, const struct spa_pod *param) {
    // TODO(simon): Register the parameter.
}



// NOTE(simon): Registry listeners.

internal Void pipewire_registry_global(Void *data, U32 id, U32 permissions, const char *type, U32 version, const struct spa_dict *props) {
    printf("global %s %u\n", type, id);
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

        // TODO(simon): Signal object creation to user.

        // TODO(simon): How exactly should these properties be interpreted? Do they mix with the object info messages?
        const struct spa_dict_item *property = 0;
        spa_dict_for_each(property, props) {
            // TODO(simon): Singal property to user.
        }

        // NOTE(simon): Since we added a new listener, there could be new
        // events to collect.
        pipewire_synchronize();
    }
}

internal Void pipewire_registry_global_remove(Void *data, U32 id) {
    Pipewire_Entity *entity = pipewire_entity_from_id(id);
    if (!pipewire_entity_is_nil(entity)) {
        entity->deleted = true;
    }
}
