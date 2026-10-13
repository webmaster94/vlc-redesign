# Reframe integration checks in an isolated Windows CI environment.
# Copyright 2026 webmaster94. GPL-2.0-or-later.
param([Parameter(Mandatory=$true)][string]$Artifacts)
$ErrorActionPreference = 'Stop'
$Artifacts = (Resolve-Path -LiteralPath $Artifacts).Path
$testRoot = Join-Path $env:RUNNER_TEMP 'reframe-integration'
New-Item -ItemType Directory -Force -Path $testRoot | Out-Null
$portableArchive = Join-Path $Artifacts 'Reframe-3.0.24-r2-win64-portable.zip'
Expand-Archive -LiteralPath $portableArchive -DestinationPath (Join-Path $testRoot 'portable')
$app = Join-Path $testRoot 'portable\Reframe\reframe.exe'
$preview = Join-Path $testRoot 'preview.png'
$render = Start-Process -FilePath $app -ArgumentList @('--no-one-instance','--no-qt-privacy-ask',"--qt-redesign-preview=$preview") -PassThru -WindowStyle Hidden
if (-not $render.WaitForExit(30000)) { $render.Kill(); throw 'Player rendering timed out' }
if ($render.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $preview)) { throw 'Player rendering failed' }
if ((Get-Item -LiteralPath $preview).Length -lt 1000) { throw 'Rendered preview is empty' }

# A generated WAV exercises real demux and audio decoding without sample copyright.
$wave = Join-Path $testRoot 'sample with spaces.wav'
$samples = [byte[]]::new(240000)
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

$controlReport = Join-Path $testRoot 'controls.txt'
$controls = Start-Process -FilePath $app -ArgumentList @('--no-one-instance','--ignore-config','--no-qt-privacy-ask','--aout=dummy',"--qt-redesign-smoke=$controlReport",('"' + $wave + '"')) -PassThru -WindowStyle Hidden
if (-not $controls.WaitForExit(60000)) { $controls.Kill(); throw 'Widget and playback integration checks timed out' }
if (-not (Test-Path -LiteralPath $controlReport)) { throw 'Widget and playback integration report was not written' }
$controlResult = Get-Content -LiteralPath $controlReport -Raw
if (-not $controlResult.StartsWith('PASS:')) { throw $controlResult }
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
$setup = Join-Path $Artifacts 'Reframe-3.0.24-r2-win64.exe'
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

Get-ChildItem -LiteralPath $testRoot -Filter 'controls.txt.page*.png' | Copy-Item -Destination $Artifacts
