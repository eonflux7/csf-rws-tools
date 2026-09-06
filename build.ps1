[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Release',

    [string]$Target,

    [switch]$CoreOnly,

    [switch]$Reconfigure
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-CMake {
    param([Parameter(Mandatory)][string[]]$Arguments)

    & cmake @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "CMake failed with exit code $LASTEXITCODE."
    }
}

$configurePreset = if ($CoreOnly) { 'core' } else { 'default' }
$presetPrefix = if ($CoreOnly) { 'core-' } else { '' }
$buildPreset = $presetPrefix + $Config.ToLowerInvariant()
$buildDirectory = if ($CoreOnly) { 'build-core' } else { 'build' }
$cachePath = Join-Path $PSScriptRoot "$buildDirectory/CMakeCache.txt"

Push-Location $PSScriptRoot
try {
    if ($Reconfigure -or -not (Test-Path -LiteralPath $cachePath)) {
        Invoke-CMake @('--preset', $configurePreset)
    }

    $arguments = @('--build', '--preset', $buildPreset, '--parallel')
    if ($Target) {
        $arguments += @('--target', $Target)
    }
    Invoke-CMake $arguments
}
finally {
    Pop-Location
}
