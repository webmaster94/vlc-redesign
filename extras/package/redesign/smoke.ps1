# Reframe integration checks in an isolated Windows CI environment.
# Copyright 2026 webmaster94. GPL-2.0-or-later.
param([Parameter(Mandatory=$true)][string]$Artifacts, [string]$PrintReceiver)
$ErrorActionPreference = 'Stop'
$Artifacts = (Resolve-Path -LiteralPath $Artifacts).Path
$testRoot = Join-Path $env:RUNNER_TEMP 'reframe-integration'
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null
$portableArchive = Join-Path $Artifacts 'Reframe-3.0.24-r5-win64-portable.zip'
Expand-Archive -LiteralPath $portableArchive -DestinationPath (Join-Path $testRoot 'portable')
$app = Join-Path $testRoot 'portable\Reframe\reframe.exe'
# Exercise normal startup with a fresh portable profile, including the real consent dialog.
foreach ($launch in 1..2) {
    $firstRunReport = Join-Path $testRoot "first-run-$launch.txt"
    $firstRunArgs = @('--no-one-instance', "--qt-redesign-first-run-smoke=$firstRunReport")
    $firstRun = Start-Process -FilePath $app -ArgumentList $firstRunArgs -PassThru -WindowStyle Hidden
    if (-not $firstRun.WaitForExit(15000)) { $firstRun.Kill(); throw 'First-run checks timed out' }
    if (-not (Test-Path -LiteralPath $firstRunReport)) { throw 'First-run report was not written' }
    $result = Get-Content -LiteralPath $firstRunReport -Raw
    Copy-Item -LiteralPath $firstRunReport -Destination $Artifacts
    Get-ChildItem -LiteralPath $testRoot -Filter 'first-run-*.png' | Copy-Item -Destination $Artifacts
    if (-not $result.StartsWith('PASS:')) { throw $result }
    if ($launch -eq 2 -and -not $result.Contains('relaunch opened onboarding without another prompt')) {
        throw 'Saved privacy choice was not respected on the second launch'
    }
    Write-Output $result
}
$preview = Join-Path $testRoot 'preview.png'
$render = Start-Process -FilePath $app -ArgumentList @('--no-one-instance','--no-qt-privacy-ask',"--qt-redesign-preview=$preview") -PassThru -WindowStyle Hidden
if (-not $render.WaitForExit(30000)) { $render.Kill(); throw 'Player rendering timed out' }
if ($render.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $preview)) { throw 'Player rendering failed' }
if ((Get-Item -LiteralPath $preview).Length -lt 1000) { throw 'Rendered preview is empty' }

# A generated WAV exercises real demux and audio decoding without sample copyright.
$wave = Join-Path $testRoot 'sample with spaces ñ測.wav'
$samples = [byte[]]::new(240000)
for ($sampleIndex = 0; $sampleIndex -lt 120000; $sampleIndex++) {
    $sample = [int16]([Math]::Sin($sampleIndex * 2 * [Math]::PI * 440 / 8000) * 12000)
    [BitConverter]::GetBytes($sample).CopyTo($samples, $sampleIndex * 2)
}
$stream = [IO.File]::Create($wave)
$writer = [IO.BinaryWriter]::new($stream)
$writer.Write([Text.Encoding]::ASCII.GetBytes('RIFF'))
$writer.Write([int](36 + $samples.Length))
$writer.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt '))
$writer.Write([int]16); $writer.Write([int16]1); $writer.Write([int16]1)
$writer.Write([int]8000); $writer.Write([int]16000); $writer.Write([int16]2); $writer.Write([int16]16)
$writer.Write([Text.Encoding]::ASCII.GetBytes('data')); $writer.Write([int]$samples.Length); $writer.Write($samples)
$writer.Dispose()
$decoded = Join-Path $testRoot 'decoded.wav'
$sout = "#transcode{acodec=s16l,channels=1,samplerate=8000}:std{access=file,mux=wav,dst='$decoded'}"
$decode = Start-Process -FilePath $app -ArgumentList @('--intf=dummy','--ignore-config','--no-one-instance','--play-and-exit',"--sout=$sout", ('"' + $wave + '"')) -PassThru -WindowStyle Hidden
if (-not $decode.WaitForExit(30000)) { $decode.Kill(); throw 'Media decoding timed out' }
if ($decode.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $decoded) -or (Get-Item -LiteralPath $decoded).Length -lt 1000) { throw 'WAV demux/decode/output failed' }

# Additional startup options are parsed by the real launcher and bypassed by --ignore-config.
$extraPreview = Join-Path $testRoot 'extra-options-preview.png'
$extraFile = Join-Path $testRoot 'portable\Reframe\portable\extra-options.txt'
[IO.File]::WriteAllText($extraFile, ('--no-one-instance --no-qt-privacy-ask --qt-redesign-preview="' + $extraPreview + '"'), [Text.UTF8Encoding]::new($false))
$extraLaunch = Start-Process -FilePath $app -PassThru -WindowStyle Hidden
if (-not $extraLaunch.WaitForExit(30000)) { $extraLaunch.Kill(); throw 'Additional options timed out' }
if (-not (Test-Path -LiteralPath $extraPreview)) { throw 'Additional command-line options were not applied' }
Remove-Item -LiteralPath $extraFile

# The separate Waveform visualizer must produce frames from real decoded audio.
$visualRoot = Join-Path $testRoot 'visualization'
New-Item -ItemType Directory -Path $visualRoot | Out-Null
$visual = Start-Process -FilePath $app -ArgumentList @('--intf=dummy','--ignore-config','--no-one-instance','--aout=dummy','--audio-visual=visual','--effect-list=waveform','--vout=dummy','--video-filter=scene','--scene-format=png','--scene-ratio=1','--scene-replace',"--scene-path=$visualRoot",'--scene-prefix=waveform','--stop-time=2','--play-and-exit',('"' + $wave + '"')) -PassThru -WindowStyle Hidden
if (-not $visual.WaitForExit(30000)) { $visual.Kill(); throw 'Waveform visualizer timed out' }
if (-not (Test-Path -LiteralPath (Join-Path $visualRoot 'waveform.png'))) { throw 'Waveform visualizer did not render decoded audio' }
Copy-Item -LiteralPath (Join-Path $visualRoot 'waveform.png') -Destination $Artifacts

# Generated Ogg files exercise the library selection and saved-playlist path from the UI.
$musicRoot = Join-Path $testRoot 'generated-music'
New-Item -ItemType Directory -Path $musicRoot | Out-Null
$ogg = Join-Path $musicRoot 'Tone 1 ñ測.ogg'
$oggOutput = "#transcode{acodec=vorb,channels=1,samplerate=44100,ab=96}:std{access=file,mux=ogg,dst='$ogg'}"
$encode = Start-Process -FilePath $app -ArgumentList @('--intf=dummy','--ignore-config','--no-one-instance','--play-and-exit',('--sout="' + $oggOutput + '"'),('"' + $wave + '"')) -PassThru -WindowStyle Hidden
if (-not $encode.WaitForExit(30000)) { $encode.Kill(); throw 'Ogg fixture generation timed out' }
if (-not (Test-Path -LiteralPath $ogg) -or (Get-Item -LiteralPath $ogg).Length -lt 1000) { throw 'Ogg fixture generation failed' }
Copy-Item -LiteralPath $ogg -Destination (Join-Path $musicRoot 'Tone 2 ñ測.ogg')
Copy-Item -LiteralPath $ogg -Destination (Join-Path $musicRoot 'Tone 3 ñ測.ogg')

# A short synthetic YUV video exercises real video output and its fullscreen state.
$video = Join-Path $musicRoot 'generated-video.y4m'
$videoStream = [IO.File]::Create($video)
$videoWriter = [IO.BinaryWriter]::new($videoStream)
$videoWriter.Write([Text.Encoding]::ASCII.GetBytes("YUV4MPEG2 W160 H90 F10:1 Ip A1:1 C420jpeg`n"))
$frame = [byte[]]::new(21600)
for ($pixel = 0; $pixel -lt 14400; $pixel++) { $frame[$pixel] = [byte](50 + ($pixel % 160)) }
for ($pixel = 14400; $pixel -lt 18000; $pixel++) { $frame[$pixel] = 100 }
for ($pixel = 18000; $pixel -lt 21600; $pixel++) { $frame[$pixel] = 160 }
for ($frameIndex = 0; $frameIndex -lt 150; $frameIndex++) {
    $videoWriter.Write([Text.Encoding]::ASCII.GetBytes("FRAME`n"))
    $videoWriter.Write($frame)
}
$videoWriter.Dispose()

$controlReport = Join-Path $testRoot 'controls.txt'
$printProbe = $null
if ($PrintReceiver) {
    $PrintReceiver = (Resolve-Path -LiteralPath $PrintReceiver).Path
    $printLog = Join-Path $testRoot 'printscreen.txt'
    $printProbe = Start-Process -FilePath $PrintReceiver -WindowStyle Hidden -PassThru -RedirectStandardOutput $printLog -RedirectStandardError ($printLog + '.errors')
    $readyUntil = [DateTime]::UtcNow.AddSeconds(5)
    $receiverReady = $false
    while ([DateTime]::UtcNow -lt $readyUntil) {
        if (Test-Path -LiteralPath $printLog) {
            $receiverText = Get-Content -LiteralPath $printLog -Raw
            if (-not [string]::IsNullOrEmpty($receiverText) -and $receiverText -match 'READY:') { $receiverReady = $true; break }
        }
        if ($printProbe.HasExited) { break }
        Start-Sleep -Milliseconds 50
    }
    if (-not $receiverReady) { if (-not $printProbe.HasExited) { $printProbe.Kill() }; throw 'External PrintScreen receiver did not register its hotkey' }
    $env:REFRAME_PRINT_PROBE = '1'
}
$controls = Start-Process -FilePath $app -ArgumentList @('--no-one-instance','--ignore-config','--no-qt-privacy-ask','--aout=dummy',"--qt-redesign-smoke=$controlReport", "--qt-redesign-smoke-library=$musicRoot",('"' + $wave + '"')) -PassThru -WindowStyle Hidden
if (-not $controls.WaitForExit(120000)) { $controls.Kill(); if ($printProbe -and -not $printProbe.HasExited) { $printProbe.Kill() }; throw 'Widget and playback integration checks timed out' }
Remove-Item Env:REFRAME_PRINT_PROBE -ErrorAction SilentlyContinue
if (-not (Test-Path -LiteralPath $controlReport)) { throw 'Widget and playback integration report was not written' }
$controlResult = Get-Content -LiteralPath $controlReport -Raw
Copy-Item -LiteralPath $controlReport -Destination $Artifacts
Get-ChildItem -LiteralPath $testRoot -Filter 'controls.txt.*.png' | Copy-Item -Destination $Artifacts
if (-not $controlResult.StartsWith('PASS:')) { if ($printProbe -and -not $printProbe.HasExited) { $printProbe.Kill() }; throw $controlResult }
if ($printProbe) {
    if (-not $printProbe.WaitForExit(5000)) { $printProbe.Kill(); throw 'PrintScreen was not received from both fullscreen contexts' }
    Copy-Item -LiteralPath $printLog -Destination $Artifacts
    Get-Content -LiteralPath $printLog
    if ($printProbe.ExitCode -ne 0) { throw 'External PrintScreen test failed' }
}
Write-Output $controlResult
Copy-Item -LiteralPath ($controlReport + '.player.png') -Destination (Join-Path $Artifacts 'controls-player.png')
Copy-Item -LiteralPath ($controlReport + '.settings.png') -Destination (Join-Path $Artifacts 'controls-settings.png')

# Fake stock-VLC registrations make accidental cross-product writes observable.
$stock = Join-Path $testRoot 'stock-vlc'
New-Item -ItemType Directory -Path $stock | Out-Null
Set-Content -LiteralPath (Join-Path $stock 'preserve.txt') -Value 'stock VLC sentinel'
New-Item -Path 'HKLM:\Software\VideoLAN\VLC' -Force | Out-Null
New-ItemProperty -Path 'HKLM:\Software\VideoLAN\VLC' -Name InstallDir -Value $stock -PropertyType String -Force | Out-Null
$classes = 'Registry::HKEY_CLASSES_ROOT'
New-Item -Path "$classes\VLC.mp4" -Force | Out-Null
Set-Item -Path "$classes\VLC.mp4" -Value 'stock association sentinel'
New-Item -Path "$classes\.mp4" -Force | Out-Null
Set-Item -Path "$classes\.mp4" -Value 'VLC.mp4'

$destination = Join-Path $testRoot 'installed'
$setup = Join-Path $Artifacts 'Reframe-3.0.24-r5-win64.exe'
foreach ($pass in 1..2) {
    $install = Start-Process -FilePath $setup -ArgumentList @('/S',"/D=$destination") -Wait -PassThru -WindowStyle Hidden
    if ($install.ExitCode -ne 0) { throw "Installer failed on pass $pass" }
    if (-not (Test-Path -LiteralPath (Join-Path $destination 'reframe.exe'))) { throw 'Installer did not install the player' }
    if ((Get-ItemProperty 'HKLM:\Software\VideoLAN\VLC').InstallDir -ne $stock) { throw 'Stock VLC registration changed' }
    if ((Get-Item "$classes\.mp4").GetValue('') -ne 'VLC.mp4') { throw 'Existing association changed' }
    Set-Content -LiteralPath (Join-Path $destination 'user-file.txt') -Value 'user data must survive'
}
$uninstall = Start-Process -FilePath (Join-Path $destination 'uninstall.exe') -ArgumentList '/S' -Wait -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 2
if (Test-Path -LiteralPath (Join-Path $destination 'reframe.exe')) { throw 'Uninstaller left the managed executable' }
if (-not (Test-Path -LiteralPath (Join-Path $destination 'user-file.txt'))) { throw 'Uninstaller deleted a user file' }
if (-not (Test-Path -LiteralPath (Join-Path $stock 'preserve.txt'))) { throw 'Stock VLC files changed' }
if ((Get-ItemProperty 'HKLM:\Software\VideoLAN\VLC').InstallDir -ne $stock) { throw 'Uninstaller changed stock VLC registration' }
if ((Get-Item "$classes\.mp4").GetValue('') -ne 'VLC.mp4') { throw 'Uninstaller changed stock associations' }
if (Test-Path 'HKLM:\Software\Reframe') { throw 'Uninstaller left its registration' }
Copy-Item -LiteralPath $preview -Destination (Join-Path $Artifacts 'preview.png')
Write-Output 'PASS: rendering, media decode, installation, reinstallation, uninstallation, and stock VLC coexistence'

Get-ChildItem -LiteralPath $testRoot -Filter 'controls.txt.*.png' | Copy-Item -Destination $Artifacts
