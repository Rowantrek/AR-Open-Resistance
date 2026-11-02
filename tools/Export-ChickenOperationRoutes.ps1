[CmdletBinding()]
param([string]$OutputDirectory = 'docs/validation')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'CampaignGroundingData.ps1')
$repoRoot = Split-Path -Parent $PSScriptRoot
$layerRoot = Join-Path $repoRoot 'AROpenResistance/Open Resistance/Worlds/AROR_Everon/AROR_Everon_Chicken_Layers'
$outputRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
if (!$outputRoot.StartsWith($repoRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
 throw 'The output directory must be inside the repository'
}
$catalogue = @{}
foreach ($row in (Import-Csv (Join-Path $repoRoot 'docs/EVERON_CAMPAIGN_LOCATIONS.csv'))) { $catalogue[$row.Id] = $row }
$sites = @{}
$layerNames = @('CivilianLocations.layer', 'MilitaryInstallations.layer')
foreach ($layerName in $layerNames) {
 $roots = Read-CampaignLayerLocations ([IO.File]::ReadAllText((Join-Path $layerRoot $layerName))) -RootsOnly
 foreach ($entry in $roots.GetEnumerator()) {
  $id = $entry.Key; $root = $entry.Value
  if ($sites.ContainsKey($id)) { throw "Duplicate Chicken location ID: $id" }
  $type = [regex]::Match($root.Text, '(?m)^\s+m_eType\s+(\w+)').Groups[1].Value
  $role = [regex]::Match($root.Text, '(?m)^\s+m_eInitialControllingRole\s+(\w+)').Groups[1].Value
  $name = [regex]::Match($root.Text, '(?m)^\s+m_sName\s+"([^"]+)"').Groups[1].Value
  if (!$type -and $catalogue.ContainsKey($id)) { $type = $catalogue[$id].Type }
  if (!$role -and $catalogue.ContainsKey($id)) { $role = $catalogue[$id].Role }
  if (!$name -and $catalogue.ContainsKey($id)) { $name = $catalogue[$id].Name }
  if (!$type -or !$role -or !$name) { throw "Unresolved location metadata: $id" }
  $sites[$id] = [pscustomobject]@{ Id=$id; Name=$name; Type=$type; Role=$role; X=$root.Origin[0]; Y=$root.Origin[1]; Z=$root.Origin[2] }
 }
}
$allSites = @($sites.Values | Sort-Object Id)
$origins = @($allSites | Where-Object Type -In @('OUTPOST', 'MILITARY_BASE', 'AIRPORT'))
$militaryTypes = @('OUTPOST', 'MILITARY_BASE', 'AIRPORT', 'FACTORY', 'RESOURCES', 'PORT')
$executorPath = Join-Path $repoRoot 'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationExecutorComponent.c'
$executorSource = [IO.File]::ReadAllText($executorPath)
$preferredMatch = [regex]::Match($executorSource, '\[Attribute\("([\d.]+)",[^\r\n]*\)\]\s+protected float m_fMinOriginDistance;')
if (!$preferredMatch.Success) { throw 'Cannot resolve the source preferred origin distance' }
$preferredDistance = [double]$preferredMatch.Groups[1].Value
$managerLayer = [IO.File]::ReadAllText((Join-Path $layerRoot 'Managers.layer'))
$preferredOverride = [regex]::Match($managerLayer, '(?m)^\s+m_fMinOriginDistance\s+([\d.]+)\s*$')
if ($preferredOverride.Success) { $preferredDistance=[double]$preferredOverride.Groups[1].Value }
if ($allSites.Count -ne 45 -or $origins.Count -ne 21) { throw "Unexpected Chicken topology: $($allSites.Count) locations, $($origins.Count) launch sites" }
$rows = foreach ($origin in $origins) { foreach ($target in $allSites) {
 if ($origin.Id -eq $target.Id) { continue }
 $distance = [Math]::Sqrt([Math]::Pow($target.X-$origin.X,2) + [Math]::Pow($target.Z-$origin.Z,2))
 $risks = @()
 if ($distance -lt $preferredDistance) { $risks += 'Inside preferred origin distance; fallback candidate' }
 if ($target.Type -eq 'PORT') { $risks += 'Coastal target; water, pad and dismount acceptance required' }
 if ($origin.Y -le 10 -or $target.Y -le 10) { $risks += 'Low authored elevation; confirm dry launch/approach' }
 if ([Math]::Abs($target.Y-$origin.Y) -ge 100) { $risks += 'Large endpoint elevation difference; intermediate terrain unknown' }
 if ($target.Id -eq 'everon_lamentin_port') { $risks += 'Known native launch/boarding/LZ regression target' }
 [pscustomobject]@{
  OriginId=$origin.Id; OriginName=$origin.Name; OriginType=$origin.Type; InitialOriginRole=$origin.Role
  TargetId=$target.Id; TargetName=$target.Name; TargetType=$target.Type; InitialTargetRole=$target.Role
  OriginX=$origin.X; OriginY=$origin.Y; OriginZ=$origin.Z; TargetX=$target.X; TargetY=$target.Y; TargetZ=$target.Z
  StraightLineMetres=[Math]::Round($distance,1); EndpointHeightDifference=[Math]::Round($target.Y-$origin.Y,1)
  CaptureQRF=$true; AutomaticAttack=($target.Type -in $militaryTypes)
  ResponseDomains='GROUND;AIRBORNE;AIR_GROUND'; OwnershipRequirement='Origin owned by responding army; capture/attack target validated at runtime'
  RiskNotes=($risks -join '; '); NativeGroundPlanning='NOT_RUN'; NativeAirLaunchPlanning='NOT_RUN'; NativeLightAirLaunchPlanning='NOT_RUN'; NativeLandingPads='NOT_RUN'
  NativeRoadConnectivity='NOT_RUN'; NativeDriving='NOT_RUN'; NativeLanding='NOT_RUN'; DedicatedServer='NOT_RUN'
 }
}}
if ($rows.Count -ne 924) { throw 'Missing or duplicated directed origin/target pairs' }
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$allSites | Export-Csv -LiteralPath (Join-Path $outputRoot 'CHICKEN_OPERATION_LOCATIONS.csv') -NoTypeInformation -Encoding UTF8
$rows | Export-Csv -LiteralPath (Join-Path $outputRoot 'CHICKEN_OPERATION_ROUTES.csv') -NoTypeInformation -Encoding UTF8
$hashes = foreach ($layerName in $layerNames) { [pscustomobject]@{ File=$layerName; SHA256=(Get-FileHash -LiteralPath (Join-Path $layerRoot $layerName) -Algorithm SHA256).Hash } }
$summary = [ordered]@{ Locations=$allSites.Count; LaunchSites=$origins.Count; DirectedPairs=$rows.Count; PreferredOriginMetres=$preferredDistance
 AttackPairs=@($rows | Where-Object AutomaticAttack).Count; PortPairs=@($rows | Where-Object TargetType -EQ 'PORT').Count
 Evidence='Authored world metadata and endpoint geometry only. No road topology, intermediate slopes, bridges, driving or flight was evaluated.'
 Layers=$hashes; ExecutorSHA256=(Get-FileHash -LiteralPath $executorPath -Algorithm SHA256).Hash }
$summary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outputRoot 'CHICKEN_OPERATION_ROUTES.json') -Encoding UTF8
$summary | ConvertTo-Json -Depth 4
