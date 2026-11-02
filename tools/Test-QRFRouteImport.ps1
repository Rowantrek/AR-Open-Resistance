# Synthetic log fixtures only. Does not supply any native route evidence.
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
$baseline=Join-Path $repoRoot 'docs/validation/CHICKEN_OPERATION_ROUTES.csv'
$before=(Get-FileHash -LiteralPath $baseline).Hash
$rows=@(Import-Csv -LiteralPath $baseline)
$first=$rows[0]; $second=$rows[1]
$token=[Guid]::NewGuid().ToString('N')
$fixture=Join-Path $env:TEMP ('aror-route-'+$token+'.log')
$output=Join-Path $env:TEMP ('aror-route-'+$token+'.csv')
try {
 $lines=@(
  '[AROR ROUTE AUDIT] start origins=21 targets=45',
  "[AROR ROUTE AUDIT] pair origin=$($first.OriginId) target=$($first.TargetId) groundPlanning=PASS airLaunchPlanning=PASS",
  '[AROR ROUTE AUDIT] start origins=21 targets=45',
  "[AROR ROUTE AUDIT] pair origin=$($second.OriginId) target=$($second.TargetId) groundPlanning=FAIL airLaunchPlanning=FAIL lightAirLaunchPlanning=PASS",
  "[AROR ROUTE AUDIT] landing target=$($second.TargetId) checked=121/121 pads=0 spacing=0 water=121",
  '[AROR ROUTE AUDIT] complete checked=1 planningPassed=0'
 )
 [IO.File]::WriteAllLines($fixture,$lines)
 & (Join-Path $PSScriptRoot 'Import-QRFRouteAudit.ps1') -LogPath $fixture -OutputPath $output
 $imported=@(Import-Csv -LiteralPath $output)
 if ($imported.Count -ne 924) { throw 'Importer lost baseline pairs' }
 if ($imported[0].NativeGroundPlanning -ne 'NOT_RUN' -or $imported[1].NativeGroundPlanning -ne 'FAIL' -or $imported[1].NativeAirLaunchPlanning -ne 'FAIL' -or $imported[1].NativeLightAirLaunchPlanning -ne 'PASS') { throw 'Importer mixed runs or lost separate heavy/light planning outcomes' }
 if ($imported[1].NativeLandingPads -ne '0') { throw 'Failed landing search lost its zero-pad result' }
 if (@($imported | Where-Object { $_.NativeDriving -ne 'NOT_RUN' -or $_.NativeLanding -ne 'NOT_RUN' -or $_.DedicatedServer -ne 'NOT_RUN' }).Count) { throw 'Planning import fabricated native execution evidence' }
 [IO.File]::AppendAllText($fixture,"`n[AROR ROUTE AUDIT] pair origin=unknown target=unknown groundPlanning=PASS airLaunchPlanning=PASS")
 $rejected=$false
 try { & (Join-Path $PSScriptRoot 'Import-QRFRouteAudit.ps1') -LogPath $fixture -OutputPath $output } catch { $rejected=$true }
 if (!$rejected) { throw 'Unknown topology was accepted' }
 if ((Get-FileHash -LiteralPath $baseline).Hash -ne $before) { throw 'Importer modified baseline evidence' }
 Write-Output 'Passed six synthetic route-import checks. No native driving, flight or planning was executed.'
} finally {
 foreach ($temporary in @($fixture,$output)) { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force } }
}
