$ErrorActionPreference = 'Stop'
$keys = @(
    'HKCU:\Software\Classes\SystemFileAssociations\.wav\shell\LosslessCodec.Compress',
    'HKCU:\Software\Classes\SystemFileAssociations\.clss\shell\LosslessCodec.Decompress'
)
foreach ($key in $keys) {
    if (Test-Path -LiteralPath $key) {
        Remove-Item -LiteralPath $key -Recurse -Force
    }
}
Write-Host 'Context menu actions removed for the current user.'
