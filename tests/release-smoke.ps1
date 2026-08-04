param(
    [Parameter(Mandatory)]
    [string]$ReleaseDirectory,
    [Parameter(Mandatory)]
    [string]$Version,
    [Parameter(Mandatory)]
    [string]$PackageZip
)

$ErrorActionPreference = "Stop"
$ReleaseDirectory = (Resolve-Path -LiteralPath $ReleaseDirectory).Path
$PackageZip = (Resolve-Path -LiteralPath $PackageZip).Path
$Launcher = Join-Path $ReleaseDirectory "X4TripleScreen.exe"
$Hook = Join-Path $ReleaseDirectory "X4TripleScreenHook.dll"
$Sums = Join-Path $ReleaseDirectory "SHA256SUMS.txt"

& $Launcher --self-test
if ($LASTEXITCODE -ne 0) { throw "Built-in self-tests failed with exit code $LASTEXITCODE." }

$LauncherVersion = (Get-Item -LiteralPath $Launcher).VersionInfo.ProductVersion
$HookVersion = (Get-Item -LiteralPath $Hook).VersionInfo.ProductVersion
if ($LauncherVersion -ne $Version) { throw "Launcher version is '$LauncherVersion', expected '$Version'." }
if ($HookVersion -ne $Version) { throw "Hook version is '$HookVersion', expected '$Version'." }

$Entries = Get-Content -LiteralPath $Sums
if ($Entries.Count -ne 2) { throw "SHA256SUMS.txt must contain exactly two entries." }
foreach ($Entry in $Entries) {
    if ($Entry -notmatch '^([0-9a-f]{64})  (.+)$') { throw "Invalid checksum line: $Entry" }
    $Expected = $Matches[1]
    $File = Join-Path $ReleaseDirectory $Matches[2]
    $Actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $File).Hash.ToLowerInvariant()
    if ($Actual -ne $Expected) { throw "Checksum mismatch for $File." }
}

$ArchiveEntries = tar -tf $PackageZip
$Required = @(
    "X4TripleScreen-v$Version/X4TripleScreen.exe",
    "X4TripleScreen-v$Version/X4TripleScreenHook.dll",
    "X4TripleScreen-v$Version/x4-triple-screen.ini",
    "X4TripleScreen-v$Version/SHA256SUMS.txt",
    "X4TripleScreen-v$Version/README.md",
    "X4TripleScreen-v$Version/RELEASE-NOTES.md"
)
foreach ($RequiredEntry in $Required) {
    if ($ArchiveEntries -notcontains $RequiredEntry) { throw "Package is missing $RequiredEntry." }
}
if ($ArchiveEntries | Where-Object { $_ -match '(?i)(\.pdb$|\.log$|/obj/)' }) {
    throw "Package contains development or log files."
}

Write-Host "Release smoke tests passed for $Version."
