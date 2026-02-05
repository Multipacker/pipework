#include "src/base/base_include.h"

#include "src/base/base_include.c"

#include "new-pipewire.h"
#include "new-pipewire.c"

internal S32 os_run(Str8List arguments) {
    pipewire_init();

    for (;;) {
        Arena_Temporary scratch = arena_get_scratch(0, 0);

        pipewire_tick();

        Pipewire_ObjectArray nodes = pipewire_objects_from_kind(scratch.arena, Pipewire_ObjectKind_Node);
        for (U64 i = 0; i < nodes.count; ++i) {
            Pipewire_Object *node = nodes.objects[i];

            Str8 name = pipewire_string_from_property_name(node, str8_literal("node.name"));
            printf("Node name: %.*s\n", str8_expand(name));
        }

        arena_end_temporary(scratch);
    }

    pipewire_deinit();

    return 0;
}
