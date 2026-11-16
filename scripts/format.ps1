<#
.SYNOPSIS
  Format (or check the formatting of) all C sources and headers with clang-format.

.DESCRIPTION
  Uses .clang-format in the repository root; files listed in .clang-format-ignore are skipped.

.PARAMETER Check
  Do not modify files; exit with code 1 if any file is not formatted.
#>
[CmdletBinding()]
param([switch]$Check)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

$cf = Get-Command clang-format -ErrorAction SilentlyContinue
$cf = if ($cf) { $cf.Source } else { Join-Path $env:ProgramFiles 'LLVM\bin\clang-format.exe' }
if (-not (Test-Path $cf)) { throw 'clang-format not found (run scripts\setup-tools-windows.ps1).' }

$ignoreFile = Join-Path $repo '.clang-format-ignore'
$ignore = @()
if (Test-Path $ignoreFile) {
    $ignore = Get-Content $ignoreFile | ForEach-Object { $_.Trim() } | Where-Object { $_ -and -not $_.StartsWith('#') }
}

Push-Location $repo
try {
    $files = @(git ls-files '*.c' '*.h' | Where-Object { $f = $_; -not ($ignore | Where-Object { $f -like $_ }) })
    Write-Host "clang-format ($(& $cf --version)) on $($files.Count) files"
    if ($Check) {
        & $cf --dry-run --Werror @files
        if ($LASTEXITCODE -ne 0) { Write-Host 'Formatting issues found; run scripts\format.ps1'; exit 1 }
        Write-Host 'All files formatted.'
    }
    else {
        & $cf -i @files
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}
finally { Pop-Location }
