# Changelog

## 0.2.1-beta - 2026-08-05

- Refuse to attach after X4 has already created its Vulkan window; users are now told to close X4 and let the launcher start it.
- Show persistent Windows error dialogs instead of allowing startup errors to disappear with the console window.
- Explain that the complete release ZIP must be extracted and that the EXE, DLL, and INI must remain together.
- Select release notes from the requested package version instead of a hard-coded filename.

## 0.2.0-beta - 2026-08-05

- Validate the full executable path before injecting into an existing or Steam replacement `X4.exe` process.
- Refuse ambiguous multiple matching X4 processes.
- Restore only the backed-up display values, preserving unrelated current X4 settings.
- Clear the read-only attribute when restoring a configuration locked by the launcher.
- Write X4 configuration changes atomically.
- Add built-in self-tests, release-package checks, and a pinned clean Windows CI build.
- Use a stable GitHub Releases link in public documentation.

## 0.1.0-alpha - 2026-08-04

- Initial X4-specific launcher and window hook.
- Automatic Steam, X4, and virtual-desktop detection.
- Automatic handling of Steam's replacement X4 process.
- Configurable coordinates and dimensions for arbitrary rectangular layouts.
- X4 config backup, optional read-only lock, and restore command.
- Verified 7680x1440 client area across a mixed-resolution three-monitor setup without NVIDIA Surround.
