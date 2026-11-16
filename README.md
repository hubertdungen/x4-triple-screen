# X4 Triple Screen

Experimental open-source Windows launcher for running **X4: Foundations** in one borderless window across multiple independent monitors, without NVIDIA Surround.

X4 normally pulls an oversized window back to one monitor. X4 Triple Screen starts the game, loads an X4-specific window hook, presents the complete Windows virtual desktop as the selected monitor, and keeps the requested window rectangle in place.

## Download

[Download the newest X4 Triple Screen release ZIP](https://github.com/hubertdungen/x4-triple-screen/releases). Use Windows **Extract All** and keep these three files together in the same folder:

- `X4TripleScreen.exe`
- `X4TripleScreenHook.dll`
- `x4-triple-screen.ini`

Do not download or extract only the `.exe`. Close X4 completely, then double-click `X4TripleScreen.exe`; the launcher starts X4 itself. Steam may remain open.

Release notes and SHA-256 checksums are published beside every download.

No installation, NVIDIA Surround, service, driver, or permanent modification to `X4.exe` is required.

## Tested configuration

- Windows 11
- Steam edition of X4: Foundations
- Centre display: 2560x1440
- Two 1920x1080 side displays running at an effective 2560x1440 through NVIDIA DLDSR/DSR
- Final X4 client area: **7680x1440**

The hook has been confirmed in the running game: X4 continued rendering at 7680x1440 instead of reverting to a single-display 2560x1392 window.

## Quick start

1. Make every monitor use the same effective vertical resolution.
2. Arrange the monitors as one rectangle in Windows Display Settings.
3. Close X4 completely. Steam may remain open.
4. Download the release ZIP and choose **Extract All** to a writable folder.
5. Confirm that the EXE, DLL, and INI listed above are together in that folder.
6. Double-click `X4TripleScreen.exe`. **Do not start X4 first**: the launcher must load the hook before X4 creates its Vulkan window.

The default `auto` configuration detects Steam, X4, and the complete Windows virtual desktop. For the mixed 1440p/1080p setup, see the [step-by-step mixed-resolution guide](docs/SETUP-MIXED-RESOLUTION.md).

## Other layouts and resolutions

Edit `x4-triple-screen.ini` beside the launcher. For three effective 2560x1440 displays with the centre display set as primary:

```ini
[window]
x=-2560
y=0
width=7680
height=1440
```

The rectangle is expressed in Windows virtual-desktop coordinates. Negative `x` is normal when a monitor is left of the primary display. Leave all four values as `auto` to cover the complete virtual desktop.

This is not limited to three 2560x1440 displays: two monitors, ultrawides, asymmetric widths, and other rectangular layouts can use explicit coordinates and dimensions.

## What it changes

- Creates `config.x4-triple-screen.backup.xml` beside the active X4 `config.xml`.
- Sets X4 to windowed mode and writes the requested width and height before launch.
- Loads `X4TripleScreenHook.dll` into `X4.exe` only for that game session.
- Keeps the window borderless and at the requested position and size.
- Writes `X4TripleScreen.log` beside the launcher for troubleshooting.

It does **not** patch `X4.exe`, install a service or driver, enable Surround, or run in unrelated processes.

By default the X4 configuration remains writable. To deliberately make it read-only, set `lock_x4_config=1`; X4 will then be unable to save other settings until the attribute is removed.

Restore the backed-up X4 display settings with:

```text
X4TripleScreen.exe --restore-config
```

The restore command preserves unrelated current X4 settings and clears the read-only attribute if `lock_x4_config=1` had enabled it.

Deleting the extracted folder removes the tool itself.

## Known limitations

- X4 does not provide native triple-projection rendering, bezel correction, or per-monitor FOV controls.
- Menus, HUD elements, and especially the map can stretch across the complete window and may be awkward or partially unusable.
- 7680x1440 is about 11.1 million rendered pixels per frame and can substantially reduce performance.
- Different refresh rates can coexist, but presentation depends on Windows, the GPU driver, and X4's Vulkan swapchain.
- The binaries are not code-signed. SmartScreen or antivirus may warn because the launcher loads a DLL into X4. Do not disable security software; inspect/build the source if you do not trust a release binary.
- X4 is the only supported target. Do not use this technique with multiplayer games protected by anti-cheat.

## Troubleshooting

- Run `X4TripleScreen.exe --dry-run` to inspect detection without changing or launching anything.
- Check `X4TripleScreen.log` beside the launcher.
- If X4 is not detected, set `x4_exe` and `steam_exe` explicitly in the INI.
- If the wrong area is covered, set explicit `x`, `y`, `width`, and `height`.
- If side images are vertically offset, align the monitor rectangles in Windows and make their effective heights match.
- If X4 is already running, the launcher stops and tells you to close it. Exit X4 completely, then run `X4TripleScreen.exe`; attaching after the Vulkan window exists is not reliable.
- If Windows reports a missing DLL or INI, extract the complete release ZIP instead of copying only the launcher EXE.

Normal X4 extensions/mods should not affect the hook because it operates on the native Windows window rather than game assets or scripts. UI mods can, of course, change how usable the already very wide menus and map are.

## Building from source

Requirements:

- Zig 0.16.0 for Windows x86-64
- Vendored MinHook 1.3.4 sources

Run:

```text
build.cmd C:\path\to\zig.exe
```

PowerShell users can alternatively run:

```powershell
.\build.ps1 -Zig C:\path\to\zig.exe -Configuration Release
```

Artifacts are written to `release/`. See [How it works](docs/HOW-IT-WORKS.md) for the implementation and threat boundary.

Run the built-in checks with `X4TripleScreen.exe --self-test`. Release packaging and clean Windows builds are also exercised by GitHub Actions.

## License and credits

X4 Triple Screen is MIT licensed. [MinHook](https://github.com/TsudaKageyu/minhook) is distributed under its 2-clause BSD license.

The mixed-resolution setup is inspired by Alechilles' [iRacing guide for different-resolution triple monitors](https://www.reddit.com/r/iRacing/comments/18lgbga/guide_how_to_use_three_monitors_with_different/).

This project is not affiliated with or endorsed by Egosoft, Valve, Microsoft, NVIDIA, or the CRU project.


<p align="center"><a href="https://dlforge.dungenlabs.com"><img src="docs/branding/dlforge-logo.svg#gh-light-mode-only" alt="DL Forge" width="120"><img src="docs/branding/dlforge-logo-negative.svg#gh-dark-mode-only" alt="DL Forge" width="120"></a></p>
