<#
.SYNOPSIS
  Run the static analyzers on the library sources and write reports to build\analysis\.

.DESCRIPTION
  1. MSVC code analysis (/analyze) via the win-analyze preset
  2. clang-tidy (checks in .clang-tidy) using the compile database of -Preset
  3. cppcheck (warning, performance, portability) using the same compile database

  Prints a summary of the finding counts. Reports: build\analysis\{msvc-analyze,clang-tidy,cppcheck}.txt

.PARAMETER Preset
  Configure preset that provides compile_commands.json for clang-tidy and cppcheck (default win-debug).

.PARAMETER Strict
  Exit with code 1 if any analyzer reports a finding.
#>
[CmdletBinding()]
param(
    [string]$Preset = 'win-debug',
    [switch]$Strict
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'devshell.ps1')

function Invoke-Native {
    $exe = $args[0]; $rest = @(); if ($args.Count -gt 1) { $rest = $args[1..($args.Count - 1)] }
    & $exe @rest
    if ($LASTEXITCODE -ne 0) { throw "'$exe $($rest -join ' ')' failed with exit code $LASTEXITCODE" }
}

function Invoke-Capture {
    # Returns stdout+stderr as strings. Windows PowerShell turns redirected native stderr into error
    # records, which must not be fatal here (analyzers report progress and findings on stderr).
    $exe = $args[0]; $rest = @(); if ($args.Count -gt 1) { $rest = $args[1..($args.Count - 1)] }
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & $exe @rest 2>&1 | ForEach-Object { "$_" } }
    finally { $ErrorActionPreference = $old }
}

Push-Location $repo
try {
    $out = Join-Path $repo 'build\analysis'
    New-Item -ItemType Directory -Force $out | Out-Null
    # Library sources only: tracked .c files in the repository root.
    $sources = @(git ls-files '*.c' | Where-Object { $_ -notmatch '/' })
    $summary = [ordered]@{}

    # 1. MSVC /analyze (clean build so that every file is analyzed)
    Invoke-Native cmake --preset win-analyze | Out-Null
    $msvc = Invoke-Capture cmake --build --preset win-analyze --clean-first
    Set-Content -Path (Join-Path $out 'msvc-analyze.txt') -Value @($msvc)
    $summary['MSVC /analyze (C6xxx/C28xxx)'] = @($msvc | Select-String -Pattern 'warning C(6|28)\d{3}' | Select-Object -ExpandProperty Line -Unique).Count

    # 2. clang-tidy
    Invoke-Native cmake --preset $Preset | Out-Null
    $db = Join-Path $repo "build\$Preset"
    $tidy = Invoke-Capture clang-tidy -p $db --quiet @sources
    Set-Content -Path (Join-Path $out 'clang-tidy.txt') -Value @($tidy)
    $summary['clang-tidy'] = @($tidy | Select-String -Pattern ': (warning|error): ' | Select-Object -ExpandProperty Line -Unique).Count

    # 3. cppcheck
    $filters = $sources | ForEach-Object { "--file-filter=*$_" }
    $cpp = Invoke-Capture cppcheck "--project=$(Join-Path $db 'compile_commands.json')" '--enable=warning,performance,portability' `
        --std=c99 --inline-suppr --quiet '--template={file}:{line}: {severity}: {message} [{id}]' @filters
    Set-Content -Path (Join-Path $out 'cppcheck.txt') -Value @($cpp)
    $summary['cppcheck'] = @($cpp | Select-String -Pattern ': (error|warning|performance|portability): ').Count

    Write-Host ''
    Write-Host "Static analysis summary (reports in $out):"
    $total = 0
    foreach ($k in $summary.Keys) { Write-Host ("  {0,-30} {1}" -f $k, $summary[$k]); $total += $summary[$k] }
    if ($Strict -and $total -gt 0) { exit 1 }
}
finally { Pop-Location }
