param(
    [string]$Zig = "zig",
    [string]$Version = "0.2.1-beta"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ReleaseRoot = Join-Path $ProjectRoot "release"
$FolderName = "X4TripleScreen-v$Version"
$PackageRoot = Join-Path $ReleaseRoot $FolderName
$ZipPath = Join-Path $ReleaseRoot "$FolderName.zip"
$ZipHashPath = "$ZipPath.sha256"
$ReleaseNotes = Join-Path $ProjectRoot "docs\RELEASE-NOTES-$Version.md"

if (-not (Test-Path -LiteralPath $ReleaseNotes)) {
    throw "Release notes were not found: $ReleaseNotes"
}

& (Join-Path $ProjectRoot "build.ps1") -Zig $Zig -Configuration Release

New-Item -ItemType Directory -Path $PackageRoot -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $PackageRoot "docs") -Force | Out-Null

$RootFiles = @(
    "X4TripleScreen.exe",
    "X4TripleScreenHook.dll",
    "x4-triple-screen.ini",
    "README.md",
    "LICENSE",
    "THIRD-PARTY-NOTICES.md",
    "CHANGELOG.md",
    "CONTRIBUTING.md",
    "SHA256SUMS.txt"
)
foreach ($File in $RootFiles) {
    Copy-Item -LiteralPath (Join-Path $ReleaseRoot $File) -Destination $PackageRoot -Force
}

Copy-Item -LiteralPath (Join-Path $ProjectRoot "docs\HOW-IT-WORKS.md") -Destination (Join-Path $PackageRoot "docs") -Force
Copy-Item -LiteralPath (Join-Path $ProjectRoot "docs\SETUP-MIXED-RESOLUTION.md") -Destination (Join-Path $PackageRoot "docs") -Force
Copy-Item -LiteralPath $ReleaseNotes -Destination (Join-Path $PackageRoot "RELEASE-NOTES.md") -Force

Compress-Archive -LiteralPath $PackageRoot -DestinationPath $ZipPath -CompressionLevel Optimal -Force
$ZipHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ZipPath).Hash.ToLowerInvariant()
"$ZipHash  $FolderName.zip" | Set-Content -LiteralPath $ZipHashPath -Encoding ascii

Write-Host "Packaged $ZipPath"
