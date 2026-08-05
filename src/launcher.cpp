#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Settings {
  fs::path ini;
  fs::path dll;
  fs::path x4_exe;
  fs::path steam_exe;
  RECT desktop{};
  bool prepare_config = true;
  bool lock_config = false;
  int steam_wait_seconds = 20;
};

std::wstring ReadIniString(const fs::path& ini, const wchar_t* section, const wchar_t* key,
                           const wchar_t* fallback) {
  wchar_t value[4096]{};
  GetPrivateProfileStringW(section, key, fallback, value, 4096, ini.c_str());
  return value;
}

bool IsAuto(const std::wstring& value) {
  return value.empty() || _wcsicmp(value.c_str(), L"auto") == 0;
}

int ReadCoordinate(const fs::path& ini, const wchar_t* key, int fallback) {
  const std::wstring value = ReadIniString(ini, L"window", key, L"auto");
  return IsAuto(value) ? fallback : _wtoi(value.c_str());
}

std::wstring UnescapeVdf(std::wstring value) {
  size_t offset = 0;
  while ((offset = value.find(L"\\\\", offset)) != std::wstring::npos) {
    value.replace(offset, 2, L"\\");
    ++offset;
  }
  return value;
}

std::optional<std::wstring> LastQuotedValue(const std::wstring& line) {
  const size_t end = line.rfind(L'"');
  if (end == std::wstring::npos || end == 0) return std::nullopt;
  const size_t begin = line.rfind(L'"', end - 1);
  if (begin == std::wstring::npos || begin + 1 >= end) return std::nullopt;
  return line.substr(begin + 1, end - begin - 1);
}

fs::path SteamFromRegistry() {
  HKEY key = nullptr;
  wchar_t value[4096]{};
  DWORD bytes = sizeof(value);
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &key) == ERROR_SUCCESS) {
    if (RegQueryValueExW(key, L"SteamPath", nullptr, nullptr, reinterpret_cast<BYTE*>(value), &bytes) == ERROR_SUCCESS) {
      RegCloseKey(key);
      return fs::path(value) / L"steam.exe";
    }
    RegCloseKey(key);
  }
  return {};
}

std::vector<fs::path> SteamLibraryRoots(const fs::path& steam_exe) {
  std::vector<fs::path> roots;
  if (!steam_exe.empty()) roots.push_back(steam_exe.parent_path());
  const fs::path libraries = steam_exe.parent_path() / L"steamapps" / L"libraryfolders.vdf";
  std::wifstream file(libraries);
  std::wstring line;
  while (std::getline(file, line)) {
    if (line.find(L"\"path\"") == std::wstring::npos) continue;
    if (auto value = LastQuotedValue(line)) roots.emplace_back(UnescapeVdf(*value));
  }
  std::sort(roots.begin(), roots.end());
  roots.erase(std::unique(roots.begin(), roots.end()), roots.end());
  return roots;
}

fs::path X4FromSteam(const fs::path& steam_exe) {
  for (const fs::path& root : SteamLibraryRoots(steam_exe)) {
    const fs::path manifest = root / L"steamapps" / L"appmanifest_392160.acf";
    if (!fs::exists(manifest)) continue;
    std::wifstream file(manifest);
    std::wstring line;
    while (std::getline(file, line)) {
      if (line.find(L"\"installdir\"") == std::wstring::npos) continue;
      if (auto value = LastQuotedValue(line)) {
        const fs::path exe = root / L"steamapps" / L"common" / *value / L"X4.exe";
        if (fs::exists(exe)) return exe;
      }
    }
  }
  return {};
}

std::vector<DWORD> FindProcesses(const wchar_t* name, DWORD excluded_pid = 0) {
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return {};
  std::vector<DWORD> pids;
  if (Process32FirstW(snapshot, &entry)) {
    do {
      if (_wcsicmp(entry.szExeFile, name) == 0 && entry.th32ProcessID != excluded_pid) {
        pids.push_back(entry.th32ProcessID);
      }
    } while (Process32NextW(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return pids;
}

DWORD FindProcess(const wchar_t* name, DWORD excluded_pid = 0) {
  const std::vector<DWORD> pids = FindProcesses(name, excluded_pid);
  return pids.empty() ? 0 : pids.front();
}

std::optional<fs::path> ProcessExecutablePath(DWORD pid) {
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!process) return std::nullopt;
  std::vector<wchar_t> path(32768);
  DWORD size = static_cast<DWORD>(path.size());
  const BOOL ok = QueryFullProcessImageNameW(process, 0, path.data(), &size);
  CloseHandle(process);
  if (!ok || size == 0) return std::nullopt;
  return fs::path(std::wstring(path.data(), size));
}

std::wstring NormalizedPath(const fs::path& path) {
  std::error_code ec;
  fs::path normalized = fs::weakly_canonical(path, ec);
  if (ec) normalized = fs::absolute(path, ec).lexically_normal();
  std::wstring value = normalized.wstring();
  std::transform(value.begin(), value.end(), value.begin(),
                 [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
  return value;
}

bool ProcessMatchesExecutable(DWORD pid, const fs::path& expected) {
  const auto actual = ProcessExecutablePath(pid);
  if (!actual) return false;
  std::error_code ec;
  if (fs::equivalent(*actual, expected, ec) && !ec) return true;
  return NormalizedPath(*actual) == NormalizedPath(expected);
}

DWORD FindExpectedProcess(const wchar_t* name, const fs::path& expected, DWORD excluded_pid,
                          bool* ambiguous) {
  DWORD match = 0;
  int matches = 0;
  for (DWORD pid : FindProcesses(name, excluded_pid)) {
    if (!ProcessMatchesExecutable(pid, expected)) continue;
    match = pid;
    ++matches;
  }
  if (ambiguous) *ambiguous = matches > 1;
  return matches == 1 ? match : 0;
}

bool StartSteam(const Settings& settings) {
  if (FindProcess(L"steam.exe")) return true;
  if (settings.steam_exe.empty() || !fs::exists(settings.steam_exe)) return false;
  std::wcout << L"Starting Steam...\n";
  STARTUPINFOW startup{};
  PROCESS_INFORMATION process{};
  startup.cb = sizeof(startup);
  std::wstring command = L"\"" + settings.steam_exe.wstring() + L"\" -silent";
  if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      settings.steam_exe.parent_path().c_str(), &startup, &process)) {
    return false;
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  for (int i = 0; i < settings.steam_wait_seconds * 4; ++i) {
    if (FindProcess(L"steam.exe")) return true;
    Sleep(250);
  }
  return false;
}

bool InjectDll(HANDLE process, const fs::path& dll) {
  const std::wstring path = fs::absolute(dll).wstring();
  const SIZE_T bytes = (path.size() + 1) * sizeof(wchar_t);
  LPVOID remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!remote) return false;
  bool success = false;
  if (WriteProcessMemory(process, remote, path.c_str(), bytes, nullptr)) {
    const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    const auto load_library = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(kernel32, "LoadLibraryW"));
    HANDLE thread = CreateRemoteThread(process, nullptr, 0, load_library, remote, 0, nullptr);
    if (thread) {
      if (WaitForSingleObject(thread, 15000) == WAIT_OBJECT_0) {
        DWORD result = 0;
        GetExitCodeThread(thread, &result);
        success = result != 0;
      }
      CloseHandle(thread);
    }
  }
  VirtualFreeEx(process, remote, 0, MEM_RELEASE);
  return success;
}

std::string ReadText(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool WriteTextAtomically(const fs::path& path, const std::string& text) {
  fs::path temporary = path;
  temporary += L".x4ts.tmp";
  {
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    file.flush();
    if (!file) {
      std::error_code ignored;
      fs::remove(temporary, ignored);
      return false;
    }
  }
  SetFileAttributesW(temporary.c_str(), FILE_ATTRIBUTE_NORMAL);
  if (!MoveFileExW(temporary.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    std::error_code ignored;
    fs::remove(temporary, ignored);
    return false;
  }
  return true;
}

std::optional<std::string> XmlValue(const std::string& text, const std::string& key) {
  const std::string open = "<" + key + ">";
  const std::string close = "</" + key + ">";
  const size_t begin = text.find(open);
  if (begin == std::string::npos) return std::nullopt;
  const size_t value_begin = begin + open.size();
  const size_t end = text.find(close, value_begin);
  if (end == std::string::npos) return std::nullopt;
  return text.substr(value_begin, end - value_begin);
}

bool ReplaceXmlValue(std::string& text, const std::string& key, const std::string& value) {
  const std::string open = "<" + key + ">";
  const std::string close = "</" + key + ">";
  const size_t begin = text.find(open);
  if (begin == std::string::npos) return false;
  const size_t value_begin = begin + open.size();
  const size_t end = text.find(close, value_begin);
  if (end == std::string::npos) return false;
  text.replace(value_begin, end - value_begin, value);
  return true;
}

bool RestoreDisplayValues(std::string& current, const std::string& original) {
  for (const std::string key : {"fullscreen", "borderless", "res_width", "res_height"}) {
    const auto value = XmlValue(original, key);
    if (!value || !ReplaceXmlValue(current, key, *value)) return false;
  }
  return true;
}

std::optional<fs::path> FindX4Config() {
  PWSTR documents_raw = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents_raw))) return std::nullopt;
  fs::path base = fs::path(documents_raw) / L"Egosoft" / L"X4";
  CoTaskMemFree(documents_raw);
  if (!fs::exists(base)) return std::nullopt;
  fs::path newest;
  fs::file_time_type newest_time{};
  for (const auto& entry : fs::directory_iterator(base)) {
    if (!entry.is_directory()) continue;
    const fs::path config = entry.path() / L"config.xml";
    if (!fs::exists(config)) continue;
    const auto modified = fs::last_write_time(config);
    if (newest.empty() || modified > newest_time) {
      newest = config;
      newest_time = modified;
    }
  }
  if (newest.empty()) return std::nullopt;
  return newest;
}

bool PrepareX4Config(const Settings& settings) {
  const auto config_path = FindX4Config();
  if (!config_path) {
    std::wcerr << L"Warning: X4 config.xml was not found. The hook will still be attempted.\n";
    return true;
  }
  const fs::path backup = config_path->parent_path() / L"config.x4-triple-screen.backup.xml";
  std::error_code ec;
  if (!fs::exists(backup)) fs::copy_file(*config_path, backup, fs::copy_options::none, ec);
  if (ec) {
    std::wcerr << L"Could not create config backup: " << backup << L"\n";
    return false;
  }

  std::string text = ReadText(*config_path);
  if (text.empty()) return false;
  const int width = settings.desktop.right - settings.desktop.left;
  const int height = settings.desktop.bottom - settings.desktop.top;
  bool valid = true;
  valid &= ReplaceXmlValue(text, "fullscreen", "false");
  valid &= ReplaceXmlValue(text, "borderless", "false");
  valid &= ReplaceXmlValue(text, "res_width", std::to_string(width));
  valid &= ReplaceXmlValue(text, "res_height", std::to_string(height));
  if (!valid) {
    std::wcerr << L"config.xml does not contain the expected X4 display keys.\n";
    return false;
  }

  const DWORD original_attributes = GetFileAttributesW(config_path->c_str());
  if (original_attributes == INVALID_FILE_ATTRIBUTES) return false;
  if (!SetFileAttributesW(config_path->c_str(), original_attributes & ~FILE_ATTRIBUTE_READONLY)) {
    std::wcerr << L"Could not unlock X4 config for writing.\n";
    return false;
  }
  const bool wrote_config = WriteTextAtomically(*config_path, text);
  DWORD final_attributes = original_attributes;
  if (settings.lock_config || (original_attributes & FILE_ATTRIBUTE_READONLY) != 0) {
    final_attributes |= FILE_ATTRIBUTE_READONLY;
  } else {
    final_attributes &= ~FILE_ATTRIBUTE_READONLY;
  }
  if (!SetFileAttributesW(config_path->c_str(), final_attributes) || !wrote_config) return false;
  std::wcout << L"Prepared X4 config: " << config_path->wstring() << L"\n";
  std::wcout << L"Backup: " << backup.wstring() << L"\n";
  return true;
}

bool RestoreX4Config() {
  const auto config_path = FindX4Config();
  if (!config_path) return false;
  const fs::path backup = config_path->parent_path() / L"config.x4-triple-screen.backup.xml";
  if (!fs::exists(backup)) return false;
  std::string current = ReadText(*config_path);
  const std::string original = ReadText(backup);
  if (current.empty() || original.empty()) return false;
  if (!RestoreDisplayValues(current, original)) return false;

  const DWORD attributes = GetFileAttributesW(config_path->c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) return false;
  if (!SetFileAttributesW(config_path->c_str(), attributes & ~FILE_ATTRIBUTE_READONLY)) return false;
  if (!WriteTextAtomically(*config_path, current)) return false;
  // The restore command is also the recovery path for lock_x4_config=1.
  return SetFileAttributesW(config_path->c_str(), attributes & ~FILE_ATTRIBUTE_READONLY) != FALSE;
}

bool ValidDesktopRectangle(const Settings& settings) {
  const long long width = static_cast<long long>(settings.desktop.right) - settings.desktop.left;
  const long long height = static_cast<long long>(settings.desktop.bottom) - settings.desktop.top;
  return width > 0 && height > 0 && width <= 100000 && height <= 100000;
}

bool RunSelfTests() {
  std::string xml = "<root><fullscreen>true</fullscreen><borderless>true</borderless>"
                    "<res_width>1920</res_width><res_height>1080</res_height></root>";
  bool ok = XmlValue(xml, "res_width") == std::optional<std::string>("1920");
  ok &= ReplaceXmlValue(xml, "fullscreen", "false");
  ok &= ReplaceXmlValue(xml, "res_width", "7680");
  ok &= XmlValue(xml, "fullscreen") == std::optional<std::string>("false");
  ok &= XmlValue(xml, "res_width") == std::optional<std::string>("7680");
  ok &= !ReplaceXmlValue(xml, "missing", "1");

  std::string current = "<root><fullscreen>false</fullscreen><borderless>false</borderless>"
                        "<res_width>7680</res_width><res_height>1440</res_height>"
                        "<quality>ultra</quality></root>";
  const std::string original = "<root><fullscreen>true</fullscreen><borderless>true</borderless>"
                               "<res_width>2560</res_width><res_height>1440</res_height>"
                               "<quality>medium</quality></root>";
  ok &= RestoreDisplayValues(current, original);
  ok &= XmlValue(current, "res_width") == std::optional<std::string>("2560");
  ok &= XmlValue(current, "quality") == std::optional<std::string>("ultra");

  Settings valid;
  valid.desktop = {-2560, 0, 5120, 1440};
  Settings invalid;
  invalid.desktop = {0, 0, 0, 1440};
  ok &= ValidDesktopRectangle(valid);
  ok &= !ValidDesktopRectangle(invalid);
  std::wcout << (ok ? L"Self-tests passed.\n" : L"Self-tests failed.\n");
  return ok;
}

Settings LoadSettings(const fs::path& launcher) {
  Settings settings;
  const fs::path directory = launcher.parent_path();
  settings.ini = directory / L"x4-triple-screen.ini";
  settings.dll = directory / L"X4TripleScreenHook.dll";
  const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
  const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
  const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  const int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  const int x = ReadCoordinate(settings.ini, L"x", vx);
  const int y = ReadCoordinate(settings.ini, L"y", vy);
  const int width = ReadCoordinate(settings.ini, L"width", vw);
  const int height = ReadCoordinate(settings.ini, L"height", vh);
  settings.desktop = {x, y, x + width, y + height};
  settings.prepare_config = GetPrivateProfileIntW(L"launcher", L"prepare_x4_config", 1, settings.ini.c_str()) != 0;
  settings.lock_config = GetPrivateProfileIntW(L"launcher", L"lock_x4_config", 0, settings.ini.c_str()) != 0;
  settings.steam_wait_seconds = GetPrivateProfileIntW(L"launcher", L"steam_wait_seconds", 20, settings.ini.c_str());

  std::wstring steam = ReadIniString(settings.ini, L"launcher", L"steam_exe", L"auto");
  settings.steam_exe = IsAuto(steam) ? SteamFromRegistry() : fs::path(steam);
  std::wstring x4 = ReadIniString(settings.ini, L"launcher", L"x4_exe", L"auto");
  settings.x4_exe = IsAuto(x4) ? X4FromSteam(settings.steam_exe) : fs::path(x4);
  return settings;
}

void PrintSettings(const Settings& settings) {
  std::wcout << L"\nX4 Triple Screen\n"
             << L"================\n"
             << L"Desktop target : x=" << settings.desktop.left << L" y=" << settings.desktop.top
             << L" width=" << settings.desktop.right - settings.desktop.left
             << L" height=" << settings.desktop.bottom - settings.desktop.top << L"\n"
             << L"X4 executable   : " << (settings.x4_exe.empty() ? L"not found" : settings.x4_exe.wstring()) << L"\n"
             << L"Hook DLL        : " << settings.dll.wstring() << L"\n\n";
}

int ShowError(const std::wstring& message, int code) {
  std::wcerr << message << L"\n";
  MessageBoxW(nullptr, message.c_str(), L"X4 Triple Screen", MB_OK | MB_ICONERROR);
  return code;
}

int AttachToExisting(DWORD pid, const Settings& settings) {
  HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                   PROCESS_VM_WRITE | PROCESS_VM_READ,
                               FALSE, pid);
  if (!process) {
    std::wcerr << L"Could not open X4 process (error " << GetLastError() << L").\n";
    return 5;
  }
  const bool injected = InjectDll(process, settings.dll);
  CloseHandle(process);
  if (!injected) {
    std::wcerr << L"DLL injection failed.\n";
    return 6;
  }
  std::wcout << L"Hook injected into existing X4 process (pid " << pid << L").\n";
  return 0;
}

int LaunchSuspended(const Settings& settings) {
  if (!StartSteam(settings)) {
    std::wcerr << L"Steam is not running and could not be started.\n";
    return 7;
  }
  STARTUPINFOW startup{};
  PROCESS_INFORMATION process{};
  startup.cb = sizeof(startup);
  std::wstring command = L"\"" + settings.x4_exe.wstring() + L"\"";
  if (!CreateProcessW(settings.x4_exe.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED,
                      nullptr, settings.x4_exe.parent_path().c_str(), &startup, &process)) {
    std::wcerr << L"Could not start X4 (error " << GetLastError() << L").\n";
    return 8;
  }
  std::wcout << L"X4 created in suspended state (pid " << process.dwProcessId << L").\n";
  if (!InjectDll(process.hProcess, settings.dll)) {
    std::wcerr << L"DLL injection failed; the suspended X4 process will be closed.\n";
    TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 9;
  }
  ResumeThread(process.hThread);
  CloseHandle(process.hThread);

  // Steam may replace the executable we started with a second X4.exe process.
  // Follow that hand-off and inject the real game automatically. If no
  // replacement appears, the suspended process is the real game and already
  // has the hook loaded.
  constexpr int kReplacementWaitMs = 30000;
  constexpr int kPollMs = 250;
  for (int elapsed = 0; elapsed < kReplacementWaitMs; elapsed += kPollMs) {
    bool ambiguous = false;
    const DWORD replacement =
        FindExpectedProcess(L"X4.exe", settings.x4_exe, process.dwProcessId, &ambiguous);
    if (ambiguous) {
      CloseHandle(process.hProcess);
      std::wcerr << L"More than one matching replacement X4 process was found.\n";
      return 11;
    }
    if (replacement) {
      std::wcout << L"Steam handed X4 to process " << replacement << L"; loading hook there...\n";
      CloseHandle(process.hProcess);
      return AttachToExisting(replacement, settings);
    }

    if (WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0) {
      // The bootstrap process has exited. Steam can take a moment to expose the
      // replacement in the process list, so keep polling until the deadline.
      Sleep(kPollMs);
      continue;
    }

    // A direct/non-Steam launch that remains alive does not need the full wait.
    // Ten seconds is enough to distinguish it from Steam's normal bootstrap.
    if (elapsed >= 10000) {
      CloseHandle(process.hProcess);
      std::wcout << L"Hook loaded. X4 is starting.\n";
      return 0;
    }
    Sleep(kPollMs);
  }

  const DWORD bootstrap_exit = WaitForSingleObject(process.hProcess, 0);
  CloseHandle(process.hProcess);
  if (bootstrap_exit == WAIT_OBJECT_0) {
    std::wcerr << L"Steam closed the bootstrap process, but the final X4 process was not found.\n";
    return 10;
  }
  std::wcout << L"Hook loaded. X4 is starting.\n";
  return 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  SetConsoleOutputCP(CP_UTF8);
  wchar_t module[MAX_PATH]{};
  GetModuleFileNameW(nullptr, module, MAX_PATH);
  const fs::path launcher = fs::absolute(module);
  if (argc > 1 && _wcsicmp(argv[1], L"--self-test") == 0) {
    return RunSelfTests() ? 0 : 12;
  }
  Settings settings = LoadSettings(launcher);
  PrintSettings(settings);

  if (!ValidDesktopRectangle(settings)) {
    return ShowError(
        L"Invalid window rectangle. Check x, y, width, and height in x4-triple-screen.ini.", 2);
  }

  if (argc > 1 && _wcsicmp(argv[1], L"--restore-config") == 0) {
    if (RestoreX4Config()) {
      std::wcout << L"Original X4 config restored from backup.\n";
      return 0;
    }
    return ShowError(L"No restorable X4 config backup was found.", 2);
  }

  if (!fs::exists(settings.ini)) {
    return ShowError(
        L"Missing x4-triple-screen.ini next to the launcher.\n\n"
        L"Download the release ZIP, choose Extract All, and keep the EXE, DLL, and INI together.",
        2);
  }
  if (!fs::exists(settings.dll)) {
    return ShowError(
        L"Missing X4TripleScreenHook.dll next to the launcher.\n\n"
        L"Download the release ZIP, choose Extract All, and keep the EXE, DLL, and INI together.",
        3);
  }
  if (settings.x4_exe.empty() || !fs::exists(settings.x4_exe) ||
      _wcsicmp(settings.x4_exe.filename().c_str(), L"X4.exe") != 0) {
    return ShowError(L"X4.exe was not found. Set x4_exe in x4-triple-screen.ini.", 4);
  }

  if (argc > 1 && _wcsicmp(argv[1], L"--dry-run") == 0) {
    std::wcout << L"Dry run complete; nothing was changed or launched.\n";
    return 0;
  }

  bool ambiguous = false;
  const DWORD existing = FindExpectedProcess(L"X4.exe", settings.x4_exe, 0, &ambiguous);
  if (ambiguous) {
    return ShowError(L"More than one matching X4 process is running. Close X4 and try again.", 11);
  }
  if (existing) {
    return ShowError(
        L"X4 is already running.\n\n"
        L"Close X4 completely, then run X4TripleScreen.exe. Steam may remain open.\n\n"
        L"The launcher must start X4 before its Vulkan window is created.",
        13);
  }

  if (settings.prepare_config && !PrepareX4Config(settings)) {
    return ShowError(L"X4 config preparation failed. No process was injected.", 4);
  }

  const int result = LaunchSuspended(settings);
  if (result != 0) {
    return ShowError(
        L"X4 could not be started with the triple-screen hook.\n\n"
        L"See the console output, README.md, and X4TripleScreen.log for details.",
        result);
  } else {
    Sleep(1500);
  }
  return result;
}
