param([Parameter(Mandatory=$true)][string]$Probes)
$ErrorActionPreference = 'Stop'
$Probes = (Resolve-Path -LiteralPath $Probes).Path
foreach ($name in @('native-seek-data','profile-security','network-security','artwork-security')) {
    $log = Join-Path $Probes ($name + '.txt')
    $process = Start-Process -FilePath (Join-Path $Probes ($name + '.exe')) -WindowStyle Hidden -PassThru -RedirectStandardOutput $log -RedirectStandardError ($log + '.errors')
    if (-not $process.WaitForExit(60000)) { $process.Kill(); throw "$name timed out" }
    Get-Content -LiteralPath $log
    if ($process.ExitCode -ne 0) { Get-Content -LiteralPath ($log + '.errors'); throw "$name failed" }
}
