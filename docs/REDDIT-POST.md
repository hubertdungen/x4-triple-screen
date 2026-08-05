# Reddit publication draft

## Suggested title

[Guide + open-source tool] Mixed-resolution triple monitors in X4 without NVIDIA Surround

## Post

Hi everyone,

**Update — v0.2.1-beta is now available:** the launcher now prevents the unreliable “start X4 first, attach later” path and displays clear errors when the ZIP was not fully extracted.

I wanted to run X4 across three independent monitors: a native 2560x1440 centre monitor and two 1920x1080 side monitors. NVIDIA Surround was not a good option for this combination because the centre image became soft/blurry, while generic borderless-window tools entered a resize loop: they would set the wide window, then X4 would immediately pull it back to one monitor.

After a lot of testing, we found that X4 accepts the wide resolution in `config.xml` but constrains its real window during startup. I have therefore made **X4 Triple Screen**, a small open-source Windows launcher and X4-specific window hook.

**Download:** https://github.com/hubertdungen/x4-triple-screen/releases

**Source, documentation, and checksums:** https://github.com/hubertdungen/x4-triple-screen

The tested result is a real **7680x1440 X4 client area** across three monitors, with Surround disabled.

### What the tool does

- detects Steam, X4, and the complete Windows virtual desktop automatically;
- backs up X4's display configuration;
- starts X4 and follows Steam's process hand-off;
- loads an X4-only window hook that keeps the requested position and size;
- does not patch `X4.exe`, install a service/driver, or affect saves/gameplay;
- supports custom coordinates and resolutions through a small INI file.

### How to launch it

1. Download the release ZIP — do not download or extract only the `.exe`.
2. Use Windows **Extract All** and keep `X4TripleScreen.exe`, `X4TripleScreenHook.dll`, and `x4-triple-screen.ini` together in the same folder.
3. Close X4 completely. Steam may remain open.
4. Double-click `X4TripleScreen.exe`. **Do not start X4 first:** the launcher starts the game itself and must load the hook before X4 creates its Vulkan window.

### Mixed 1440p + 1080p monitor setup

The display side is based on the very useful [mixed-resolution iRacing guide by Alechilles](https://www.reddit.com/r/iRacing/comments/18lgbga/guide_how_to_use_three_monitors_with_different/), adapted here for X4.

1. Keep NVIDIA Surround disabled.
2. In NVIDIA Control Panel, enable the 1.78x DLDSR/DSR factor for the 1080p side displays.
3. Set each side display to an effective 2560x1440. They remain physical 1080p panels; the driver downsamples the 1440p image.
4. Keep the centre display at native 2560x1440.
5. In Windows Display Settings, make the centre primary and arrange all three as one aligned horizontal row.
6. You should now have a 7680x1440 virtual desktop: left at x=-2560, centre at x=0, right at x=2560.
7. Close X4, extract the complete X4 Triple Screen ZIP, and run the launcher. Let the launcher start X4.

CRU is **not normally required**. It can help only if a monitor exposes an unwanted TV/high-resolution mode and NVIDIA calculates DSR from the wrong base resolution. The repository guide explains the safe backup/recovery approach and links only to CRU's official thread.

### Important limitations

This is one very wide X4/Vulkan surface, not three separately projected cameras.

- The cockpit/world view is wonderfully immersive, but the far sides can be geometrically stretched.
- Menus, HUD elements, and the map span the full width. Some are awkward and the map can be genuinely unpleasant to use.
- 7680x1440 is about 11.1 million pixels per frame, so expect a meaningful performance cost.
- The binaries are unsigned. Windows/antivirus may warn because this tool injects its own DLL into X4. Please do not disable security software; the full source and build instructions are available for review.

For me, the immersion is worth the UI trade-off. If you already have a mixed-resolution three-monitor desk, why leave the side displays unused?

This is currently a public beta and targets the Steam/Windows version. Tested layouts, bug reports, and code review are very welcome. Please include your physical/effective monitor resolutions and the log when reporting a problem.
