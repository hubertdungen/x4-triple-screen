param(
    [string]$Zig = "zig",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$MinHook = Join-Path $ProjectRoot "third_party\minhook-1.3.4"
$Output = Join-Path $ProjectRoot "release"

$ZigExecutable = if (Test-Path -LiteralPath $Zig) {
    (Resolve-Path -LiteralPath $Zig).Path
} else {
    $Command = Get-Command $Zig -ErrorAction SilentlyContinue
    if (-not $Command) { throw "Zig was not found. Pass -Zig with the path to zig.exe." }
    $Command.Source
}
if (-not (Test-Path -LiteralPath (Join-Path $MinHook "LICENSE.txt"))) {
    throw "MinHook 1.3.4 sources are missing under third_party."
}

New-Item -ItemType Directory -Path $Output -Force | Out-Null
$Optimize = if ($Configuration -eq "Release") { "-O2" } else { "-O0" }
$Target = "x86_64-windows-gnu"
$ObjectDir = Join-Path $Output "obj"
New-Item -ItemType Directory -Path $ObjectDir -Force | Out-Null
$LauncherResource = Join-Path $ObjectDir "launcher.res"
$HookResource = Join-Path $ObjectDir "hook.res"
& $ZigExecutable rc /nologo /fo $LauncherResource (Join-Path $ProjectRoot "resources\launcher.rc")
if ($LASTEXITCODE -ne 0) { throw "Launcher resource build failed." }
& $ZigExecutable rc /nologo /fo $HookResource (Join-Path $ProjectRoot "resources\hook.rc")
if ($LASTEXITCODE -ne 0) { throw "Hook resource build failed." }

& $ZigExecutable c++ -target $Target -std=c++20 $Optimize -municode `
    (Join-Path $ProjectRoot "src\launcher.cpp") $LauncherResource `
    -lshell32 -lole32 -ladvapi32 -o (Join-Path $Output "X4TripleScreen.exe")
if ($LASTEXITCODE -ne 0) { throw "Launcher build failed." }

$MinHookSources = @("buffer.c", "hook.c", "trampoline.c", "hde\hde64.c")
$MinHookObjects = @()
foreach ($Source in $MinHookSources) {
    $Object = Join-Path $ObjectDir (($Source -replace "[\\/]", "_") -replace "\.c$", ".o")
    & $ZigExecutable cc -target $Target $Optimize -DUNICODE -D_UNICODE `
        -I (Join-Path $MinHook "include") -I (Join-Path $MinHook "src") `
        -c (Join-Path $MinHook "src\$Source") -o $Object
    if ($LASTEXITCODE -ne 0) { throw "MinHook build failed for $Source." }
    $MinHookObjects += $Object
}

& $ZigExecutable c++ -target $Target -std=c++20 $Optimize -shared -DUNICODE -D_UNICODE `
    (Join-Path $ProjectRoot "src\hook.cpp") $MinHookObjects $HookResource `
    -I (Join-Path $MinHook "include") -I (Join-Path $MinHook "src") `
    -luser32 -ladvapi32 -o (Join-Path $Output "X4TripleScreenHook.dll")
if ($LASTEXITCODE -ne 0) { throw "Hook DLL build failed." }

Copy-Item -LiteralPath (Join-Path $ProjectRoot "x4-triple-screen.ini") -Destination $Output -Force
Copy-Item -LiteralPath (Join-Path $ProjectRoot "README.md") -Destination $Output -Force -ErrorAction SilentlyContinue
Copy-Item -LiteralPath (Join-Path $ProjectRoot "LICENSE") -Destination $Output -Force -ErrorAction SilentlyContinue
Copy-Item -LiteralPath (Join-Path $ProjectRoot "THIRD-PARTY-NOTICES.md") -Destination $Output -Force -ErrorAction SilentlyContinue
Copy-Item -LiteralPath (Join-Path $ProjectRoot "CHANGELOG.md") -Destination $Output -Force -ErrorAction SilentlyContinue
Copy-Item -LiteralPath (Join-Path $ProjectRoot "CONTRIBUTING.md") -Destination $Output -Force -ErrorAction SilentlyContinue
$OutputDocs = Join-Path $Output "docs"
New-Item -ItemType Directory -Path $OutputDocs -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $ProjectRoot "docs\HOW-IT-WORKS.md") -Destination $OutputDocs -Force
Copy-Item -LiteralPath (Join-Path $ProjectRoot "docs\SETUP-MIXED-RESOLUTION.md") -Destination $OutputDocs -Force

Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $Output "X4TripleScreen.exe"), (Join-Path $Output "X4TripleScreenHook.dll") |
    ForEach-Object { "{0}  {1}" -f $_.Hash.ToLowerInvariant(), (Split-Path -Leaf $_.Path) } |
    Set-Content -LiteralPath (Join-Path $Output "SHA256SUMS.txt") -Encoding ascii

Write-Host "Built $Configuration artifacts in $Output"
