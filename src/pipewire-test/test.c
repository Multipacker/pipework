#include "src/base/base_include.h"
#include "src/graphics/graphics_include.h"
#include "src/render/render_core.h"
#include "src/render/opengl/opengl_bindings.h"
#include "src/render/opengl/wayland_opengl.h"

#include "src/base/base_include.c"
#include "src/graphics/graphics_include.c"
#include "src/render/opengl/wayland_opengl.c"

internal Void test_wayland_xdg_wm_base_ping(Void *data, struct xdg_wm_base *xdg_wm_base, U32 serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}

global const struct xdg_wm_base_listener test_wayland_xdg_wm_base_listener = {
    .ping = test_wayland_xdg_wm_base_ping,
};

internal Void test_wayland_registry_global(Void *data, struct wl_registry *registry, U32 name, const char *interface, U32 version) {
    Wayland_State *state = &global_wayland_state;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        if (version >= 6) {
            state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 6);
        } else {
            state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
        }
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 3);
        xdg_wm_base_add_listener(state->xdg_wm_base, &test_wayland_xdg_wm_base_listener, 0);
    } else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
        state->xdg_decoration_manager = wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1);
    }
}

internal Void test_wayland_registry_global_remove(Void *data, struct wl_registry *registry, U32 name) {
}

global const struct wl_registry_listener test_wayland_registry_listener = {
#undef global
    .global = test_wayland_registry_global,
#define global static
    .global_remove = test_wayland_registry_global_remove,
};

internal S32 os_run(Str8List arguments) {
    Arena *arena = arena_create();

    // gfx_init();
    {
        Wayland_State *state = &global_wayland_state;
        state->arena = arena_create();
        state->selection_source_arena = arena_create();

        // NOTE(simon): Connect to the display and listen for initial list of globals.
        state->display = wl_display_connect(0);
        struct wl_registry *registry = wl_display_get_registry(state->display);
        wl_registry_add_listener(registry, &test_wayland_registry_listener, 0);
        wl_display_roundtrip(state->display);

        // NOTE(simon): Exit if we don't have the required globals.
        if (!(state->compositor && state->shm && state->xdg_wm_base)) {
            // TODO(simon): Inform the user.
            os_exit(1);
        }
    }

    // opengl_backend_init();

    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");
    PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC eglCreatePlatformWindowSurfaceEXT = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC) eglGetProcAddress("eglCreatePlatformWindowSurfaceEXT");
    EGLDisplay egl_display = 0;
    EGLConfig  egl_config  = 0;
    EGLContext egl_context = 0;
    {
        Wayland_State *wayland_state = &global_wayland_state;
        Arena_Temporary scratch = arena_get_scratch(0, 0);

        if (!eglGetPlatformDisplayEXT || !eglCreatePlatformWindowSurfaceEXT) {
            gfx_message(true, str8_literal("Failed to initialize OpenGL"), str8_literal("Could not load eglGetPlatformDisplayEXT or eglCreatePlatformWindowSurfaceEXT."));
            os_exit(1);
        }

        // NOTE(simon): Get display.
        egl_display = eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, wayland_state->display, 0);
        if (egl_display == EGL_NO_DISPLAY) {
            gfx_message(true, str8_literal("Failed to initialize OpenGL"), str8_literal("Could not acquire EGL display."));
            os_exit(1);
        }

        // NOTE(simon): Initialize.
        EGLint major = 0, minor = 0;
        if (!eglInitialize(egl_display, &major, &minor)) {
            gfx_message(true, str8_literal("Failed to initialize OpenGL"), str8_literal("Could not initialize EGL."));
            os_exit(1);
        }

        // NOTE(simon): Bind OpenGL API.
        if (!eglBindAPI(EGL_OPENGL_API)) {
            gfx_message(true, str8_literal("Failed to initialize OpenGL"), str8_literal("Could not bind OpenGL API."));
            os_exit(1);
        }

        // NOTE(simon): Gather configs.
        EGLint config_attributes[] = {
            EGL_SURFACE_TYPE,      EGL_WINDOW_BIT,
            EGL_CONFORMANT,        EGL_OPENGL_BIT,
            EGL_RENDERABLE_TYPE,   EGL_OPENGL_BIT,
            EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
            EGL_LEVEL,             0,
            EGL_ALPHA_SIZE,        8,
            EGL_RED_SIZE,          8,
            EGL_GREEN_SIZE,        8,
            EGL_BLUE_SIZE,         8,
            EGL_DEPTH_SIZE,        24,
            EGL_STENCIL_SIZE,      8,
            EGL_NONE,
        };
        EGLint available_config_count = 0;
        eglChooseConfig(egl_display, config_attributes, 0, 0, &available_config_count);
        EGLConfig *available_configs = arena_push_array(scratch.arena, EGLConfig, (U64) available_config_count);
        eglChooseConfig(egl_display, config_attributes, available_configs, available_config_count, &available_config_count);

        // NOTE(simon): Choose config.
        for (EGLint i = 0; i < available_config_count; ++i) {
            EGLConfig config = available_configs[i];

            if (1) {
                egl_config = config;
                break;
            }
        }

        if (!egl_config) {
            gfx_message(true, str8_literal("Failed to initialize OpenGL"), str8_literal("Could not find a suitable config."));
            os_exit(1);
        }

        // NOTE(simon): Create context.
        EGLint context_attributes[] = {
            EGL_CONTEXT_MAJOR_VERSION, 3,
            EGL_CONTEXT_MINOR_VERSION, 3,
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
            EGL_NONE,
        };
        egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, context_attributes);
        if (egl_context == EGL_NO_CONTEXT) {
            gfx_message(true, str8_literal("Failed to initialize OpenGL"), str8_literal("Could not create a context."));
            os_exit(1);
        }

        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context);

#define X(type, name) name = (type) eglGetProcAddress(#name); assert(name);
        GL_FUNCTIONS(X)
#undef X

        arena_end_temporary(scratch);
    }

    Gfx_Window gfx_window = { 0 }; // gfx_window_create(str8_literal("Pipewire-test"), 1280, 720);
    {
        U32 width = 1280;
        U32 height = 720;
        Str8 title = str8_literal("Pipewire-test");

        Wayland_State *state = &global_wayland_state;
        Arena_Temporary scratch = arena_get_scratch(0, 0);

        // NOTE(simon): Allocate window.
        Wayland_Window *window = state->window_freelist;
        if (window) {
            sll_stack_pop(state->window_freelist);
            U64 generation = window->generation;
            memory_zero_struct(window);
            window->generation = generation;
        } else {
            window = arena_push_struct(state->arena, Wayland_Window);
        }
        dll_push_back(state->first_window, state->last_window, window);

        // NOTE(simon): Allocate base surface.
        window->surface = wayland_surface_create();
        window->surface->width  = (S32) width;
        window->surface->height = (S32) height;

        // NOTE(simon): Allocate XDG surface.
        window->xdg_surface = xdg_wm_base_get_xdg_surface(state->xdg_wm_base, window->surface->surface);
        xdg_surface_add_listener(window->xdg_surface, &wayland_xdg_surface_listener, window);

        // NOTE(simon): Get the toplevel XDG role.
        window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
        xdg_toplevel_add_listener(window->xdg_toplevel, &wayland_xdg_toplevel_listener, window);
        CStr title_cstr = cstr_from_str8(scratch.arena, title);
        xdg_toplevel_set_title(window->xdg_toplevel, title_cstr);

        // NOTE(simon): Commit all changes.
        wl_surface_commit(window->surface->surface);

        gfx_window = wayland_handle_from_window(window);
        arena_end_temporary(scratch);
    }

    // render_create(gfx_window)
    struct wl_egl_window *egl_window = 0;
    EGLSurface *egl_surface = 0;
    V2U32 surface_resolution = { 0 };
    {
        Wayland_Window *graphics_window = wayland_window_from_handle(gfx_window);

        egl_window = wl_egl_window_create(graphics_window->surface->surface, graphics_window->surface->width, graphics_window->surface->height);

        const EGLint surface_attributes[] = {
            EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_SRGB,
            EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
            EGL_NONE,
        };

        printf("before eglCreateWindowSurface\n");
        egl_surface = eglCreatePlatformWindowSurfaceEXT(egl_display, egl_config, egl_window, surface_attributes);
        printf("after eglCreateWindowSurface\n");

        if (egl_surface == EGL_NO_SURFACE) {
            gfx_message(true, str8_literal("Failed to create OpenGL window"), str8_literal("Could not create a EGL window surface."));
            os_exit(1);
        }

        eglSwapInterval(egl_display, 1);
    }

    B32 exit = false;
    while (!exit) {
        Arena_Temporary scratch = arena_get_scratch(0, 0);

        {
            Wayland_State *state = &global_wayland_state;

            state->event_arena = scratch.arena;

            // TODO(simon): Error handling
            while (wl_display_prepare_read(state->display) != 0) {
                wl_display_dispatch_pending(state->display);
            }

            wl_display_flush(state->display);

            struct pollfd fds[1] = { 0 };
            fds[0].fd     = wl_display_get_fd(state->display);
            fds[0].events = POLLIN;
            poll(fds, array_count(fds), 0);

            // NOTE(simon): Handle display events.
            if (fds[0].revents & POLLIN) {
                wl_display_read_events(state->display);
                wl_display_dispatch_pending(state->display);
            } else {
                wl_display_cancel_read(state->display);
            }

            for (Gfx_Event *event = state->events.first; event; event = event->next) {
                if (event->kind == Gfx_EventKind_Quit) {
                    exit = true;
                }
            }

            // NOTE(simon): Reset event state.
            state->event_arena = 0;
            memory_zero_struct(&state->events);
        }

        if (!exit) {
            // NOTE(simon): Update window.
            V2U32 client_size = gfx_client_area_from_window(gfx_window);

            // NOTE(simon): Draw UI.
            if (surface_resolution.width != client_size.width || surface_resolution.height != client_size.height) {
                surface_resolution = client_size;
                wl_egl_window_resize(egl_window, (S32) client_size.width, (S32) client_size.height, 0, 0);
            }

            eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

            // NOTE(simon): This doesn't automatically get set if our first
            // eglMakeCurrent doesn't have a default framebuffer. On my desktop using
            // Xwayland, I get a black screen if I don't run this with a surface bound.
            glDrawBuffer(GL_BACK);

            glViewport(0, 0, (GLsizei) client_size.width, (GLsizei) client_size.height);
            glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            Wayland_Window *graphics_window = wayland_window_from_handle(gfx_window);
            if (graphics_window->is_configured) {
                eglSwapBuffers(egl_display, egl_surface);
            }
        }

        arena_end_temporary(scratch);
    }

    // opengl_backend_destroy(gfx_window, render);
    {
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context);
        eglDestroySurface(egl_display, egl_surface);
        wl_egl_window_destroy(egl_window);
    }

    // gfx_window_close(gfx_window);
    {
        Wayland_State *state = &global_wayland_state;
        Wayland_Window *window = wayland_window_from_handle(gfx_window);

        arena_destroy(window->title_bar_arena);

        xdg_toplevel_destroy(window->xdg_toplevel);
        xdg_surface_destroy(window->xdg_surface);
        wayland_surface_destroy(window->surface);

        ++window->generation;

        dll_remove(state->first_window, state->last_window, window);
        sll_stack_push(state->window_freelist, window);
    }

    // opengl_backend_deinit();
    {
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglTerminate(egl_display);
        eglReleaseThread();
    }

    return 0;
}
