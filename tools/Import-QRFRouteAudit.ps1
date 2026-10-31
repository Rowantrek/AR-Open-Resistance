[CmdletBinding()]
param(
 [Parameter(Mandatory = $true)][string]$LogPath,
 [string]$OutputPath = 'docs/validation/CHICKEN_OPERATION_ROUTES.native.csv'
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$lines = [IO.File]::ReadAllLines((Resolve-Path -LiteralPath $LogPath))
$start = -1
for ($i=0; $i -lt $lines.Length; $i++) { if ($lines[$i] -match '\[AROR ROUTE AUDIT\] start ') { $start=$i } }
if ($start -lt 0) { throw 'No route-audit start marker; no native evidence imported' }
$pairs = @{}; $lightPairs = @{}; $pads = @{}; $complete=$false
foreach ($line in $lines[$start..($lines.Length-1)]) {
 if ($line -match '\[AROR ROUTE AUDIT\] pair origin=(\S+) target=(\S+) groundPlanning=(PASS|FAIL) airLaunchPlanning=(PASS|FAIL|NO_SAFE_LZ)') {
  $pairs[$Matches[1]+'/'+$Matches[2]] = @($Matches[3],$Matches[4])
 }
 if ($line -match '\[AROR ROUTE AUDIT\] pair origin=(\S+) target=(\S+).* lightAirLaunchPlanning=(PASS|FAIL|NO_SAFE_LZ)') { $lightPairs[$Matches[1]+'/'+$Matches[2]]=$Matches[3] }
 if ($line -match '\[AROR ROUTE AUDIT\] landing target=(\S+) checked=\d+/\d+ pads=(\d+)') { $pads[$Matches[1]]=$Matches[2] }
 if ($line -match '\[AROR ROUTE AUDIT\] landing target=(\S+) unavailable;') { $pads[$Matches[1]]='UNAVAILABLE' }
 if ($line -match '\[AROR ROUTE AUDIT\] complete ') { $complete=$true }
}
$rows = @(Import-Csv -LiteralPath (Join-Path $repoRoot 'docs/validation/CHICKEN_OPERATION_ROUTES.csv'))
$header=[regex]::Match($lines[$start],'start origins=(\d+) targets=(\d+)')
if(!$header.Success -or [int]$header.Groups[1].Value -ne @($rows.OriginId | Select-Object -Unique).Count -or [int]$header.Groups[2].Value -ne @($rows.TargetId | Select-Object -Unique).Count){throw 'Audit header does not match the current Chicken matrix'}
$knownPairs = @{}; $knownTargets = @{}
foreach ($row in $rows) {
 $key=$row.OriginId+'/'+$row.TargetId
 $knownPairs[$key]=$true; $knownTargets[$row.TargetId]=$true
 if ($pairs.ContainsKey($key)) { $row.NativeGroundPlanning=$pairs[$key][0]; $row.NativeAirLaunchPlanning=$pairs[$key][1] }
 if ($lightPairs.ContainsKey($key)) { $row.NativeLightAirLaunchPlanning=$lightPairs[$key] }
 if ($pads.ContainsKey($row.TargetId)) { $row.NativeLandingPads=$pads[$row.TargetId] }
}
foreach ($key in $pairs.Keys) { if (!$knownPairs.ContainsKey($key)) { throw "Log contains a pair outside the current Chicken matrix: $key" } }
foreach ($key in $pads.Keys) { if (!$knownTargets.ContainsKey($key)) { throw "Log contains a target outside the current Chicken matrix: $key" } }
$targetPath = $OutputPath
if (![IO.Path]::IsPathRooted($targetPath)) { $targetPath=Join-Path $repoRoot $targetPath }
$targetPath = [IO.Path]::GetFullPath($targetPath)
$sourcePath = [IO.Path]::GetFullPath((Join-Path $repoRoot 'docs/validation/CHICKEN_OPERATION_ROUTES.csv'))
if ($targetPath -eq $sourcePath) { throw 'Native evidence must be written separately from the generated baseline' }
$rows | Export-Csv -LiteralPath $targetPath -NoTypeInformation -Encoding UTF8
$fullyObserved=$complete -and $pairs.Count -eq $rows.Count -and $pads.Count -eq $knownTargets.Count
Write-Output "Imported latest audit: $($pairs.Count)/$($rows.Count) pairs, $($pads.Count) landing targets, completion marker=$complete, complete evidence=$fullyObserved. Driving, actual landing, full road connectivity and dedicated-server evidence remain NOT_RUN."
