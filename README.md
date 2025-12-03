# pipewire-test

## Building and Running

The program runs under both X11 and Wayland, and depending on which you want to
run you will need different dependencies. The exact names might vary depending
on your distro.

* X11: `xcb`, `xcb-cursor`, `egl`, `xkbcomon-x11`, `xkbcommon`
* Wayland: `wayland`, `wayland-protocols`, `egl`, `xkbcommon`

You will also need clang. Navigate to the project root and run
`scripts/build_clang.sh`. The build script will try to detect which window
server you are currently running and will choose the backend that matches that.
You can override this by manually specifying either `wayland` or `x11` on the
command line when running the script. If you want a release build, append
`release` before running the build script.
