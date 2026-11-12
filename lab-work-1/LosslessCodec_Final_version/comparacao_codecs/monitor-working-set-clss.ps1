param(
    [int]$TargetRuns = 40,
    [int]$PollMilliseconds = 10
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $projectRoot 'build'
$outputCsv = Join-Path $PSScriptRoot 'working-set-clss-manual.csv'
$processNames = @('wav_lossless_enc', 'wav_lossless_dec', 'MyLosslessCodec')

if (Test-Path -LiteralPath $outputCsv) {
    throw "O ficheiro já existe e não será substituído: $outputCsv"
}
if ($TargetRuns -lt 1) {
    throw 'TargetRuns tem de ser pelo menos 1.'
}

$records = [System.Collections.Generic.List[object]]::new()
$seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
$runNumber = 0
$watchStarted = Get-Date

function Get-RunDetails {
    param(
        [System.Diagnostics.Process]$Process,
        [string]$ProcessName
    )

    $commandLine = ''
    try {
        $processInfo = Get-CimInstance -ClassName Win32_Process -Filter "ProcessId = $($Process.Id)"
        if ($null -ne $processInfo) {
            $commandLine = [string]$processInfo.CommandLine
        }
    }
    catch {
        # O registo do working set continua a ser possível mesmo sem ler a linha de comandos.
    }

    $operation = switch -Exact ($ProcessName) {
        'wav_lossless_enc' { 'Compressão' }
        'wav_lossless_dec' { 'Descompressão' }
        default {
            if ($commandLine -match '(?i)(^|\s)-d(\s|$)') { 'Descompressão' }
            elseif ($commandLine -match '(?i)(^|\s)-c(\s|$)') { 'Compressão' }
            else { 'Não identificada' }
        }
    }

    $wantedExtension = if ($operation -eq 'Compressão') { 'wav' } else { 'clss' }
    $fileName = ''
    $pathMatches = [regex]::Matches(
        $commandLine,
        '"(?<quoted>[^"]+\.(?:wav|clss))"|(?<plain>[^\s"]+\.(?:wav|clss))',
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase
    )
    foreach ($match in $pathMatches) {
        $candidate = if ($match.Groups['quoted'].Success) {
            $match.Groups['quoted'].Value
        }
        else {
            $match.Groups['plain'].Value
        }
        if ([System.IO.Path]::GetExtension($candidate).TrimStart('.') -ieq $wantedExtension) {
            $fileName = [System.IO.Path]::GetFileName($candidate)
            break
        }
    }

    if ([string]::IsNullOrWhiteSpace($fileName)) {
        $fileName = 'não identificado pela linha de comandos'
    }

    [pscustomobject]@{
        Operation = $operation
        FileName = $fileName
        CommandLine = $commandLine
    }
}

function Write-RunCsvRow {
    param([object]$Row)

    if (Test-Path -LiteralPath $outputCsv) {
        $Row | Export-Csv -LiteralPath $outputCsv -NoTypeInformation -Append -Encoding UTF8
    }
    else {
        $Row | Export-Csv -LiteralPath $outputCsv -NoTypeInformation -Encoding UTF8
    }
}

function Write-SummaryRows {
    if ($records.Count -eq 0) {
        return
    }

    foreach ($operation in @('Compressão', 'Descompressão')) {
        $group = @($records | Where-Object Operacao -eq $operation)
        if ($group.Count -eq 0) {
            continue
        }

        $sortedPeaks = @($group | ForEach-Object { [double]$_.PicoWorkingSetMiB } | Sort-Object)
        $middle = [int][Math]::Floor($sortedPeaks.Count / 2)
        $median = if ($sortedPeaks.Count % 2) {
            $sortedPeaks[$middle]
        }
        else {
            ($sortedPeaks[$middle - 1] + $sortedPeaks[$middle]) / 2
        }
        $maximum = $group | Sort-Object { [double]$_.PicoWorkingSetMiB } -Descending | Select-Object -First 1
        $average = ($sortedPeaks | Measure-Object -Average).Average

        Write-RunCsvRow ([pscustomobject]@{
            TipoRegisto = 'Resumo'
            Ficheiro = $maximum.Ficheiro
            Operacao = $operation
            Executavel = ''
            PID = ''
            PicoWorkingSetBytes = ''
            PicoWorkingSetMiB = ''
            NumeroExecucoes = $group.Count
            PicoMaximoMiB = [Math]::Round([double]$maximum.PicoWorkingSetMiB, 3)
            FicheiroDoPicoMaximo = $maximum.Ficheiro
            MediaMiB = [Math]::Round([double]$average, 3)
            MedianaMiB = [Math]::Round([double]$median, 3)
            InicioDetecao = ''
            FimDetecao = ''
            LinhaComandos = ''
        })
    }
}

Write-Host "Monitor do working set do CLSS preparado."
Write-Host "A observar apenas: $($processNames -join ', ') em $buildRoot"
Write-Host "Serão recolhidas $TargetRuns execuções, uma de cada vez."
Write-Host 'Inicia agora cada compressão/descodificação num processo separado.'
Write-Host 'O nome do ficheiro será lido da linha de comandos, quando estiver disponível.'
Write-Host 'Para terminar antes de completar a recolha, usa Ctrl+C.'

try {
    while ($runNumber -lt $TargetRuns) {
        $candidate = $null
        $processName = ''

        while ($null -eq $candidate) {
            foreach ($name in $processNames) {
                foreach ($process in [System.Diagnostics.Process]::GetProcessesByName($name)) {
                    try {
                        $process.Refresh()
                        $startTime = $process.StartTime
                        $key = "$($process.Id)|$($startTime.ToUniversalTime().Ticks)"
                        if ($startTime -lt $watchStarted -or $seen.Contains($key)) {
                            $process.Dispose()
                            continue
                        }

                        $exePath = $process.MainModule.FileName
                        $expectedPath = Join-Path $buildRoot "$name.exe"
                        if (-not [string]::Equals(
                            [System.IO.Path]::GetFullPath($exePath),
                            [System.IO.Path]::GetFullPath($expectedPath),
                            [System.StringComparison]::OrdinalIgnoreCase
                        )) {
                            $process.Dispose()
                            continue
                        }

                        $candidate = $process
                        $processName = $name
                        [void]$seen.Add($key)
                        break
                    }
                    catch {
                        $process.Dispose()
                    }
                }
                if ($null -ne $candidate) {
                    break
                }
            }

            if ($null -eq $candidate) {
                Start-Sleep -Milliseconds $PollMilliseconds
            }
        }

        $runNumber++
        $details = Get-RunDetails -Process $candidate -ProcessName $processName
        $processId = $candidate.Id
        $observedStart = Get-Date
        $peakBytes = 0L
        Write-Host "A observar execução $runNumber/$TargetRuns : $($details.Operation), $($details.FileName), PID $processId"

        while ($true) {
            try {
                $candidate.Refresh()
                $reportedPeak = [long]$candidate.PeakWorkingSet64
                $currentWorkingSet = [long]$candidate.WorkingSet64
                if ($reportedPeak -gt $peakBytes) {
                    $peakBytes = $reportedPeak
                }
                if ($currentWorkingSet -gt $peakBytes) {
                    $peakBytes = $currentWorkingSet
                }
            }
            catch {
                # O processo pode terminar entre a atualização e a leitura.
            }

            if ($candidate.WaitForExit($PollMilliseconds)) {
                break
            }
        }

        $observedEnd = Get-Date
        $candidate.Dispose()
        $peakMiB = [Math]::Round($peakBytes / 1MB, 3)

        $row = [pscustomobject]@{
            TipoRegisto = 'Individual'
            Ficheiro = $details.FileName
            Operacao = $details.Operation
            Executavel = "$processName.exe"
            PID = $processId
            PicoWorkingSetBytes = $peakBytes
            PicoWorkingSetMiB = $peakMiB
            NumeroExecucoes = ''
            PicoMaximoMiB = ''
            FicheiroDoPicoMaximo = ''
            MediaMiB = ''
            MedianaMiB = ''
            InicioDetecao = $observedStart.ToString('o')
            FimDetecao = $observedEnd.ToString('o')
            LinhaComandos = $details.CommandLine
        }
        $records.Add($row)
        Write-RunCsvRow $row

        Write-Host "Registado: $peakMiB MiB."
    }
}
finally {
    Write-SummaryRows
    if ($records.Count -gt 0) {
        Write-Host "CSV atualizado: $outputCsv"
    }
}

Write-Host 'Recolha concluída.'
