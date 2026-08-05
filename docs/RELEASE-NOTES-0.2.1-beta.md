# X4 Triple Screen v0.2.1-beta

This beta fixes the most confusing first-run failure: starting X4 before the launcher.

## What changed

- The launcher now refuses an already-running X4 session because attaching after the Vulkan window exists is not reliable.
- Startup failures are shown in a Windows dialog, so a missing companion file no longer looks like “nothing happened”.
- The missing-file message explains that the full ZIP must be extracted, with the EXE, DLL, and INI kept together.
- The README and public guide now put the correct launch order first.

## Correct launch order

1. Download `X4TripleScreen-v0.2.1-beta.zip` from this release.
2. Choose Windows **Extract All**. Do not extract only the EXE.
3. Keep `X4TripleScreen.exe`, `X4TripleScreenHook.dll`, and `x4-triple-screen.ini` together.
4. Close X4 completely. Steam may remain open.
5. Run `X4TripleScreen.exe` and let it start X4.

This remains a beta release. The binaries are unsigned and may trigger a SmartScreen or antivirus warning because the launcher loads its own DLL into X4. Do not disable security software; source and reproducible build instructions are available in the repository.
