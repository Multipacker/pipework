#ifndef PIPEWIRE_INCLUDE_H
#define PIPEWIRE_INCLUDE_H

#undef global
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wc2y-extensions"
#include <spa/utils/result.h>
#include <spa/debug/pod.h>
#include <pipewire/pipewire.h>
#include <pipewire/extensions/metadata.h>
#pragma clang diagnostic pop
#define global static

typedef struct Pipewire_Handle Pipewire_Handle;
struct Pipewire_Handle {
    U64 u64[2];
};

typedef enum {
    Pipewire_ObjectKind_Null,
    Pipewire_ObjectKind_Client,
    Pipewire_ObjectKind_Device,
    Pipewire_ObjectKind_Factory,
    Pipewire_ObjectKind_Link,
    Pipewire_ObjectKind_Metadata,
    Pipewire_ObjectKind_Module,
    Pipewire_ObjectKind_Node,
    Pipewire_ObjectKind_Port,
    Pipewire_ObjectKind_COUNT,
} Pipewire_ObjectKind;

typedef enum {
    Pipewire_EventKind_Create,
    Pipewire_EventKind_UpdateProperties,
    Pipewire_EventKind_UpdateParameter,
    Pipewire_EventKind_AddMetadata,
    Pipewire_EventKind_Destroy,
} Pipewire_EventKind;

typedef struct Pipewire_Property Pipewire_Property;
struct Pipewire_Property {
    Str8 key;
    Str8 value;
};

typedef struct Pipewire_ParameterNode Pipewire_ParameterNode;
struct Pipewire_ParameterNode {
    struct Pipewire_ParameterNode *next;
    struct spa_pod *parameter;
};

typedef struct Pipewire_Parameter Pipewire_Parameter;
struct Pipewire_Parameter {
    Pipewire_Parameter *next;
    Pipewire_Parameter *previous;
    U32 id;
    U32 flags;
    U64 count;
    struct spa_pod **parameters;
};

typedef struct Pipewire_Event Pipewire_Event;
struct Pipewire_Event {
    Pipewire_Event *next;

    Pipewire_EventKind kind;
    U32 id;

    // NOTE(simon): Create
    // TODO(simon): Do we really need a separate object kind or can we bake this into Pipewire_EventKind?
    Pipewire_ObjectKind object_kind;
    Pipewire_Handle     handle;

    // NOTE(simon): UpdateProperties
    U64 property_count;
    Pipewire_Property *properties;

    // NOTE(simon): UpdateParameter
    U32 parameter_id;
    U32 parameter_flags;
    S32 parameter_sequence;
    U32 parameter_count;
    Pipewire_ParameterNode *first_parameter;
    Pipewire_ParameterNode *last_parameter;

    // NOTE(simon): AddMetadata
    U32  metadata_issuer;
    Str8 metadata_key;
    Str8 metadata_type;
    Str8 metadata_value;
};

typedef struct Pipewire_EventList Pipewire_EventList;
struct Pipewire_EventList {
    Pipewire_Event *first;
    Pipewire_Event *last;
    U64 count;
};

typedef struct Pipewire_Metadata Pipewire_Metadata;
struct Pipewire_Metadata {
    Pipewire_Metadata *next;
    Pipewire_Metadata *previous;
    U32  issuer;
    Str8 key;
    Str8 type;
    Str8 value;
};

typedef struct Pipewire_Object Pipewire_Object;
struct Pipewire_Object {
    Pipewire_Object *next;
    Pipewire_Object *previous;

    Pipewire_ObjectKind kind;

    U32 id;
    U64 generation;

    Pipewire_Handle entity;

    // NOTE(simon): State.

    // NOTE(simon): Properties.
    U64 property_count;
    Pipewire_Property *properties;

    // NOTE(simon): Parameters.
    Pipewire_Parameter *first_parameter;
    Pipewire_Parameter *last_parameter;

    // NOTE(simon): Metadata.
    Pipewire_Metadata *first_metadata;
    Pipewire_Metadata *last_metadata;
};

global Pipewire_Object pipewire_object_nil = { 0 };

typedef struct Pipewire_ObjectList Pipewire_ObjectList;
struct Pipewire_ObjectList {
    Pipewire_Object *first;
    Pipewire_Object *last;
};

typedef struct Pipewire_ChunkNode Pipewire_ChunkNode;
struct Pipewire_ChunkNode {
    Pipewire_ChunkNode *next;
    U64 size;
};

global U64 pipewire_chunk_sizes[] = {
    16,
    64,
    256,
    1024,
    4096,
    16384,
    0xFFFFFFFFFFFFFFFF,
};

typedef struct Pipewire_ObjectStore Pipewire_ObjectStore;
struct Pipewire_ObjectStore {
    // NOTE(simon): Allocators.
    Arena *arena;
    Pipewire_Object    *object_freelist;
    Pipewire_Parameter *parameter_freelist;
    Pipewire_Metadata  *metadata_freelist;
    Pipewire_ChunkNode *chunk_freelist[array_count(pipewire_chunk_sizes)];

    // NOTE(simon): Pipewire id -> object map.
    Pipewire_ObjectList *object_map;
    U64 object_map_capacity;
};

typedef struct Pipewire_Entity Pipewire_Entity;
struct Pipewire_Entity {
    // NOTE(simon): Hashmap links.
    Pipewire_Entity *next;
    Pipewire_Entity *previous;

    Pipewire_ObjectKind kind;

    // NOTE(simon): Identifier from Pipewire
    U32 id;

    // NOTE(simon): Generation because I don't know the exact guarantees about
    // Pipewires IDs.
    U64 generation;

    // NOTE(simon): Pipewire handle.
    struct pw_proxy *proxy;

    // NOTE(simon): Listener handle.
    struct spa_hook object_listener;

    // NOTE(simon): Low-level state tracking.
    B8    changed;
    Void *info;
};

global Pipewire_Entity pipewire_entity_nil = { 0 };

typedef struct Pipewire_EntityList Pipewire_EntityList;
struct Pipewire_EntityList {
    Pipewire_Entity *first;
    Pipewire_Entity *last;
};

typedef struct Pipewire_State Pipewire_State;
struct Pipewire_State {
    // NOTE(simon): Allocators.
    Arena *arena;
    Pipewire_Entity *entity_freelist;

    // NOTE(simon): Core Pipewire handles.
    struct pw_main_loop *loop;
    struct pw_context   *context;
    struct pw_core      *core;
    struct pw_registry  *registry;

    // NOTE(simon): Listener handles.
    struct spa_hook core_listener;
    struct spa_hook registry_listener;

    // NOTE(simon): Id to determine when we are completely synchronized with
    // the core.
    S32 core_sequence;

    // NOTE(simon): Pipewire id -> entity map.
    Pipewire_EntityList *entity_map;
    U64 entity_map_capacity;

    Arena *event_arena;
    Pipewire_EventList events;

    Pipewire_ObjectStore *store;
};

global Pipewire_State *pipewire_state;

internal Str8 pipewire_string_from_object_kind(Pipewire_ObjectKind kind);

// NOTE(simon): Events.
internal Pipewire_Event *pipewire_event_list_push(Arena *arena, Pipewire_EventList *list);
internal Pipewire_Event *pipewire_event_list_push_properties(Arena *arena, Pipewire_EventList *list, U32 id, struct spa_dict *properties);

// NOTE(simon): Entity allocation/freeing.
internal Pipewire_Entity *pipewire_entity_allocate(U32 id);
internal Void             pipewire_entity_free(Pipewire_Entity *entity);

// NOTE(simon): Entity <-> handle.
internal Pipewire_Entity *pipewire_entity_from_handle(Pipewire_Handle handle);
internal Pipewire_Entity *pipewire_entity_from_id(U32 id);
internal Pipewire_Handle  pipewire_handle_from_entity(Pipewire_Entity *entity);
internal B32              pipewire_entity_is_nil(Pipewire_Entity *entity);

internal Void pipewire_synchronize(Void);

// NOTE(simon): Startup/shutdown.
internal Void pipewire_init(Void);
internal Void pipewire_deinit(Void);



// NOTE(simon): Client listeners.
internal Void pipewire_client_info(Void *data, const struct pw_client_info *info);
internal Void pipewire_client_permissions(Void *data, U32 index, U32 n_permissions, const struct pw_permission *permissions);
global struct pw_client_events pipewire_client_listener = {
    PW_VERSION_CLIENT_EVENTS,
    .info        = pipewire_client_info,
    .permissions = pipewire_client_permissions,
};

// NOTE(simon): Core listeners.
// TODO(simon): There are more events here, are they interesting?
internal Void pipewire_core_done(Void *data, U32 id, S32 sequence);
internal Void pipewire_core_error(Void *data, U32 id, S32 sequence, S32 res, const char *message);
global struct pw_core_events pipewire_core_listener = {
    PW_VERSION_CORE_EVENTS,
    .done  = pipewire_core_done,
    .error = pipewire_core_error,
};

// NOTE(simon): Device listeners.
internal Void pipewire_device_info(Void *data, const struct pw_device_info *info);
internal Void pipewire_device_param(Void *data, S32 sequence, U32 id, U32 index, U32 next, const struct spa_pod *param);
global struct pw_device_events pipewire_device_listener = {
    PW_VERSION_DEVICE_EVENTS,
    .info  = pipewire_device_info,
    .param = pipewire_device_param,
};

// NOTE(simon): Factory listeners.
internal Void pipewire_factory_info(Void *data, const struct pw_factory_info *info);
global struct pw_factory_events pipewire_factory_listener = {
    PW_VERSION_FACTORY_EVENTS,
    .info = pipewire_factory_info,
};

// NOTE(simon): Link listeners.
internal Void pipewire_link_info(Void *data, const struct pw_link_info *info);
global struct pw_link_events pipewire_link_listener = {
    PW_VERSION_LINK_EVENTS,
    .info = pipewire_link_info,
};

// NOTE(simon): Metadata listeners.
internal S32 pipewire_metadata_property(Void *data, U32 subject, const char *key, const char *type, const char *value);
global struct pw_metadata_events pipewire_metadata_listener = {
    PW_VERSION_METADATA_EVENTS,
    .property = pipewire_metadata_property,
};

// NOTE(simon): Module listeners.
internal Void pipewire_module_info(Void *data, const struct pw_module_info *info);
global struct pw_module_events pipewire_module_listener = {
    PW_VERSION_MODULE_EVENTS,
    .info = pipewire_module_info,
};

// NOTE(simon): Node listeners.
internal Void pipewire_node_info(Void *data, const struct pw_node_info *info);
internal Void pipewire_node_param(Void *data, S32 sequence, U32 id, U32 index, U32 next, const struct spa_pod *param);
global struct pw_node_events pipewire_node_listener = {
    PW_VERSION_NODE_EVENTS,
    .info  = pipewire_node_info,
    .param = pipewire_node_param,
};

// NOTE(simon): Port listeners.
internal Void pipewire_port_info(Void *data, const struct pw_port_info *info);
internal Void pipewire_port_param(Void *data, S32 sequence, U32 id, U32 index, U32 next, const struct spa_pod *param);
global struct pw_port_events pipewire_port_listener = {
    PW_VERSION_PORT_EVENTS,
    .info  = pipewire_port_info,
    .param = pipewire_port_param,
};

// NOTE(simon): Registry listeners.
internal Void pipewire_registry_global(Void *data, U32 id, U32 permissions, const char *type, U32 version, const struct spa_dict *props);
internal Void pipewire_registry_global_remove(Void *data, U32 id);
global struct pw_registry_events pipewire_registry_listener = {
#undef global
    PW_VERSION_REGISTRY_EVENTS,
    .global        = pipewire_registry_global,
    .global_remove = pipewire_registry_global_remove,
#define global static
};

#endif // PIPEWIRE_INCLUDE_H
