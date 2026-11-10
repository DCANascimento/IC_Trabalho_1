param([string]$ExecutablePath)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
    $ExecutablePath = Join-Path $PSScriptRoot 'build\MyLosslessCodec.exe'
}
if (-not [System.IO.Path]::IsPathRooted($ExecutablePath)) {
    $ExecutablePath = Join-Path $PSScriptRoot $ExecutablePath
}
$exePath = $ExecutablePath
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "Executable not found at '$exePath'. Build the project first."
}
$exePath = (Resolve-Path -LiteralPath $exePath).Path
$verbs = @(
    @{ Extension = '.wav'; Name = 'LosslessCodec.Compress'; Label = 'Compress to CLSS'; Args = '-c' },
    @{ Extension = '.clss'; Name = 'LosslessCodec.Decompress'; Label = 'Decompress to WAV'; Args = '-d' }
)
foreach ($verb in $verbs) {
    $baseKey = "Software\Classes\SystemFileAssociations\$($verb.Extension)\shell\$($verb.Name)"
    $key = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($baseKey)
    $key.SetValue('', $verb.Label, [Microsoft.Win32.RegistryValueKind]::String)
    $key.SetValue('Icon', $exePath, [Microsoft.Win32.RegistryValueKind]::String)
    $commandKey = $key.CreateSubKey('command')
    $commandKey.SetValue('', ('"' + $exePath + '" ' + $verb.Args + ' "%1"'), [Microsoft.Win32.RegistryValueKind]::String)
    $commandKey.Dispose()
    $key.Dispose()
}
Write-Host 'Context menu actions installed for WAV and CLSS (current user).'
