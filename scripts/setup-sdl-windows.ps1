<#
.SYNOPSIS
  Clone, build and install the SDL version used by this repository (Windows, MSVC x64).

.DESCRIPTION
  The same script ships with SDL_gfx, SDL2_gfx and SDL3_gfx. It detects which SDL major version the
  repository needs (or use -Sdl) and creates this layout below -DepsRoot:

      external\<name>    upstream git clone, pinned to a release tag / commit
      build\<name>       build tree
      install\sdl<N>     install prefix (SDL headers, import libs, DLLs and CMake/pkg-config files)

  The CMake presets of the gfx repositories use <repo>\..\install\sdl<N> as CMAKE_PREFIX_PATH, so the
  default -DepsRoot (the parent folder of the repository) works without further configuration.

  Requires: Git, CMake >= 3.21, Visual Studio 2022+ with the C++ x64 toolset (Ninja and MSBuild from VS).

.PARAMETER Sdl
  3          SDL3  (libsdl-org/SDL, CMake)
  2          SDL2  (libsdl-org/SDL, CMake)
  1-classic  classic SDL 1.2 (libsdl-org/SDL-1.2, VisualC solution via MSBuild)
  1-compat   sdl12-compat (SDL 1.2 API on top of SDL2; builds SDL2 first if missing)
  Default: 3/2/1-classic, detected from the gfx sources next to this script.

.PARAMETER DepsRoot
  Root folder for external\, build\ and install\. Default: parent folder of this repository.

.PARAMETER Ref
  Git tag or commit to build. Default: the pinned version listed in $Pins below.

.PARAMETER Config
  Build configuration (default RelWithDebInfo; classic SDL 1.2 maps it to its Release configuration).

.PARAMETER Clean
  Delete the build tree (and, for classic SDL 1.2, the in-tree MSBuild output) before building.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\scripts\setup-sdl-windows.ps1

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\scripts\setup-sdl-windows.ps1 -Sdl 1-compat
#>
[CmdletBinding()]
param(
    [ValidateSet('3', '2', '1-classic', '1-compat')]
    [string]$Sdl,
    [string]$DepsRoot,
    [string]$Ref,
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Config = 'RelWithDebInfo',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3

# Pinned upstream versions (update deliberately; tests/goldens are validated against these).
$Pins = @{
    '3'         = @{ Url = 'https://github.com/libsdl-org/SDL.git';          Ref = 'release-3.4.20';  Name = 'SDL3';         Prefix = 'sdl3' }
    '2'         = @{ Url = 'https://github.com/libsdl-org/SDL.git';          Ref = 'release-2.32.10'; Name = 'SDL2';         Prefix = 'sdl2' }
    '1-classic' = @{ Url = 'https://github.com/libsdl-org/SDL-1.2.git';      Ref = '81490298a02fe6806c05ac7b5afde7d88fcc7bf9'; Name = 'SDL-1.2'; Prefix = 'sdl1' }
    '1-compat'  = @{ Url = 'https://github.com/libsdl-org/sdl12-compat.git'; Ref = 'release-1.2.78';  Name = 'sdl12-compat'; Prefix = 'sdl1' }
}

$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $DepsRoot) { $DepsRoot = Split-Path -Parent $RepoRoot }
$DepsRoot = [IO.Path]::GetFullPath($DepsRoot)

if (-not $Sdl) {
    if (Test-Path (Join-Path $RepoRoot 'SDL3_gfxPrimitives.h')) { $Sdl = '3' }
    elseif (Test-Path (Join-Path $RepoRoot 'SDL2_gfxPrimitives.h')) { $Sdl = '2' }
    elseif (Test-Path (Join-Path $RepoRoot 'SDL_gfxPrimitives.h')) { $Sdl = '1-classic' }
    else { throw "Cannot detect the SDL version for $RepoRoot; pass -Sdl." }
}

$pin = $Pins[$Sdl]
if (-not $Ref) { $Ref = $pin.Ref }
$src = Join-Path $DepsRoot "external\$($pin.Name)"
$bld = Join-Path $DepsRoot "build\$($pin.Name)"
$prefix = Join-Path $DepsRoot "install\$($pin.Prefix)"

function Invoke-Native {
    # Runs a native command and throws on a non-zero exit code.
    $exe = $args[0]
    $rest = @()
    if ($args.Count -gt 1) { $rest = $args[1..($args.Count - 1)] }
    & $exe @rest
    if ($LASTEXITCODE -ne 0) { throw "'$exe $($rest -join ' ')' failed with exit code $LASTEXITCODE" }
}

function Sync-Source([string]$Url, [string]$Dir, [string]$GitRef) {
    if (-not (Test-Path (Join-Path $Dir '.git'))) {
        New-Item -ItemType Directory -Force $Dir | Out-Null
        Invoke-Native git -C $Dir init --quiet
        Invoke-Native git -C $Dir remote add origin $Url
    }
    $target = $GitRef
    git -C $Dir rev-parse --verify --quiet "$GitRef^{commit}" | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Invoke-Native git -C $Dir fetch --quiet --depth 1 origin $GitRef
        $target = 'FETCH_HEAD'
    }
    Invoke-Native git -C $Dir -c advice.detachedHead=false checkout --quiet --force $target
    Write-Host "$Dir @ $(git -C $Dir rev-parse --short HEAD) ($GitRef)"
}

function Build-CMakeSdl([string[]]$ExtraArgs) {
    if ($Clean -and (Test-Path $bld)) { Remove-Item -Recurse -Force $bld }
    $cfg = @('-S', $src, '-B', $bld, '-G', 'Ninja', "-DCMAKE_BUILD_TYPE=$Config", "-DCMAKE_INSTALL_PREFIX=$prefix") + $ExtraArgs
    Invoke-Native cmake @cfg
    Invoke-Native cmake --build $bld
    Invoke-Native cmake --install $bld
}

function Get-PlatformToolset {
    switch -Wildcard ($env:VisualStudioVersion) {
        '18.*' { return 'v145' }
        '17.*' { return 'v143' }
        default { throw "Unsupported Visual Studio version '$env:VisualStudioVersion' (need 2022 or later)." }
    }
}

Write-Host "SDL $Sdl -> $prefix"

# Classic SDL 1.2 and sdl12-compat share install\sdl1; never mix their files.
$providerFile = Join-Path $prefix 'sdl-provider.txt'
if ((Test-Path $providerFile) -and ((Get-Content $providerFile -Raw).Split(' ')[0] -ne $pin.Name)) {
    Write-Host "Removing $prefix (installed by a different SDL provider)"
    Remove-Item -Recurse -Force $prefix
}

. (Join-Path $PSScriptRoot 'devshell.ps1')
Sync-Source $pin.Url $src $Ref

switch ($Sdl) {
    '3' {
        Build-CMakeSdl @('-DSDL_TEST_LIBRARY=ON', '-DSDL_TESTS=OFF', '-DSDL_EXAMPLES=OFF', '-DSDL_INSTALL_DOCS=OFF')
    }
    '2' {
        Build-CMakeSdl @('-DSDL_TEST=ON', '-DSDL_TESTS=OFF')
    }
    '1-compat' {
        $sdl2Prefix = Join-Path $DepsRoot 'install\sdl2'
        if (-not (Test-Path (Join-Path $sdl2Prefix 'cmake\SDL2Config.cmake'))) {
            Write-Host 'SDL2 not installed yet - building it first (sdl12-compat loads SDL2 at runtime).'
            & $PSCommandPath -Sdl 2 -DepsRoot $DepsRoot -Config $Config
        }
        Build-CMakeSdl @("-DCMAKE_PREFIX_PATH=$sdl2Prefix", '-DSDL12TESTS=OFF', '-DSDL12DEVEL=ON')
        Copy-Item (Join-Path $sdl2Prefix 'bin\SDL2.dll') (Join-Path $prefix 'bin') -Force
    }
    '1-classic' {
        # The VisualC projects need SDL_config.h, which is not generated on Windows.
        $cfgHeader = Join-Path $src 'include\SDL_config.h'
        if (-not (Test-Path $cfgHeader)) { Copy-Item (Join-Path $src 'include\SDL_config.h.default') $cfgHeader }

        $msCfg = if ($Config -eq 'Debug') { 'Debug' } else { 'Release' }
        $outDirs = @((Join-Path $src "VisualC\SDL\x64\$msCfg"), (Join-Path $src "VisualC\SDLmain\x64\$msCfg"))
        if ($Clean) { $outDirs | Where-Object { Test-Path $_ } | ForEach-Object { Remove-Item -Recurse -Force $_ } }

        # The 2012-era projects build in-tree (VisualC\<project>\x64\<config>) and are retargeted on the fly.
        Invoke-Native msbuild (Join-Path $src 'VisualC\SDL_VS2010.sln') /m /nologo /v:m `
            "/p:Configuration=$msCfg" /p:Platform=x64 "/p:PlatformToolset=$(Get-PlatformToolset)" /p:WindowsTargetPlatformVersion=10.0

        foreach ($d in 'include\SDL', 'lib', 'bin') { New-Item -ItemType Directory -Force (Join-Path $prefix $d) | Out-Null }
        Copy-Item (Join-Path $src 'include\*.h') (Join-Path $prefix 'include\SDL') -Force
        Copy-Item (Join-Path $outDirs[0] 'SDL.lib'), (Join-Path $outDirs[1] 'SDLmain.lib') (Join-Path $prefix 'lib') -Force
        Copy-Item (Join-Path $outDirs[0] 'SDL.dll') (Join-Path $prefix 'bin') -Force
        $pdb = Join-Path $outDirs[0] 'SDL.pdb'
        if (Test-Path $pdb) { Copy-Item $pdb (Join-Path $prefix 'bin') -Force }
        Copy-Item (Join-Path $src 'COPYING') (Join-Path $prefix 'COPYING-SDL-1.2.txt') -Force
    }
}

Set-Content -Path $providerFile -Value "$($pin.Name) $Ref" -Encoding ascii
Write-Host "Done: $($pin.Name) ($Ref) installed to $prefix"
