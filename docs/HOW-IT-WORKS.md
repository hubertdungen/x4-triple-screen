# How X4 Triple Screen works

## The X4 problem

Changing `config.xml` to an oversized window is not sufficient. X4 accepts the saved dimensions, but during window creation it queries the selected monitor and constrains/repositions the window to that monitor. External window movers then fight with X4, producing the resize loop seen with generic borderless-window tools.

## Launcher

`X4TripleScreen.exe`:

1. Reads `x4-triple-screen.ini` and the Windows virtual desktop.
2. Finds Steam and X4 from the Steam registry and library manifests, unless explicit paths are configured.
3. Backs up and prepares X4's `config.xml` for windowed rendering.
4. Starts X4 suspended and loads the hook before normal window creation.
5. Detects Steam's replacement X4 process and loads the hook there as well.

If X4 is already running, the launcher can attach to that process directly.

## Hook

`X4TripleScreenHook.dll` uses MinHook to intercept only selected Win32 window-management calls in `X4.exe`:

- window creation and style changes;
- window position/size changes;
- selected-monitor information;
- virtual-screen metrics used by the game.

For the X4 top-level window, the hook returns the configured virtual rectangle as the monitor work area, removes the normal frame, and rejects attempts to shrink the window. A low-frequency watchdog reapplies the requested rectangle as a fallback.

The game renderer remains X4's Vulkan renderer. The tool does not intercept Vulkan, alter game assets, modify saves, or change simulation/gameplay logic.

## Scope and security boundary

- The launcher validates that the target filename is `X4.exe`.
- The DLL exists only inside the launched X4 process and unloads when X4 exits.
- There is no background service, driver, updater, network access, telemetry, or persistence mechanism.
- Source and reproducible build instructions are included so release binaries can be independently inspected.
- DLL injection is inherently security-sensitive and can trigger heuristic antivirus detection. Never download releases from unofficial mirrors and never disable security software to force execution.

## Files touched

The only user file modified is the active X4 `config.xml`. Its first-run backup is named `config.x4-triple-screen.backup.xml` in the same folder. The launcher can restore that backup with `--restore-config`.

The tool also creates `X4TripleScreen.log` in its own folder.
