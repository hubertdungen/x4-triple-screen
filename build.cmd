@echo off
setlocal

set "ZIG=%~1"
if "%ZIG%"=="" set "ZIG=zig"
set "ROOT=%~dp0"
set "MINHOOK=%ROOT%third_party\minhook-1.3.4"
set "OUT=%ROOT%release"
set "OBJ=%OUT%\obj"

if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OBJ%" mkdir "%OBJ%"

"%ZIG%" rc /nologo /fo "%OBJ%\launcher.res" "%ROOT%resources\launcher.rc" || exit /b 1
"%ZIG%" rc /nologo /fo "%OBJ%\hook.res" "%ROOT%resources\hook.rc" || exit /b 1

"%ZIG%" c++ -target x86_64-windows-gnu -std=c++20 -O2 -municode ^
  "%ROOT%src\launcher.cpp" "%OBJ%\launcher.res" -lshell32 -lole32 -ladvapi32 -luser32 ^
  -o "%OUT%\X4TripleScreen.exe" || exit /b 1

"%ZIG%" cc -target x86_64-windows-gnu -O2 -DUNICODE -D_UNICODE -I "%MINHOOK%\include" -I "%MINHOOK%\src" -c "%MINHOOK%\src\buffer.c" -o "%OBJ%\buffer.o" || exit /b 1
"%ZIG%" cc -target x86_64-windows-gnu -O2 -DUNICODE -D_UNICODE -I "%MINHOOK%\include" -I "%MINHOOK%\src" -c "%MINHOOK%\src\hook.c" -o "%OBJ%\minhook.o" || exit /b 1
"%ZIG%" cc -target x86_64-windows-gnu -O2 -DUNICODE -D_UNICODE -I "%MINHOOK%\include" -I "%MINHOOK%\src" -c "%MINHOOK%\src\trampoline.c" -o "%OBJ%\trampoline.o" || exit /b 1
"%ZIG%" cc -target x86_64-windows-gnu -O2 -DUNICODE -D_UNICODE -I "%MINHOOK%\include" -I "%MINHOOK%\src" -c "%MINHOOK%\src\hde\hde64.c" -o "%OBJ%\hde64.o" || exit /b 1

"%ZIG%" c++ -target x86_64-windows-gnu -std=c++20 -O2 -shared -DUNICODE -D_UNICODE ^
  -I "%MINHOOK%\include" -I "%MINHOOK%\src" "%ROOT%src\hook.cpp" ^
  "%OBJ%\buffer.o" "%OBJ%\minhook.o" "%OBJ%\trampoline.o" "%OBJ%\hde64.o" "%OBJ%\hook.res" ^
  -luser32 -ladvapi32 -o "%OUT%\X4TripleScreenHook.dll" || exit /b 1

copy /y "%ROOT%x4-triple-screen.ini" "%OUT%\x4-triple-screen.ini" >nul
copy /y "%ROOT%README.md" "%OUT%\README.md" >nul
copy /y "%ROOT%LICENSE" "%OUT%\LICENSE" >nul
copy /y "%ROOT%THIRD-PARTY-NOTICES.md" "%OUT%\THIRD-PARTY-NOTICES.md" >nul
copy /y "%ROOT%CHANGELOG.md" "%OUT%\CHANGELOG.md" >nul
copy /y "%ROOT%CONTRIBUTING.md" "%OUT%\CONTRIBUTING.md" >nul
if not exist "%OUT%\docs" mkdir "%OUT%\docs"
copy /y "%ROOT%docs\HOW-IT-WORKS.md" "%OUT%\docs\HOW-IT-WORKS.md" >nul
copy /y "%ROOT%docs\SETUP-MIXED-RESOLUTION.md" "%OUT%\docs\SETUP-MIXED-RESOLUTION.md" >nul

powershell.exe -NoProfile -Command "$files = @('%OUT%\X4TripleScreen.exe', '%OUT%\X4TripleScreenHook.dll'); Get-FileHash -Algorithm SHA256 -LiteralPath $files | ForEach-Object { $_.Hash.ToLowerInvariant() + '  ' + (Split-Path -Leaf $_.Path) } | Set-Content -LiteralPath '%OUT%\SHA256SUMS.txt' -Encoding ascii" || exit /b 1

echo Build complete: %OUT%
endlocal
