[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Release',

    [switch]$CoreOnly,

    [switch]$NoBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-Native {
    param(
        [Parameter(Mandatory)][string]$Command,
        [Parameter(Mandatory)][string[]]$Arguments
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE."
    }
}

$configurePreset = if ($CoreOnly) { 'core' } else { 'default' }
$presetPrefix = if ($CoreOnly) { 'core-' } else { '' }
$testPreset = $presetPrefix + $Config.ToLowerInvariant()
$buildDirectory = if ($CoreOnly) { 'build-core' } else { 'build' }
$cachePath = Join-Path $PSScriptRoot "$buildDirectory/CMakeCache.txt"

Push-Location $PSScriptRoot
try {
    if (-not (Test-Path -LiteralPath $cachePath)) {
        Invoke-Native 'cmake' @('--preset', $configurePreset)
    }

    if (-not $NoBuild) {
        Invoke-Native 'cmake' @(
            '--build', '--preset', $testPreset, '--target', 'rws_core_tests', '--parallel'
        )
    }

    Invoke-Native 'ctest' @('--preset', $testPreset)
}
finally {
    Pop-Location
}
