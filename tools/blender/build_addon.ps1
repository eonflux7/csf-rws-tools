$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot 'rws_lightmaps'
$output = Join-Path $PSScriptRoot 'rws_lightmaps.zip'
if (Test-Path -LiteralPath $output) {
    Remove-Item -LiteralPath $output
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::Open(
    $output, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($name in @('__init__.py', 'README.md')) {
        $path = Join-Path $source $name
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $archive, $path, "rws_lightmaps/$name",
            [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
}
finally {
    $archive.Dispose()
}
Write-Output $output
