param(
    [string]$ToolsRoot = (Join-Path $env:TEMP 'lossless-codec-benchmark-20261018'),
    [int]$Repetitions = 3,
    [switch]$MemoryOnly
)

$ErrorActionPreference = 'Stop'
[System.Globalization.CultureInfo]::CurrentCulture = [System.Globalization.CultureInfo]::InvariantCulture

$projectRoot = $PSScriptRoot
$scratchRoot = Join-Path $env:TEMP ('codec-comparison-runs-' + [guid]::NewGuid().ToString('N'))
$runRoot = Join-Path $scratchRoot 'runs'
$csvSuffix = if ($MemoryOnly) { '-memoria' } else { '' }
$rawCsv = Join-Path $projectRoot "benchmark-comparacao$csvSuffix.csv"
$summaryCsv = Join-Path $projectRoot "benchmark-comparacao-resumo$csvSuffix.csv"

if (-not $MemoryOnly -and $Repetitions -lt 3) {
    throw 'Use at least three repetitions to reduce the effect of temporary variations.'
}
if ((Test-Path -LiteralPath $rawCsv) -or (Test-Path -LiteralPath $summaryCsv)) {
    throw 'The comparison CSV files already exist and will not be overwritten.'
}

$flac = Join-Path $ToolsRoot 'flac\flac-1.5.0-win\Win64\flac.exe'
$wavpack = Join-Path $ToolsRoot 'wavpack\wavpack.exe'
$wvunpack = Join-Path $ToolsRoot 'wavpack\wvunpack.exe'
$takc = Join-Path $ToolsRoot 'tak\Applications\Takc.exe'
$zstd = Join-Path $ToolsRoot 'zstd\zstd-v1.5.7-win64\zstd.exe'
$xz = Join-Path $ToolsRoot 'xz\bin_x86-64\xz.exe'

$requiredTools = @($flac, $wavpack, $wvunpack, $takc, $zstd, $xz)
foreach ($tool in $requiredTools) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "Required executable not found: $tool"
    }
}

# Mede apenas a execução do codec; a preparação dos ficheiros e a validação ficam fora do cronómetro.
function Invoke-MeasuredProcess {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$WorkingDirectory
    )

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in $Arguments) {
        [void]$startInfo.ArgumentList.Add([string]$argument)
    }

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    if (-not $process.Start()) {
        throw "Could not start $Executable"
    }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()

    # No Windows, as propriedades de memória podem ficar vazias depois da saída.
    # Por isso recolhemos o pico acumulado enquanto o processo ainda está ativo.
    $peakBytes = 0L
    while ($true) {
        try {
            $process.Refresh()
            $reportedPeak = $process.PeakWorkingSet64
            $currentWorkingSet = $process.WorkingSet64
            if ($null -ne $reportedPeak -and [long]$reportedPeak -gt $peakBytes) {
                $peakBytes = [long]$reportedPeak
            }
            if ($null -ne $currentWorkingSet -and [long]$currentWorkingSet -gt $peakBytes) {
                $peakBytes = [long]$currentWorkingSet
            }
        }
        catch {
            # O processo pode terminar entre WaitForExit e a leitura dos contadores.
        }
        if ($process.WaitForExit(5)) {
            break
        }
    }
    $timer.Stop()
    $exitCode = $process.ExitCode
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    $process.Dispose()

    if ($exitCode -ne 0) {
        throw "Process failed ($exitCode): $Executable $($Arguments -join ' ')`n$stderr`n$stdout"
    }

    [pscustomobject]@{
        Seconds = $timer.Elapsed.TotalSeconds
        PeakMiB = $peakBytes / 1MB
    }
}

# Lê o formato e a posição do bloco de amostras sem guardar o áudio completo em memória.
function Get-WavInfo {
    param([string]$Path)

    $stream = [System.IO.File]::OpenRead($Path)
    $reader = [System.IO.BinaryReader]::new($stream)
    try {
        if ([System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(4)) -ne 'RIFF') {
            throw "File does not start with RIFF: $Path"
        }
        [void]$reader.ReadUInt32()
        if ([System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(4)) -ne 'WAVE') {
            throw "RIFF file does not contain WAVE: $Path"
        }

        $format = $null
        $dataOffset = -1L
        $dataBytes = -1L
        while ($stream.Position + 8 -le $stream.Length) {
            $chunkId = [System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(4))
            $chunkBytes = [long]$reader.ReadUInt32()
            $chunkOffset = $stream.Position

            if ($chunkId -eq 'fmt ' -and $chunkBytes -ge 16) {
                $formatBytes = $reader.ReadBytes(16)
                $format = [pscustomobject]@{
                    AudioFormat = [System.BitConverter]::ToUInt16($formatBytes, 0)
                    Channels = [System.BitConverter]::ToUInt16($formatBytes, 2)
                    SampleRate = [System.BitConverter]::ToUInt32($formatBytes, 4)
                    AverageBytesPerSecond = [System.BitConverter]::ToUInt32($formatBytes, 8)
                    BlockAlign = [System.BitConverter]::ToUInt16($formatBytes, 12)
                    BitsPerSample = [System.BitConverter]::ToUInt16($formatBytes, 14)
                }
            }
            elseif ($chunkId -eq 'data') {
                $dataOffset = $chunkOffset
                $dataBytes = $chunkBytes
            }

            $nextChunk = $chunkOffset + $chunkBytes + ($chunkBytes % 2)
            if ($nextChunk -gt $stream.Length) {
                throw "Invalid WAV block size: $Path"
            }
            $stream.Position = $nextChunk
            if ($null -ne $format -and $dataOffset -ge 0) {
                break
            }
        }

        if ($null -eq $format -or $dataOffset -lt 0) {
            throw "Could not locate fmt/data in $Path"
        }
        [pscustomobject]@{
            AudioFormat = $format.AudioFormat
            Channels = $format.Channels
            SampleRate = $format.SampleRate
            AverageBytesPerSecond = $format.AverageBytesPerSecond
            BlockAlign = $format.BlockAlign
            BitsPerSample = $format.BitsPerSample
            DataOffset = $dataOffset
            DataBytes = $dataBytes
        }
    }
    finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

function Get-FileRangeHash {
    param(
        [string]$Path,
        [long]$Offset,
        [long]$Length
    )

    $stream = [System.IO.File]::OpenRead($Path)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $stream.Position = $Offset
        $buffer = [byte[]]::new(1MB)
        $remaining = $Length
        while ($remaining -gt 0) {
            $read = $stream.Read($buffer, 0, [int][Math]::Min($buffer.Length, $remaining))
            if ($read -le 0) {
                throw "Unexpected end of file while validating $Path"
            }
            [void]$sha.TransformBlock($buffer, 0, $read, $buffer, 0)
            $remaining -= $read
        }
        [void]$sha.TransformFinalBlock([byte[]]::new(0), 0, 0)
        [System.Convert]::ToHexString($sha.Hash)
    }
    finally {
        $sha.Dispose()
        $stream.Dispose()
    }
}

function Get-Median {
    param([double[]]$Values)
    $sorted = @($Values | Sort-Object)
    $middle = [int][Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2) {
        return [double]$sorted[$middle]
    }
    return ([double]$sorted[$middle - 1] + [double]$sorted[$middle]) / 2
}

$files = @(Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Audio_Datasets') -Recurse -File -Filter '*.wav' | Sort-Object FullName)
if ($files.Count -ne 20) {
    throw "Expected to find the 20 WAV benchmark samples; found $($files.Count)."
}

$codecs = @('FLAC', 'WavPack', 'TAK', 'Zstandard', 'LZMA')
$settings = @{
    FLAC = 'FLAC 1.5.0; -5; 1 thread'
    WavPack = 'WavPack 5.9.0; normal; --no-threads'
    TAK = 'TAK 2.3.3; -p2 (predefinição); -tn1; executável Takc de 32 bits'
    Zstandard = 'Zstandard 1.5.7; -3 (predefinição); 1 thread'
    LZMA = 'XZ Utils 5.8.4; formato LZMA; -6; 1 thread'
}

$sourceMetadata = @{}
foreach ($file in $files) {
    $info = Get-WavInfo -Path $file.FullName
    if ($info.AudioFormat -ne 1 -or $info.BitsPerSample -ne 16) {
        throw "Unexpected format; this benchmark requires PCM16: $($file.FullName)"
    }
    $sourceMetadata[$file.FullName] = [pscustomobject]@{
        Info = $info
        PcmHash = Get-FileRangeHash -Path $file.FullName -Offset $info.DataOffset -Length $info.DataBytes
        FullHash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        DurationSeconds = $info.DataBytes / $info.AverageBytesPerSecond
    }
}

New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
$records = [System.Collections.Generic.List[object]]::new()
$totalRuns = $files.Count * $codecs.Count * $Repetitions
$completedRuns = 0

for ($pass = 1; $pass -le $Repetitions; $pass++) {
    foreach ($file in $files) {
        # Alterna a ordem por repetição para não favorecer sempre o mesmo codec.
        for ($orderIndex = 0; $orderIndex -lt $codecs.Count; $orderIndex++) {
            $codecIndex = ($orderIndex + $pass - 1) % $codecs.Count
            $codec = $codecs[$codecIndex]
            $caseDirectory = Join-Path $runRoot (Join-Path $codec (Join-Path ('pass-{0:D2}' -f $pass) $file.BaseName))
            New-Item -ItemType Directory -Path $caseDirectory -Force | Out-Null
            $inputPath = Join-Path $caseDirectory 'input.wav'
            $encodedPath = Join-Path $caseDirectory 'encoded.bin'
            $decodedPath = Join-Path $caseDirectory 'restored.wav'
            Copy-Item -LiteralPath $file.FullName -Destination $inputPath

            switch ($codec) {
                'FLAC' {
                    $encodedPath = Join-Path $caseDirectory 'encoded.flac'
                    $encodeResult = Invoke-MeasuredProcess $flac @('-s', '-f', '--threads=1', '-5', '-o', $encodedPath, $inputPath) $caseDirectory
                    $decodeResult = Invoke-MeasuredProcess $flac @('-d', '-s', '-f', '-o', $decodedPath, $encodedPath) $caseDirectory
                }
                'WavPack' {
                    $encodedPath = Join-Path $caseDirectory 'encoded.wv'
                    $encodeResult = Invoke-MeasuredProcess $wavpack @('-q', '--no-threads', $inputPath, $encodedPath) $caseDirectory
                    $decodeResult = Invoke-MeasuredProcess $wvunpack @('-q', '--no-threads', $encodedPath, $decodedPath) $caseDirectory
                }
                'TAK' {
                    $encodedPath = Join-Path $caseDirectory 'encoded.tak'
                    $encodeResult = Invoke-MeasuredProcess $takc @('-e', '-p2', '-tn1', '-silent', $inputPath, $encodedPath) $caseDirectory
                    $decodeResult = Invoke-MeasuredProcess $takc @('-d', '-silent', '-overwrite', $encodedPath, $decodedPath) $caseDirectory
                }
                'Zstandard' {
                    $encodedPath = Join-Path $caseDirectory 'encoded.zst'
                    $encodeResult = Invoke-MeasuredProcess $zstd @('-q', '-f', '-3', '--threads=1', '-o', $encodedPath, $inputPath) $caseDirectory
                    $decodeResult = Invoke-MeasuredProcess $zstd @('-q', '-d', '-f', '--threads=1', '-o', $decodedPath, $encodedPath) $caseDirectory
                }
                'LZMA' {
                    # O modo LZMA do XZ cria input.wav.lzma; removemos a cópia de entrada
                    # antes de descomprimir, para o programa poder reconstruir input.wav.
                    $encodedPath = "$inputPath.lzma"
                    $encodeResult = Invoke-MeasuredProcess $xz @('-q', '-f', '-k', '-6', '-T1', '--format=lzma', $inputPath) $caseDirectory
                    $encodedBytes = (Get-Item -LiteralPath $encodedPath).Length
                    Remove-Item -LiteralPath $inputPath
                    $decodeResult = Invoke-MeasuredProcess $xz @('-q', '-d', '-f', '-k', '-T1', '--format=lzma', $encodedPath) $caseDirectory
                    $decodedPath = $inputPath
                }
            }

            if ($codec -ne 'LZMA') {
                $encodedBytes = (Get-Item -LiteralPath $encodedPath).Length
            }
            $decodedInfo = Get-WavInfo -Path $decodedPath
            $sourceInfo = $sourceMetadata[$file.FullName].Info
            $formatMatches = $decodedInfo.AudioFormat -eq $sourceInfo.AudioFormat `
                -and $decodedInfo.Channels -eq $sourceInfo.Channels `
                -and $decodedInfo.SampleRate -eq $sourceInfo.SampleRate `
                -and $decodedInfo.BitsPerSample -eq $sourceInfo.BitsPerSample `
                -and $decodedInfo.DataBytes -eq $sourceInfo.DataBytes
            $pcmMatches = $formatMatches -and (
                (Get-FileRangeHash -Path $decodedPath -Offset $decodedInfo.DataOffset -Length $decodedInfo.DataBytes) `
                -eq $sourceMetadata[$file.FullName].PcmHash
            )
            $fullWavMatches = (Get-FileHash -LiteralPath $decodedPath -Algorithm SHA256).Hash `
                -eq $sourceMetadata[$file.FullName].FullHash
            if (-not $pcmMatches) {
                throw "PCM validation failed: $codec / $($file.Name) / repetition $pass"
            }
            if (($codec -eq 'Zstandard' -or $codec -eq 'LZMA') -and -not $fullWavMatches) {
                throw "Byte-for-byte WAV validation failed: $codec / $($file.Name) / repetition $pass"
            }

            $records.Add([pscustomobject]@{
                Codec = $codec
                Configuration = $settings[$codec]
                File = $file.Name
                Pass = $pass
                OriginalBytes = [long]$file.Length
                CompressedBytes = [long]$encodedBytes
                EncodeSeconds = $encodeResult.Seconds.ToString('F6', [System.Globalization.CultureInfo]::InvariantCulture)
                DecodeSeconds = $decodeResult.Seconds.ToString('F6', [System.Globalization.CultureInfo]::InvariantCulture)
                EncodePeakWorkingSetMiB = $encodeResult.PeakMiB.ToString('F3', [System.Globalization.CultureInfo]::InvariantCulture)
                DecodePeakWorkingSetMiB = $decodeResult.PeakMiB.ToString('F3', [System.Globalization.CultureInfo]::InvariantCulture)
                PCMIdentical = $pcmMatches
                FullWavByteIdentical = $fullWavMatches
            })

            # Apaga apenas a pasta desta execução, criada pelo próprio benchmark.
            $runRootFull = [System.IO.Path]::GetFullPath($runRoot).TrimEnd('\') + '\'
            $caseFull = [System.IO.Path]::GetFullPath($caseDirectory)
            if (-not $caseFull.StartsWith($runRootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Refusing to clean up outside the benchmark directory: $caseFull"
            }
            Remove-Item -LiteralPath $caseFull -Recurse -Force

            $completedRuns++
            if (($completedRuns % 20) -eq 0 -or $completedRuns -eq $totalRuns) {
                Write-Output "Measurements completed: $completedRuns / $totalRuns"
            }
        }
    }
}

$records | Export-Csv -LiteralPath $rawCsv -NoTypeInformation -Encoding utf8
$totalInputBytes = [long](($files | Measure-Object -Property Length -Sum).Sum)
$totalDuration = [double](($sourceMetadata.Values | Measure-Object -Property DurationSeconds -Sum).Sum)
$summary = foreach ($codec in $codecs) {
    $codecRows = @($records | Where-Object Codec -eq $codec)
    $passRows = foreach ($passGroup in ($codecRows | Group-Object Pass)) {
        [pscustomobject]@{
            Pass = [int]$passGroup.Name
            CompressedBytes = [long](($passGroup.Group | Measure-Object -Property CompressedBytes -Sum).Sum)
            EncodeSeconds = [double](($passGroup.Group | Measure-Object { [double]$_.EncodeSeconds } -Sum).Sum)
            DecodeSeconds = [double](($passGroup.Group | Measure-Object { [double]$_.DecodeSeconds } -Sum).Sum)
        }
    }
    $compressedBytes = [long](Get-Median -Values @($passRows | ForEach-Object { [double]$_.CompressedBytes }))
    $encodeSeconds = Get-Median -Values @($passRows | ForEach-Object { [double]$_.EncodeSeconds })
    $decodeSeconds = Get-Median -Values @($passRows | ForEach-Object { [double]$_.DecodeSeconds })
    $encodePeak = [double](($codecRows | ForEach-Object { [double]$_.EncodePeakWorkingSetMiB } | Measure-Object -Maximum).Maximum)
    $decodePeak = [double](($codecRows | ForEach-Object { [double]$_.DecodePeakWorkingSetMiB } | Measure-Object -Maximum).Maximum)
    [pscustomobject]@{
        Codec = $codec
        Configuration = $settings[$codec]
        OriginalBytes = $totalInputBytes
        CompressedBytes = $compressedBytes
        OutputPercentOfOriginal = 100 * $compressedBytes / $totalInputBytes
        ReductionPercent = 100 * (1 - $compressedBytes / $totalInputBytes)
        AudioDurationSeconds = $totalDuration
        EncodeSecondsMedianPass = $encodeSeconds
        DecodeSecondsMedianPass = $decodeSeconds
        EncodeMBPerSecond = ($totalInputBytes / 1e6) / $encodeSeconds
        DecodeMBPerSecond = ($totalInputBytes / 1e6) / $decodeSeconds
        EncodeRealtimeFactor = $totalDuration / $encodeSeconds
        DecodeRealtimeFactor = $totalDuration / $decodeSeconds
        EncodePeakWorkingSetMiB = $encodePeak
        DecodePeakWorkingSetMiB = $decodePeak
        AllPCMIdentical = -not [bool]($codecRows | Where-Object { $_.PCMIdentical -ne 'True' })
        AllWavFilesByteIdentical = -not [bool]($codecRows | Where-Object { $_.FullWavByteIdentical -ne 'True' })
    }
}
$summary | Export-Csv -LiteralPath $summaryCsv -NoTypeInformation -Encoding utf8

Write-Output "Detailed CSV: $rawCsv"
Write-Output "Summary: $summaryCsv"
$summary | Format-Table Codec,CompressedBytes,OutputPercentOfOriginal,EncodeSecondsMedianPass,DecodeSecondsMedianPass,EncodePeakWorkingSetMiB,DecodePeakWorkingSetMiB -AutoSize
