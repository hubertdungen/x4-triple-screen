This is the first experimental public build of X4 Triple Screen.

### Confirmed result

- Real 7680x1440 X4 client area across three independent displays.
- Tested with a native 2560x1440 centre display and two 1080p side displays running at an effective 2560x1440 through NVIDIA DLDSR/DSR.
- NVIDIA Surround disabled.
- X4 no longer constrains the game back to a single 2560x1392 display window.

### Included

- One-click Steam/X4 and virtual-desktop detection.
- Automatic handling of Steam's X4 process hand-off.
- Configurable position and resolution for other rectangular monitor layouts.
- X4 config backup, optional read-only lock, and restore command.
- Mixed-resolution DLDSR/DSR guide with optional CRU troubleshooting.
- Source code, reproducible Windows build, licenses, and SHA-256 checksums.

### Read before downloading

This is alpha software. X4 renders one very wide projection: menus, HUD elements, and the map can stretch across all displays, and edge geometry can distort. Rendering 7680x1440 is demanding.

The binaries are unsigned and load the included hook DLL into X4, so SmartScreen or heuristic antivirus warnings are possible. Do not disable security software. Download only from this repository, verify the checksums, or build from source.
