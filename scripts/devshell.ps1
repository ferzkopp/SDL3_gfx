<#
.SYNOPSIS
  Enter the Visual Studio (MSVC) x64 developer environment in the current PowerShell session.

.DESCRIPTION
  Dot-source this script so that cl.exe, link.exe, msbuild.exe, the VS-bundled CMake/Ninja and the
  Windows SDK are on PATH:

      . .\scripts\devshell.ps1

  The newest Visual Studio installation (2022 or later) that contains the C++ x64 toolset is used.
  Calling it again in an already-initialized session is a no-op.

.PARAMETER Arch
  Target architecture passed to VsDevCmd (default: amd64).
#>
[CmdletBinding()]
param(
    [ValidateSet('amd64', 'x86', 'arm64')]
    [string]$Arch = 'amd64'
)

$__vsTarget = @{ amd64 = 'x64'; x86 = 'x86'; arm64 = 'arm64' }[$Arch]
if ($env:VSCMD_ARG_TGT_ARCH -eq $__vsTarget -and (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    return
}

$__vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $__vswhere)) {
    throw 'vswhere.exe not found. Install Visual Studio 2022 or later with the "Desktop development with C++" workload.'
}

$__vsPath = & $__vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $__vsPath) {
    throw 'No Visual Studio installation with the MSVC x64 toolset found (component Microsoft.VisualStudio.Component.VC.Tools.x86.x64).'
}

Import-Module (Join-Path $__vsPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $__vsPath -SkipAutomaticLocation -DevCmdArguments "-arch=$Arch -host_arch=amd64 -no_logo" | Out-Null

# LLVM (clang-format, clang-tidy) and Cppcheck installers do not add themselves to PATH.
foreach ($__dir in @("$env:ProgramFiles\LLVM\bin", "$env:ProgramFiles\Cppcheck")) {
    if ((Test-Path $__dir) -and (($env:PATH -split ';') -notcontains $__dir)) { $env:PATH = "$__dir;$env:PATH" }
}
Write-Host "MSVC developer environment: $__vsPath ($Arch)"
