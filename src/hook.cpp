#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

#include "MinHook.h"

namespace {

struct Settings {
  RECT desktop{};
  bool borderless = true;
  bool lock_size = true;
  bool virtualize_monitor = true;
  bool topmost = false;
  int watchdog_ms = 500;
};

Settings g_settings;
HMODULE g_module = nullptr;
std::atomic<HWND> g_target{nullptr};
std::atomic<bool> g_ready{false};
thread_local bool g_internal = false;
CRITICAL_SECTION g_log_lock;

using CreateWindowExWFn = HWND(WINAPI*)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
using CreateWindowExAFn = HWND(WINAPI*)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
using SetWindowPosFn = BOOL(WINAPI*)(HWND, HWND, int, int, int, int, UINT);
using MoveWindowFn = BOOL(WINAPI*)(HWND, int, int, int, int, BOOL);
using ShowWindowFn = BOOL(WINAPI*)(HWND, int);
using SetWindowLongPtrWFn = LONG_PTR(WINAPI*)(HWND, int, LONG_PTR);
using SetWindowLongPtrAFn = LONG_PTR(WINAPI*)(HWND, int, LONG_PTR);
using SetWindowLongWFn = LONG(WINAPI*)(HWND, int, LONG);
using SetWindowLongAFn = LONG(WINAPI*)(HWND, int, LONG);
using GetMonitorInfoWFn = BOOL(WINAPI*)(HMONITOR, LPMONITORINFO);
using GetMonitorInfoAFn = BOOL(WINAPI*)(HMONITOR, LPMONITORINFO);
using GetSystemMetricsFn = int(WINAPI*)(int);

CreateWindowExWFn RealCreateWindowExW = nullptr;
CreateWindowExAFn RealCreateWindowExA = nullptr;
SetWindowPosFn RealSetWindowPos = nullptr;
MoveWindowFn RealMoveWindow = nullptr;
ShowWindowFn RealShowWindow = nullptr;
SetWindowLongPtrWFn RealSetWindowLongPtrW = nullptr;
SetWindowLongPtrAFn RealSetWindowLongPtrA = nullptr;
SetWindowLongWFn RealSetWindowLongW = nullptr;
SetWindowLongAFn RealSetWindowLongA = nullptr;
GetMonitorInfoWFn RealGetMonitorInfoW = nullptr;
GetMonitorInfoAFn RealGetMonitorInfoA = nullptr;
GetSystemMetricsFn RealGetSystemMetrics = nullptr;

int Width() { return g_settings.desktop.right - g_settings.desktop.left; }
int Height() { return g_settings.desktop.bottom - g_settings.desktop.top; }

std::wstring ModuleDirectory() {
  wchar_t path[MAX_PATH]{};
  GetModuleFileNameW(g_module, path, MAX_PATH);
  wchar_t* slash = wcsrchr(path, L'\\');
  if (slash) *slash = L'\0';
  return path;
}

void Log(const wchar_t* format, ...) {
  EnterCriticalSection(&g_log_lock);
  const std::wstring path = ModuleDirectory() + L"\\X4TripleScreen.log";
  FILE* file = nullptr;
  _wfopen_s(&file, path.c_str(), L"a, ccs=UTF-8");
  if (file) {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    fwprintf(file, L"%04u-%02u-%02u %02u:%02u:%02u.%03u ", now.wYear, now.wMonth,
             now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    va_list args;
    va_start(args, format);
    vfwprintf(file, format, args);
    va_end(args);
    fputws(L"\n", file);
    fclose(file);
  }
  LeaveCriticalSection(&g_log_lock);
}

bool IsAuto(const wchar_t* value) {
  return _wcsicmp(value, L"auto") == 0 || value[0] == L'\0';
}

int ReadIniInt(const std::wstring& ini, const wchar_t* section, const wchar_t* key, int fallback) {
  return static_cast<int>(GetPrivateProfileIntW(section, key, fallback, ini.c_str()));
}

int ReadIniCoordinate(const std::wstring& ini, const wchar_t* key, int fallback) {
  wchar_t value[64]{};
  GetPrivateProfileStringW(L"window", key, L"auto", value, 64, ini.c_str());
  return IsAuto(value) ? fallback : _wtoi(value);
}

void LoadSettings() {
  const std::wstring ini = ModuleDirectory() + L"\\x4-triple-screen.ini";
  const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
  const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
  const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  const int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  const int x = ReadIniCoordinate(ini, L"x", vx);
  const int y = ReadIniCoordinate(ini, L"y", vy);
  const int width = ReadIniCoordinate(ini, L"width", vw);
  const int height = ReadIniCoordinate(ini, L"height", vh);
  g_settings.desktop = {x, y, x + width, y + height};
  g_settings.borderless = ReadIniInt(ini, L"window", L"borderless", 1) != 0;
  g_settings.lock_size = ReadIniInt(ini, L"window", L"lock_size", 1) != 0;
  g_settings.virtualize_monitor = ReadIniInt(ini, L"window", L"virtualize_monitor", 1) != 0;
  g_settings.topmost = ReadIniInt(ini, L"window", L"topmost", 0) != 0;
  g_settings.watchdog_ms = ReadIniInt(ini, L"advanced", L"watchdog_ms", 500);
  if (g_settings.watchdog_ms < 100) g_settings.watchdog_ms = 100;
  Log(L"Configuration: x=%d y=%d width=%d height=%d borderless=%d lock_size=%d virtualize_monitor=%d",
      x, y, width, height, g_settings.borderless, g_settings.lock_size, g_settings.virtualize_monitor);
}

bool SameProcess(HWND window) {
  DWORD pid = 0;
  GetWindowThreadProcessId(window, &pid);
  return pid == GetCurrentProcessId();
}

bool LooksLikeX4Window(HWND window) {
  if (!window || !SameProcess(window)) return false;
  const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
  if ((style & WS_CHILD) != 0 || GetWindow(window, GW_OWNER) != nullptr) return false;

  wchar_t title[256]{};
  wchar_t class_name[256]{};
  GetWindowTextW(window, title, 256);
  GetClassNameW(window, class_name, 256);
  return wcsstr(title, L"X4") != nullptr || wcsstr(class_name, L"X4") != nullptr;
}

LONG_PTR BorderlessStyle(LONG_PTR style) {
  style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
  style |= WS_POPUP;
  return style;
}

LONG_PTR BorderlessExStyle(LONG_PTR style) {
  style &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE);
  return style;
}

void ApplyWindow(HWND window, bool show) {
  if (!window || !IsWindow(window) || !RealSetWindowPos) return;
  g_internal = true;
  if (g_settings.borderless) {
    LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    LONG_PTR ex_style = GetWindowLongPtrW(window, GWL_EXSTYLE);
    const LONG_PTR new_style = BorderlessStyle(style);
    const LONG_PTR new_ex_style = BorderlessExStyle(ex_style);
    if (new_style != style && RealSetWindowLongPtrW) RealSetWindowLongPtrW(window, GWL_STYLE, new_style);
    if (new_ex_style != ex_style && RealSetWindowLongPtrW) RealSetWindowLongPtrW(window, GWL_EXSTYLE, new_ex_style);
  }
  UINT flags = SWP_NOOWNERZORDER | SWP_FRAMECHANGED;
  if (!g_settings.topmost) flags |= SWP_NOZORDER;
  if (!show) flags |= SWP_NOACTIVATE;
  if (show) flags |= SWP_SHOWWINDOW;
  RealSetWindowPos(window, g_settings.topmost ? HWND_TOPMOST : nullptr,
                   g_settings.desktop.left, g_settings.desktop.top, Width(), Height(), flags);
  g_internal = false;
}

void CaptureAndApply(HWND window) {
  if (!LooksLikeX4Window(window)) return;
  HWND expected = nullptr;
  if (g_target.compare_exchange_strong(expected, window)) {
    Log(L"Captured X4 window: hwnd=0x%p", window);
  }
  if (g_target.load() == window) ApplyWindow(window, IsWindowVisible(window) != FALSE);
}

BOOL CALLBACK FindWindowCallback(HWND window, LPARAM) {
  if (LooksLikeX4Window(window)) {
    g_target.store(window);
    return FALSE;
  }
  return TRUE;
}

HWND WINAPI HookCreateWindowExW(DWORD ex_style, LPCWSTR class_name, LPCWSTR window_name, DWORD style,
                                int x, int y, int width, int height, HWND parent, HMENU menu,
                                HINSTANCE instance, LPVOID param) {
  const bool candidate = parent == nullptr && window_name && wcsstr(window_name, L"X4") != nullptr;
  if (candidate) {
    x = g_settings.desktop.left;
    y = g_settings.desktop.top;
    width = Width();
    height = Height();
    if (g_settings.borderless) {
      style = static_cast<DWORD>(BorderlessStyle(style));
      ex_style = static_cast<DWORD>(BorderlessExStyle(ex_style));
    }
  }
  HWND window = RealCreateWindowExW(ex_style, class_name, window_name, style, x, y, width, height,
                                    parent, menu, instance, param);
  CaptureAndApply(window);
  return window;
}

HWND WINAPI HookCreateWindowExA(DWORD ex_style, LPCSTR class_name, LPCSTR window_name, DWORD style,
                                int x, int y, int width, int height, HWND parent, HMENU menu,
                                HINSTANCE instance, LPVOID param) {
  const bool candidate = parent == nullptr && window_name && strstr(window_name, "X4") != nullptr;
  if (candidate) {
    x = g_settings.desktop.left;
    y = g_settings.desktop.top;
    width = Width();
    height = Height();
    if (g_settings.borderless) {
      style = static_cast<DWORD>(BorderlessStyle(style));
      ex_style = static_cast<DWORD>(BorderlessExStyle(ex_style));
    }
  }
  HWND window = RealCreateWindowExA(ex_style, class_name, window_name, style, x, y, width, height,
                                    parent, menu, instance, param);
  CaptureAndApply(window);
  return window;
}

BOOL WINAPI HookSetWindowPos(HWND window, HWND insert_after, int x, int y, int width, int height, UINT flags) {
  if (!g_internal && g_settings.lock_size && window == g_target.load()) {
    x = g_settings.desktop.left;
    y = g_settings.desktop.top;
    width = Width();
    height = Height();
    flags &= ~(SWP_NOMOVE | SWP_NOSIZE);
    if (!g_settings.topmost) flags |= SWP_NOZORDER;
  }
  return RealSetWindowPos(window, insert_after, x, y, width, height, flags);
}

BOOL WINAPI HookMoveWindow(HWND window, int x, int y, int width, int height, BOOL repaint) {
  if (!g_internal && g_settings.lock_size && window == g_target.load()) {
    return RealMoveWindow(window, g_settings.desktop.left, g_settings.desktop.top, Width(), Height(), repaint);
  }
  return RealMoveWindow(window, x, y, width, height, repaint);
}

BOOL WINAPI HookShowWindow(HWND window, int command) {
  const BOOL result = RealShowWindow(window, command);
  CaptureAndApply(window);
  return result;
}

LONG_PTR WINAPI HookSetWindowLongPtrW(HWND window, int index, LONG_PTR value) {
  if (!g_internal && window == g_target.load() && g_settings.borderless) {
    if (index == GWL_STYLE) value = BorderlessStyle(value);
    if (index == GWL_EXSTYLE) value = BorderlessExStyle(value);
  }
  return RealSetWindowLongPtrW(window, index, value);
}

LONG_PTR WINAPI HookSetWindowLongPtrA(HWND window, int index, LONG_PTR value) {
  if (!g_internal && window == g_target.load() && g_settings.borderless) {
    if (index == GWL_STYLE) value = BorderlessStyle(value);
    if (index == GWL_EXSTYLE) value = BorderlessExStyle(value);
  }
  return RealSetWindowLongPtrA(window, index, value);
}

LONG WINAPI HookSetWindowLongW(HWND window, int index, LONG value) {
  if (!g_internal && window == g_target.load() && g_settings.borderless) {
    if (index == GWL_STYLE) value = static_cast<LONG>(BorderlessStyle(value));
    if (index == GWL_EXSTYLE) value = static_cast<LONG>(BorderlessExStyle(value));
  }
  return RealSetWindowLongW(window, index, value);
}

LONG WINAPI HookSetWindowLongA(HWND window, int index, LONG value) {
  if (!g_internal && window == g_target.load() && g_settings.borderless) {
    if (index == GWL_STYLE) value = static_cast<LONG>(BorderlessStyle(value));
    if (index == GWL_EXSTYLE) value = static_cast<LONG>(BorderlessExStyle(value));
  }
  return RealSetWindowLongA(window, index, value);
}

template <typename MonitorInfo>
void VirtualizeMonitor(HMONITOR monitor, MonitorInfo* info) {
  if (!g_settings.virtualize_monitor || !info) return;
  const HWND target = g_target.load();
  if (!target) return;
  const HMONITOR target_monitor = MonitorFromWindow(target, MONITOR_DEFAULTTONEAREST);
  if (monitor != target_monitor) return;
  info->rcMonitor = g_settings.desktop;
  info->rcWork = g_settings.desktop;
  info->dwFlags |= MONITORINFOF_PRIMARY;
}

BOOL WINAPI HookGetMonitorInfoW(HMONITOR monitor, LPMONITORINFO info) {
  const BOOL result = RealGetMonitorInfoW(monitor, info);
  if (result) VirtualizeMonitor(monitor, info);
  return result;
}

BOOL WINAPI HookGetMonitorInfoA(HMONITOR monitor, LPMONITORINFO info) {
  const BOOL result = RealGetMonitorInfoA(monitor, info);
  if (result) VirtualizeMonitor(monitor, info);
  return result;
}

int WINAPI HookGetSystemMetrics(int index) {
  if (g_settings.virtualize_monitor) {
    if (index == SM_CXSCREEN || index == SM_CXFULLSCREEN) return Width();
    if (index == SM_CYSCREEN || index == SM_CYFULLSCREEN) return Height();
  }
  return RealGetSystemMetrics(index);
}

bool InstallHook(const wchar_t* module, const char* name, LPVOID replacement, LPVOID* original) {
  const MH_STATUS status = MH_CreateHookApi(module, name, replacement, original);
  if (status != MH_OK) {
    Log(L"MH_CreateHookApi failed for %S: %S", name, MH_StatusToString(status));
    return false;
  }
  return true;
}

DWORD WINAPI Bootstrap(LPVOID) {
  InitializeCriticalSection(&g_log_lock);
  Log(L"X4 Triple Screen hook loaded (pid=%lu)", GetCurrentProcessId());
  SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  LoadSettings();

  if (MH_Initialize() != MH_OK) {
    Log(L"MH_Initialize failed");
    return 1;
  }

  bool ok = true;
  ok &= InstallHook(L"user32.dll", "CreateWindowExW", reinterpret_cast<LPVOID>(HookCreateWindowExW), reinterpret_cast<LPVOID*>(&RealCreateWindowExW));
  ok &= InstallHook(L"user32.dll", "CreateWindowExA", reinterpret_cast<LPVOID>(HookCreateWindowExA), reinterpret_cast<LPVOID*>(&RealCreateWindowExA));
  ok &= InstallHook(L"user32.dll", "SetWindowPos", reinterpret_cast<LPVOID>(HookSetWindowPos), reinterpret_cast<LPVOID*>(&RealSetWindowPos));
  ok &= InstallHook(L"user32.dll", "MoveWindow", reinterpret_cast<LPVOID>(HookMoveWindow), reinterpret_cast<LPVOID*>(&RealMoveWindow));
  ok &= InstallHook(L"user32.dll", "ShowWindow", reinterpret_cast<LPVOID>(HookShowWindow), reinterpret_cast<LPVOID*>(&RealShowWindow));
  ok &= InstallHook(L"user32.dll", "SetWindowLongPtrW", reinterpret_cast<LPVOID>(HookSetWindowLongPtrW), reinterpret_cast<LPVOID*>(&RealSetWindowLongPtrW));
  ok &= InstallHook(L"user32.dll", "SetWindowLongPtrA", reinterpret_cast<LPVOID>(HookSetWindowLongPtrA), reinterpret_cast<LPVOID*>(&RealSetWindowLongPtrA));
  ok &= InstallHook(L"user32.dll", "SetWindowLongW", reinterpret_cast<LPVOID>(HookSetWindowLongW), reinterpret_cast<LPVOID*>(&RealSetWindowLongW));
  ok &= InstallHook(L"user32.dll", "SetWindowLongA", reinterpret_cast<LPVOID>(HookSetWindowLongA), reinterpret_cast<LPVOID*>(&RealSetWindowLongA));
  ok &= InstallHook(L"user32.dll", "GetMonitorInfoW", reinterpret_cast<LPVOID>(HookGetMonitorInfoW), reinterpret_cast<LPVOID*>(&RealGetMonitorInfoW));
  ok &= InstallHook(L"user32.dll", "GetMonitorInfoA", reinterpret_cast<LPVOID>(HookGetMonitorInfoA), reinterpret_cast<LPVOID*>(&RealGetMonitorInfoA));
  ok &= InstallHook(L"user32.dll", "GetSystemMetrics", reinterpret_cast<LPVOID>(HookGetSystemMetrics), reinterpret_cast<LPVOID*>(&RealGetSystemMetrics));
  if (!ok || MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
    Log(L"Could not enable all hooks");
    return 2;
  }

  g_ready.store(true);
  Log(L"Hooks enabled");
  EnumWindows(FindWindowCallback, 0);

  RECT last_logged{};
  while (true) {
    HWND target = g_target.load();
    if (!target || !IsWindow(target)) {
      g_target.store(nullptr);
      EnumWindows(FindWindowCallback, 0);
      target = g_target.load();
    }
    if (target) {
      RECT rect{};
      GetWindowRect(target, &rect);
      if (rect.left != g_settings.desktop.left || rect.top != g_settings.desktop.top ||
          rect.right != g_settings.desktop.right || rect.bottom != g_settings.desktop.bottom) {
        ApplyWindow(target, true);
      }
      GetWindowRect(target, &rect);
      if (memcmp(&rect, &last_logged, sizeof(rect)) != 0) {
        Log(L"Window rect: x=%ld y=%ld width=%ld height=%ld", rect.left, rect.top,
            rect.right - rect.left, rect.bottom - rect.top);
        last_logged = rect;
      }
    }
    Sleep(static_cast<DWORD>(g_settings.watchdog_ms));
  }
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_module = instance;
    DisableThreadLibraryCalls(instance);
    HANDLE thread = CreateThread(nullptr, 0, Bootstrap, nullptr, 0, nullptr);
    if (thread) CloseHandle(thread);
  }
  return TRUE;
}
