// NOTE(simon): Entity allocation/freeing.

internal Pipewire_Entity *pipewire_entity_allocate(U32 id) {
    // NOTE(simon): Allocate.
    Pipewire_Entity *entity = pipewire_state->entity_freelist;
    U64 generation = 0;
    if (!pipewire_entity_is_nil(entity)) {
        generation = entity->generation + 1;
        sll_stack_pop(pipewire_state->entity_freelist);
        memory_zero(entity);
    } else {
        entity = arena_push_struct(pipewire_state->arena, Pipewire_Entity);
    }

    // NOTE(simon): Fill.
    entity->id = id;
    entity->generation = generation;

    // NOTE(simon): Insert into id -> entity map.
    U64 hash = u64_hash(id);
    Pipewire_EntityList *entity_bucket = &pipewire_state->entity_map[hash & (pipewire_state->entity_map_capacity - 1)];
    dll_push_back(entity_bucket->first, entity_bucket->last, entity);

    return entity;
}

internal Void pipewire_entity_free(Pipewire_Entity *entity) {
    if (!pipewire_entity_is_nil(entity)) {
        // NOTE(simon): Destroy handles.
        spa_hook_remove(&entity->object_listener);
        pw_proxy_destroy(entity->proxy);

        // NOTE(simon): Remove from id -> entity map.
        U64 hash = u64_hash(id);
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

    for (Pipewire_EntityNode *entity_node = entity_bucket->first; entity_node; entity_node = entity_node->next) {
        Pipewire_Entity *entity = entity_node->entity;
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

    for (Pipewire_EntityNode *entity_node = entity_bucket->first; entity_node; entity_node = entity_node->next) {
        Pipewire_Entity *entity = entity_node->entity;
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
    // TODO(simon): Verify that this is the correct way to handle the sequence numbers, because it feels wrong.
    pipewire_state->core_sequence = pw_core_sync(pipewire_state->core, PW_ID_CORE, pipewire_state->core_sequence);
}



// NOTE(simon): Startup/shutdown.

internal Void pipewire_init(Void) {
    Arena *arena = arena_create();
    pipewire_state = arena_push_struct(arena, Pipewire_State);
    pipewire_state->arena = arena;

    pw_init(0, 0);

    pipewire_state->loop     = pw_main_loop_new(0);
    pipewire_state->context  = pipewire_context_new(pw_main_loop_get_loop(pipewire_state->loop), 0, 0);
    pipewire_state->core     = pw_context_connect(pipewire_state->context, 0, 0);
    pipewire_state->registry = pw_core_get_registry(pipewire_state->core, PW_VERSION_REGISTRY, 0);

    pw_core_add_listener(pipewire_state->core, &pipewire_state->core_listener, &pipewire_core_listener, 0);
    pw_registry_add_listener(pipewire_state->registry, &pipewire_state->registry_listener, &pipewire_registry_listener, 0);

    pw_main_loop_run(pipewire_state->loop);
}

internal Void pipewire_deinit(Void) {
    spa_hook_remove(&pipewire_state->registry_listener);
    spa_hook_remove(&pipewire_state->core_listener);

    // TODO(simon): Destroy registry
    pw_registry_destroy(pipewire_state->regitry, );
    pw_core_disconnect(pipewire_state->core);
    pw_context_destroy(pipewire_state->context);
    pw_main_loop_destroy(pipewire_state->loop);

    pw_deinit();

    arena_destroy(pipewire_state->arena);
}



// NOTE(simon): Client listeners.

internal Void pipewire_client_info(Void *data, const struct pw_client_info *info) {
    if (info->change_mask & PW_CLIENT_CHANGE_MASK_PROPS) {
        // TODO(simon): Replace all properties for user.
    }
}

internal Void pipewire_client_permissions(Void *data, U32 index, U32 n_permissions, const struct pw_permission *permissions) {
}



// NOTE(simon): Core listeners.

internal Void pipewire_core_done(Void *data, U32 id, S32 sequence) {
}

internal Void pipewire_core_error(Void *data, U32 id, S32 sequence) {
}



// NOTE(simon): Device listeners.

internal Void pipewire_device_info(Void *data, const struct pw_device_info *info) {
}

internal Void pipewire_device_param(Void *data, S32 sequence, U32 id, U32 index, U32 next, const struct spa_pod *param) {
}



// NOTE(simon): Factory listeners.

internal Void pipewire_factory_info(Void *data, const struct pw_factory_info *info) {
    // TODO(simon): If first info event, signal name, type and version to user.

    if (info->change_mask & PW_FACTORY_CHANGE_MASK_PROPS) {
        // TODO(simon): Replace all properties for user.
    }
}



// NOTE(simon): Link listeners.

internal Void pipewire_link_info(Void *data, const struct pw_link_info *info) {
    // TODO(simon): If first info event, signal output_node_id, output_port_id,
    // input_node_id and input_port_id to user.

    if (info->change_mask & PW_LINK_CHANGE_MASK_STATE) {
        // TODO(simon): Update state and potentially error for user.
    }

    if (info->change_mask & PW_LINK_CHANGE_MASK_FORMAT) {
        // TODO(simon): Update format for user.
    }

    if (info->change_mask & PW_LINK_CHANGE_MASK_PROPS) {
        // TODO(simon): Replace all properties for user.
    }
}



// NOTE(simon): Metadata listeners.
internal S32 pipewire_metadata_property(Void *data, U32 subject, const char *key, const char *type, const char *value) {
    // TODO(simon): Do we store the metadata on the metadata object or do we broadcast it to the subject?
}



// NOTE(simon): Module listeners.

internal Void pipewire_module_info(Void *data, const struct pw_module_info *info) {
    // TODO(simon): If first info event, signal name, fiilename and args to user.

    if (info->change_mask & PW_MODULE_CHANGE_MASK_PROPS) {
        // TODO(simon): Replace all properties for user.
    }
}



// NOTE(simon): Node listeners.

internal Void pipewire_node_info(Void *data, const struct pw_node_info *info) {
    // TODO(simon): If first info event, signal max_input_ports and max_output_ports to user.

    if (info->change_mask & PW_NODE_CHANDE_MASK_INPUT_PORTS) {
        // TODO(simon): Signal input ports to user.
    }

    if (info->change_mask & PW_NODE_CHANDE_MASK_OUTPUT_PORTS) {
        // TODO(simon): Signal output ports to user.
    }

    if (info->change_mask & PW_NODE_CHANDE_MASK_STATE) {
        // TODO(simon): Update state and potentially error for user.
    }

    if (info->change_mask & PW_NODE_CHANDE_MASK_PROPS) {
        // TODO(simon): Replace all properties for user.
    }

    if (info->change_mask & PW_NODE_CHANDE_MASK_PARAMS) {
        // TODO(simon): Enumerate parameters.

        // NOTE(simon): Since we enumerate all changed parameters, there could
        // be new events to collect.
        pipewire_synchronize();
    }
}

internal Void pipewire_node_param(Void *data, S32 sequence, U32 id, U32 index, U32 next, const struct spa_pod *param) {
}



// NOTE(simon): Registry listeners.

internal Void pipewire_registry_global(Void *data, U32 id, U32 permissions, const char *type, U32 version, const struct spa_dict *props) {
    typedef struct Interface Interface;
    struct Interface {
        char *type;
        U32   version;
        Void *listener;
    };

    Interface interfaces[] = {
        { PW_TYPE_INTERFACE_Client,   PW_VERSION_CLIENT,   &pipewire_client_listener,   },
        { PW_TYPE_INTERFACE_Device,   PW_VERSION_DEVICE,   &pipewire_device_listener,   },
        { PW_TYPE_INTERFACE_Factory,  PW_VERSION_FACTORY,  &pipewire_factory_listener,  },
        { PW_TYPE_INTERFACE_Link,     PW_VERSION_LINK,     &pipewire_link_listener,     },
        { PW_TYPE_INTERFACE_Metadata, PW_VERSION_METADATA, &pipewire_metadata_listener, },
        { PW_TYPE_INTERFACE_Module,   PW_VERSION_MODULE,   &pipewire_module_listener,   },
        { PW_TYPE_INTERFACE_Node,     PW_VERSION_NODE,     &pipewire_node_listener,     },
        { PW_TYPE_INTERFACE_Port,     PW_VERSION_PORT,     &pipewire_port_listener,     },
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
    // TODO(simon): Signal object destruction to user.
    pipewire_entity_free(entity);
}
