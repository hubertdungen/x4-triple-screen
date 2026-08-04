This beta keeps the working mixed-resolution triple-screen experience from v0.1.0-alpha and hardens the parts that touch X4 and its configuration.

### Confirmed result

- Real 7680x1440 X4 client area across three independent displays.
- Native 2560x1440 centre display with two 1080p side displays using NVIDIA DLDSR/DSR at an effective 2560x1440.
- NVIDIA Surround disabled.

### Safer in this beta

- The launcher verifies the complete executable path before injecting into an existing or Steam replacement `X4.exe` process.
- Multiple matching X4 processes are treated as an error instead of choosing one silently.
- X4 configuration writes are atomic.
- `--restore-config` restores only the original display values, preserves unrelated current settings, and clears a read-only lock applied by the launcher.
- Built-in self-tests and a clean pinned Windows CI build validate every change.

### Read before downloading

This is beta software. X4 still renders one very wide projection: menus, HUD elements, and the map can stretch across all displays, and edge geometry can distort. Rendering 7680x1440 is demanding.

The binaries are unsigned and load the included hook DLL into X4, so SmartScreen or heuristic antivirus warnings are possible. Do not disable security software. Download only from this repository, verify the checksums, or build from source.

External reports for other monitor layouts, GPUs, Windows versions, and X4 builds are still needed before calling the project stable.
