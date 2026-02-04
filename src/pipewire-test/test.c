#include "src/base/base_include.h"

#include "src/base/base_include.c"

#include "new-pipewire.h"
#include "new-pipewire.c"

internal S32 os_run(Str8List arguments) {
    pipewire_init();

    for (;;) {
        pipewire_tick();
    }

    pipewire_deinit();

    return 0;
}
