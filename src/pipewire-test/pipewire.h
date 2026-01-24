#ifndef PIPEWIRE_INCLUDE_H
#define PIPEWIRE_INCLUDE_H

#undef global
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wc2y-extensions"
#include <spa/param/format-utils.h>
#include <spa/utils/result.h>
#include <spa/debug/pod.h>
#include <spa/pod/builder.h>
#include <pipewire/pipewire.h>
#pragma clang diagnostic pop
#define global static

typedef struct Pipewire_Handle Pipewire_Handle;
struct Pipewire_Handle {
    U64 u64[2];
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

typedef struct Pipewire_ChunkNode Pipewire_ChunkNode;
struct Pipewire_ChunkNode {
    Pipewire_ChunkNode *next;
    U64 size;
};

typedef struct Pipewire_Property Pipewire_Property;
struct Pipewire_Property {
    Pipewire_Property *next;
    Pipewire_Property *previous;
    Str8 name;
    Str8 value;
};

global Pipewire_Property pipewire_nil_property = { 0 };

typedef struct Pipewire_Parameter Pipewire_Parameter;
struct Pipewire_Parameter {
    Pipewire_Parameter *next;
    Pipewire_Parameter *previous;
    U32 id;
    struct spa_pod *param;
};

global Pipewire_Parameter pipewire_nil_parameter = { 0 };

typedef enum {
    Pipewire_Object_Null,
    Pipewire_Object_Module,
    Pipewire_Object_Factory,
    Pipewire_Object_Client,
    Pipewire_Object_Device,
    Pipewire_Object_Node,
    Pipewire_Object_Port,
    Pipewire_Object_Link,
    Pipewire_Object_COUNT,
} Pipewire_ObjectKind;

typedef struct Pipewire_Object Pipewire_Object;
struct Pipewire_Object {
    Pipewire_Object *all_next;
    Pipewire_Object *all_previous;
    U64              generation;

    Pipewire_Object *parent;
    Pipewire_Object *first;
    Pipewire_Object *last;
    Pipewire_Object *next;
    Pipewire_Object *previous;

    Pipewire_ObjectKind kind;
    U32 id;

    Pipewire_Property *first_property;
    Pipewire_Property *last_property;

    Pipewire_Parameter *first_parameter;
    Pipewire_Parameter *last_parameter;

    struct pw_proxy *proxy;
    struct spa_hook  listener;

    // NOTE(simon): Low-level data storage.
    B32   changed;
    Void *info;
    U32   param_count;
    struct spa_param_info *params;
};

global Pipewire_Object pipewire_nil_object = {
    .parent   = &pipewire_nil_object,
    .first    = &pipewire_nil_object,
    .last     = &pipewire_nil_object,
    .next     = &pipewire_nil_object,
    .previous = &pipewire_nil_object,
};

// TODO(simon): What is the maximum number of channels?
typedef struct Pipewire_Volume Pipewire_Volume;
struct Pipewire_Volume {
    B32 mute;
    U32 channel_count;
    F32 channel_volumes[64];
    U32 channel_map[64];
    F32 volume_base;
    F32 volume_step;
};

typedef struct Pipewire_State Pipewire_State;
struct Pipewire_State {
    Arena *arena;

    Pipewire_Object *first_object;
    Pipewire_Object *last_object;
    Pipewire_Object *object_freelist;
    Pipewire_ChunkNode *chunk_freelist[array_count(pipewire_chunk_sizes)];
    Pipewire_Property *property_freelist;
    Pipewire_Parameter *parameter_freelist;

    struct pw_main_loop *loop;
    struct pw_context   *context;
    struct pw_core      *core;
    struct spa_hook      core_listener;
    S32                  core_sequence;

    struct pw_registry  *registry;
    struct spa_hook      registry_listener;
};

internal B32 pipewire_object_is_nil(Pipewire_Object *object);

internal Void pipewire_add_child(Pipewire_Object *parent, Pipewire_Object *child);
internal Void pipewire_remove_child(Pipewire_Object *parent, Pipewire_Object *child);

internal Pipewire_Object *pipewire_create_object(U32 id);
internal Void             pipewire_destroy_object(Pipewire_Object *object);

internal B32                pipewire_property_is_nil(Pipewire_Property *property);
internal Void               pipewire_object_update_property(Pipewire_Object *object, Str8 name, Str8 value);
internal Pipewire_Property *pipewire_property_from_object_name(Pipewire_Object *object, Str8 name);
internal Str8               pipewire_property_string_from_object_name(Pipewire_Object *object, Str8 name);
internal U32                pipewire_property_u32_from_object_name(Pipewire_Object *object, Str8 name);
internal Pipewire_Object   *pipewire_property_object_from_object_name(Pipewire_Object *object, Str8 name);

internal B32  pipewire_parameter_is_nil(Pipewire_Parameter *parameter);
internal Void pipewire_object_remove_parameter(Pipewire_Object *object, U32 id);
internal Void pipewire_object_update_parameter(Pipewire_Object *object, S32 sequence, U32 id, struct spa_pod *param);

internal U64             pipewire_chunk_index_from_size(U64 size);
internal U8             *pipewire_allocate(U64 size);
internal Void            pipewire_free(U8 *data, U64 size);
internal Str8            pipewire_string_allocate(Str8 string);
internal Void            pipewire_string_free(Str8 string);
internal struct spa_pod *pipewire_spa_pod_allocate(struct spa_pod *pod);
internal Void            pipewire_spa_pod_free(struct spa_pod *pod);

internal Pipewire_Handle  pipewire_handle_from_object(Pipewire_Object *object);
internal Pipewire_Object *pipewire_object_from_handle(Pipewire_Handle handle);

internal Pipewire_Object *pipewire_object_from_id(U32 id);

internal Void pipewire_link(Pipewire_Handle output, Pipewire_Handle input);
internal Void pipewire_remove(Pipewire_Handle handle);

internal B32 pipewire_object_is_card(Pipewire_Object *object);

internal Pipewire_Volume pipewire_volume_from_node(Pipewire_Object *object);
internal Void            pipewire_set_node_volume(Pipewire_Object *object, Pipewire_Volume volume);

internal Void pipewire_init(Void);
internal Void pipewire_synchronize(Void);
internal Void pipewire_tick(Void);
internal Void pipewire_deinit(Void);

internal U64 pipewire_spa_pod_min_type_size(U32 type);



internal Void pipewire_module_info(Void *data, const struct pw_module_info *info);

global const struct pw_module_events module_events = {
    PW_VERSION_FACTORY_EVENTS,
    .info = pipewire_module_info,
};



internal Void pipewire_factory_info(Void *data, const struct pw_factory_info *info);

global const struct pw_factory_events factory_events = {
    PW_VERSION_FACTORY_EVENTS,
    .info = pipewire_factory_info,
};



internal Void pipewire_client_info(Void *data, const struct pw_client_info *info);

global const struct pw_client_events client_events = {
    PW_VERSION_CLIENT_EVENTS,
    .info = pipewire_client_info,
};



internal Void pipewire_node_info(Void *data, const struct pw_node_info *info);
internal Void pipewire_node_param(Void *data, S32 seq, U32 id, U32 index, U32 next, const struct spa_pod *param);

global const struct pw_node_events node_events = {
    PW_VERSION_CLIENT_EVENTS,
    .info  = pipewire_node_info,
    .param = pipewire_node_param,
};



internal Void pipewire_port_info(Void *data, const struct pw_port_info *info);
internal Void pipewire_port_param(Void *data, S32 seq, U32 id, U32 index, U32 next, const struct spa_pod *param);

global const struct pw_port_events port_events = {
    PW_VERSION_PORT_EVENTS,
    .info  = pipewire_port_info,
    .param = pipewire_port_param,
};



internal Void pipewire_device_info(Void *data, const struct pw_device_info *info);
internal Void pipewire_device_param(Void *data, S32 seq, U32 id, U32 index, U32 next, const struct spa_pod *param);

global const struct pw_device_events device_events = {
    PW_VERSION_DEVICE_EVENTS,
    .info  = pipewire_device_info,
    .param = pipewire_device_param,
};



internal Void pipewire_link_info(Void *data, const struct pw_link_info *info);

global const struct pw_link_events link_events = {
    PW_VERSION_LINK_EVENTS,
    .info  = pipewire_link_info,
};



internal Void pipewire_registry_global(Void *data, U32 id, U32 permissions, const char *type, U32 version, const struct spa_dict *props);
internal Void pipewire_registry_global_remove(Void *data, U32 id);

global const struct pw_registry_events registry_events = {
#undef global
    PW_VERSION_REGISTRY_EVENTS,
    .global        = pipewire_registry_global,
    .global_remove = pipewire_registry_global_remove,
#define global static
};



internal Void pipewire_core_done(Void *data, U32 id, S32 seq);

global const struct pw_core_events pipewire_core_roundtrip_events = {
    PW_VERSION_CORE_EVENTS,
    .done = pipewire_core_done,
};

#endif // PIPEWIRE_INCLUDE_H
