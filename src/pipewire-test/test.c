#include "src/base/base_include.h"

#include "src/base/base_include.c"

#include "new-pipewire.h"
#include "new-pipewire.c"

internal S32 os_run(Str8List arguments) {
    pipewire_init();

    Pipewire_ObjectStore *store = pipewire_object_store_create();

    for (;;) {
        Arena_Temporary scratch = arena_get_scratch(0, 0);
        Pipewire_EventList events = pipewire_c2u_pop_events(scratch.arena);
        pipewire_object_store_apply_events(store, events);

        printf("Events:\n");
        for (Pipewire_Event *event = events.first; event; event = event->next) {
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

        arena_end_temporary(scratch);
    }

    pipewire_deinit();

    return 0;
}
