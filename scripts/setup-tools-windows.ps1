<#
.SYNOPSIS
  Install or verify the Windows developer tools used by this repository.

.DESCRIPTION
  Installs (system-wide, via winget) the tools that are not part of Visual Studio, then reports the
  status of every required tool. Already installed packages are skipped. Open a NEW terminal after
  installation so PATH changes take effect.

  Required, installed separately:
    - Git, CMake >= 3.21, Python 3
    - Visual Studio 2022 or later with "Desktop development with C++"
      (+ component "C++ AddressSanitizer" for the fuzz preset)

.PARAMETER CheckOnly
  Only report the tool status; do not install anything.

.PARAMETER NoOptional
  Skip optional tools (Graphviz, GitHub CLI).
#>
[CmdletBinding()]
param(
    [switch]$CheckOnly,
    [switch]$NoOptional
)

$ErrorActionPreference = 'Stop'

$packages = @(
    @{ Id = 'Ninja-build.Ninja';         Cmd = 'ninja';        Optional = $false; Purpose = 'build tool for the CMake presets' },
    @{ Id = 'LLVM.LLVM';                 Cmd = 'clang-format'; Optional = $false; Purpose = 'clang-format / clang-tidy' },
    @{ Id = 'DimitriVanHeesch.Doxygen';  Cmd = 'doxygen';      Optional = $false; Purpose = 'API documentation' },
    @{ Id = 'Cppcheck.Cppcheck';         Cmd = 'cppcheck';     Optional = $false; Purpose = 'static analysis' },
    @{ Id = 'Graphviz.Graphviz';         Cmd = 'dot';          Optional = $true;  Purpose = 'Doxygen diagrams' },
    @{ Id = 'GitHub.cli';                Cmd = 'gh';           Optional = $true;  Purpose = 'GitHub releases / repository settings' }
)

# Well-known install folders that winget does not always add to PATH of the current session.
$extraPaths = @(
    "$env:ProgramFiles\LLVM\bin",
    "$env:ProgramFiles\doxygen\bin",
    "$env:ProgramFiles\Cppcheck",
    "$env:ProgramFiles\Graphviz\bin",
    "$env:ProgramFiles\GitHub CLI"
)

function Update-SessionPath {
    # Pick up PATH changes made by installers (registry) without opening a new terminal.
    $machine = [Environment]::GetEnvironmentVariable('PATH', 'Machine')
    $user = [Environment]::GetEnvironmentVariable('PATH', 'User')
    $parts = @($env:PATH -split ';') + @($machine -split ';') + @($user -split ';') + $extraPaths
    $seen = @{}
    $env:PATH = (($parts | Where-Object { $_ -and (Test-Path $_) -and -not $seen.ContainsKey($_.ToLower()) -and ($seen[$_.ToLower()] = $true) }) -join ';')
}
Update-SessionPath

if (-not $CheckOnly) {
    if (-not (Get-Command winget -ErrorAction SilentlyContinue)) { throw 'winget not found (install "App Installer" from the Microsoft Store).' }
    foreach ($pkg in $packages) {
        if ($NoOptional -and $pkg.Optional) { continue }
        if (Get-Command $pkg.Cmd -ErrorAction SilentlyContinue) { continue }
        Write-Host "Installing $($pkg.Id) ($($pkg.Purpose))"
        winget install --exact --id $pkg.Id --silent --accept-package-agreements --accept-source-agreements
        if ($LASTEXITCODE -ne 0) { Write-Warning "winget install $($pkg.Id) returned $LASTEXITCODE" }
    }
    Update-SessionPath
}

Write-Host ''
Write-Host 'Tool status:'
$missing = 0
$required = @('git', 'cmake', 'python') + ($packages | Where-Object { -not $_.Optional } | ForEach-Object { $_.Cmd }) + @('clang-tidy')
$optional = $packages | Where-Object { $_.Optional } | ForEach-Object { $_.Cmd }
foreach ($cmd in $required + $optional) {
    $c = Get-Command $cmd -ErrorAction SilentlyContinue
    $tag = if ($optional -contains $cmd) { '(optional)' } else { '' }
    if ($c) { Write-Host ("  {0,-13} {1}" -f $cmd, $c.Source) }
    else {
        Write-Host ("  {0,-13} MISSING {1}" -f $cmd, $tag)
        if (-not $tag) { $missing++ }
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $asan = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.ASAN -property installationPath
    Write-Host ("  {0,-13} {1}" -f 'MSVC', $(if ($vs) { $vs } else { 'MISSING' }))
    Write-Host ("  {0,-13} {1}" -f 'MSVC ASan', $(if ($asan) { 'installed' } else { 'MISSING (needed for the win-fuzz preset)' }))
    if (-not $vs) { $missing++ }
    if ($vs -and -not $asan) {
        Write-Host ''
        Write-Host 'Add the AddressSanitizer component (elevation prompt) with:'
        Write-Host "  & `"$(Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\setup.exe')`" modify --installPath `"$vs`" --add Microsoft.VisualStudio.Component.VC.ASAN --passive --norestart"
    }
}
else {
    Write-Host '  Visual Studio  MISSING'
    $missing++
}

if ($missing) { Write-Warning "$missing required tool(s) missing. Open a new terminal after installing and re-run with -CheckOnly." }
