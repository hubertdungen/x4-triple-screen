# Mixed-resolution triple-monitor guide

This guide uses a 2560x1440 centre display and two 1920x1080 side displays. The same method works with other layouts when every display can be given the same effective height.

## What we are building

Windows will see three independent 2560x1440 desktops in one horizontal row:

```text
left side             centre / primary       right side
2560x1440             2560x1440              2560x1440
x = -2560             x = 0                  x = 2560

<-------------------- X4 window: 7680x1440 -------------------->
```

The 1080p panels still have their physical 1920x1080 pixels. NVIDIA DLDSR/DSR renders 2560x1440 for each side and downsamples it to the panel. The centre remains at its native 2560x1440. Surround stays disabled.

## 1. Save the normal display state

Take a screenshot of Windows Display Settings and note each monitor's normal resolution, refresh rate, orientation, and position. A display-profile utility is optional if you regularly switch between desktop and triple-screen layouts.

## 2. Enable the matching NVIDIA resolution

1. Open **NVIDIA Control Panel**.
2. Open **Manage 3D settings** and find **DSR - Factors**.
3. For a 1920x1080 side display that must simulate 2560x1440, enable **1.78x DL** when available. Classic 1.78x DSR can also expose the same dimensions.
4. Open **Change resolution**.
5. Select each 1080p side monitor and choose **2560x1440** from the Dynamic Super Resolution section.
6. Leave the centre monitor at its native **2560x1440**.

If 1.78x offers 3413x1920 instead, the driver is treating that display as if its base mode were already 2560x1440 or higher. Check the active Windows resolution and any TV/4K modes exposed by the monitor before changing anything else.

## 3. CRU is optional

[Custom Resolution Utility (CRU)](https://www.monitortests.com/forum/Thread-Custom-Resolution-Utility-CRU) edits the resolutions the monitor reports to Windows. It is **not** needed when NVIDIA already exposes the correct 2560x1440 mode on both side displays.

Use CRU only when an unwanted high-resolution EDID/TV mode causes DSR to calculate from the wrong base resolution, or when a display mode genuinely needs correction.

Before using it:

1. Download CRU only from its official Monitor Tests forum thread.
2. Export a backup of each display configuration.
3. Change only the entry you understand; do not copy timings blindly between different monitors.
4. Apply with CRU's `restart64.exe`.
5. Keep CRU's recovery options available. Its official package includes `reset-all.exe`, and the restart tool offers a recovery mode if the display becomes unusable.

CRU cannot add physical pixels to a 1080p panel. Here it can only influence which modes the NVIDIA driver sees; DLDSR/DSR performs the actual downsampling.

## 4. Align the displays in Windows

1. Open **Settings > System > Display**.
2. Set the centre monitor as the main display.
3. Arrange the left, centre, and right monitor rectangles in one horizontal row.
4. Align their top or bottom edges consistently. With all three at an effective 1440-pixel height, perfect top alignment is simplest.
5. Apply and confirm that the virtual desktop is 7680x1440.

Refresh rates do not have to match. In the tested layout the centre and side displays use different refresh rates, although perceived smoothness can differ between panels.

## 5. Run X4 Triple Screen

1. Close X4.
2. Download and extract the latest X4 Triple Screen release ZIP.
3. Double-click `X4TripleScreen.exe`.

The default configuration detects the full virtual desktop. In the example it resolves to:

```ini
x=-2560
y=0
width=7680
height=1440
```

The launcher backs up and prepares X4's display settings, follows Steam's X4 process hand-off, injects the hook, and keeps the game window on the requested rectangle.

## 6. Understand the trade-offs

This creates one very wide Vulkan render surface. It does not turn X4 into a simulator with three separately projected cameras.

- The cockpit and world gain strong peripheral immersion.
- Geometry at the far sides can be stretched because X4 uses one projection.
- Menus, HUD, and the map can span the entire width and may be inconvenient.
- The GPU renders all 11.1 million pixels of 7680x1440 before the side displays downsample them.

If those UI limitations are acceptable, the result can be far more immersive than a single display and avoids the centre-monitor blur caused by unsuitable Surround mode combinations.

## Other monitor combinations

Use the sum of the effective monitor widths and the common effective height. If the primary display is not the leftmost display, `x` will usually be negative.

Example: 2560x1440 left + 3440x1440 centre + 2560x1440 right:

```ini
[window]
x=-2560
y=0
width=8560
height=1440
```

For non-rectangular arrangements, set an explicit rectangle. Areas of that rectangle not covered by a physical monitor still consume rendering work and are not visible.
