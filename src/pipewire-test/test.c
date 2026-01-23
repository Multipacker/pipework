#include "src/base/base_include.h"
#include "src/graphics/graphics_include.h"
#include "src/render/render_core.h"
#include "src/render/opengl/opengl_bindings.h"
#include "src/render/opengl/wayland_opengl.h"

#include "src/base/base_include.c"
#include "src/graphics/graphics_include.c"
#include "src/render/opengl/wayland_opengl.c"

internal S32 os_run(Str8List arguments) {
    Arena *arena = arena_create();

    gfx_init();

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

    Gfx_Window    window = gfx_window_create(str8_literal("Pipewire-test"), 1280, 720);

    // render_create(window)
    struct wl_egl_window *egl_window = 0;
    EGLSurface *egl_surface = 0;
    V2U32 surface_resolution = { 0 };
    {
        Wayland_Window *graphics_window = wayland_window_from_handle(window);

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

        Gfx_EventList graphics_events = gfx_get_events(scratch.arena, false);
        for (Gfx_Event *event = graphics_events.first; event; event = event->next) {
            if (event->kind == Gfx_EventKind_Quit) {
                exit = true;
            }
        }

        if (!exit) {
            // NOTE(simon): Update window.
            V2U32 client_size = gfx_client_area_from_window(window);

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

            Wayland_Window *graphics_window = wayland_window_from_handle(window);
            if (graphics_window->is_configured) {
                eglSwapBuffers(egl_display, egl_surface);
            }
        }

        arena_end_temporary(scratch);
    }

    // opengl_backend_destroy(window, render);
    {
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context);
        eglDestroySurface(egl_display, egl_surface);
        wl_egl_window_destroy(egl_window);
    }

    gfx_window_close(window);

    // opengl_backend_deinit();
    {
        eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglTerminate(egl_display);
        eglReleaseThread();
    }

    return 0;
}
