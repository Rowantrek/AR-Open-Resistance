[CmdletBinding()]
param([switch]$WriteReport,[string]$ReportPath='docs/CAMPAIGN_SITE_READINESS.csv')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'CampaignGroundingData.ps1')
$repoRoot=Split-Path -Parent $PSScriptRoot
$addonRoot=Join-Path $repoRoot 'AROpenResistance/Open Resistance'
$catalogue=@(Import-Csv (Join-Path $repoRoot 'docs/EVERON_CAMPAIGN_LOCATIONS.csv'))
$sites=@($catalogue | Where-Object Type -NotIn @('TOWN','CITY'))
if($catalogue.Count -ne 41 -or $sites.Count -ne 23){throw 'Expected 41 catalogue locations including 23 military installations'}
if(@($catalogue.Id | Select-Object -Unique).Count -ne 41){throw 'Duplicate stable location IDs'}
$layer=[IO.File]::ReadAllText((Join-Path $addonRoot 'Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/MilitaryInstallations.layer'))
# Audit root data without interpreting or rewriting the user's castle/prop children.
$saved=Read-CampaignLayerLocations $layer -RootsOnly
if($saved.Count -ne 27){throw 'Expected 23 catalogued military roots, three placed seaports and the Invader stronghold'}
$strongholdId='everon_fortress_vostok'
if(!$saved.ContainsKey($strongholdId)){throw 'Invader stronghold root missing'}
if($saved[$strongholdId].Text -notmatch 'm_eType\s+MILITARY_BASE\b' -or $saved[$strongholdId].Text -notmatch 'm_eInitialControllingRole\s+INVADERS\b'){throw 'Stronghold must be an Invader-held military base'}
if(!$saved[$strongholdId].Prefab.Contains('Prefabs/Compositions/Locations/AROR_Defence_Fortress_Vostok.et')){throw 'Stronghold does not use the fortress composition'}
$fortress=[IO.File]::ReadAllText((Join-Path $addonRoot 'Prefabs/Compositions/Locations/AROR_Defence_Fortress_Vostok.et'))
if(!$fortress.Contains('AROR_StrongholdComponent') -or $fortress -notmatch 'm_iDefenceLevel 4'){throw 'Fortress composition lacks its stronghold component or level-4 garrison'}
if(!$saved['everon_airport'].Prefab.Contains('Prefabs/Compositions/Locations/AROR_Defence_Stronghold_Delaware.et')){throw 'Airport does not use the Stronghold Delaware composition'}
$delaware=[IO.File]::ReadAllText((Join-Path $addonRoot 'Prefabs/Compositions/Locations/AROR_Defence_Stronghold_Delaware.et'))
if(!$delaware.Contains('m_eHomeRole OCCUPIERS') -or ([regex]::Matches($delaware,'UH1H\.et').Count -ne 3)){throw 'Delaware must be an Occupier stronghold with three parked helicopters'}
$placedPorts=@($saved.Keys | Where-Object {$_ -notin $sites.Id -and $_ -ne $strongholdId})
if($placedPorts.Count -ne 3){throw 'Unexpected non-catalogue military roots'}
foreach($portId in $placedPorts){
 if($saved[$portId].Text -notmatch 'm_eType\s+PORT\b'){throw "Unexpected non-catalogue location: $portId"}
 if(!$saved[$portId].Prefab.Contains('Prefabs/Locations/AROR_Location_Port.et')){throw "Placed port does not inherit the expected template: $portId"}
}
$portTemplate=[IO.File]::ReadAllText((Join-Path $addonRoot 'Prefabs/Locations/AROR_Location_Port.et'))
if(!$portTemplate.Contains('AROR_GarrisonComponent') -or !$portTemplate.Contains('AROR_Prop_CaptureFlag.et')){throw 'Port template lacks garrison/capture flag'}
$definitions=(Get-Content -Raw (Join-Path $repoRoot 'docs/MILITARY_DEFENCE_PRESETS.json') | ConvertFrom-Json).Presets
function Distance($a,$b){[Math]::Sqrt([Math]::Pow(([double]$a.X-[double]$b.X),2)+[Math]::Pow(([double]$a.Z-[double]$b.Z),2))}
function PersonnelRange($type){
 switch($type){
  'OUTPOST' {return @(9,9)}
  'MILITARY_BASE' {return @(31,31)}
  'AIRPORT' {return @(48,48)}
  'STRONGHOLD' {return @(47,72)}
  default {return @(9,31)}
 }
}
$sceneSites=@($sites | ForEach-Object {
 $root=$saved[$_.Id]
 if(!$root){throw "Saved root missing: $($_.Id)"}
 $savedRole=[regex]::Match($root.Text,'m_eInitialControllingRole\s+(OCCUPIERS|INVADERS|RESISTANCE)\b')
 if(!$savedRole.Success){throw "Saved role missing: $($_.Id)"}
 $sceneType=$_.Type
 if($root.Text.Contains('AROR_Defence_Stronghold_Delaware.et')){$sceneType='STRONGHOLD'}
 [pscustomobject]@{Id=$_.Id;Type=$sceneType;Role=$savedRole.Groups[1].Value;X=$root.Origin[0];Y=$root.Origin[1];Z=$root.Origin[2]}
})
foreach($portId in $placedPorts){
 $root=$saved[$portId]
 $role=[regex]::Match($root.Text,'m_eInitialControllingRole\s+(OCCUPIERS|INVADERS|RESISTANCE)\b')
 if(!$role.Success){throw "Missing placed port role: $portId"}
 $sceneSites += [pscustomobject]@{Id=$portId;Type='PORT';Role=$role.Groups[1].Value;X=$root.Origin[0];Y=$root.Origin[1];Z=$root.Origin[2]}
}
$root=$saved[$strongholdId]
$sceneSites += [pscustomobject]@{Id=$strongholdId;Type='STRONGHOLD';Role='INVADERS';X=$root.Origin[0];Y=$root.Origin[1];Z=$root.Origin[2]}
# Circle intersections capture worst activation overlaps between site centres,
# including a player standing between four sites rather than directly at a pole.
$samples=New-Object 'Collections.Generic.List[object]'
foreach($site in $sceneSites){$samples.Add($site)}
for($a=0;$a -lt $sceneSites.Count;$a++){for($b=$a+1;$b -lt $sceneSites.Count;$b++){
 $first=$sceneSites[$a];$second=$sceneSites[$b];$distance=Distance $first $second
 if($distance -le 0 -or $distance -gt 2000){continue}
 $mx=($first.X+$second.X)/2;$mz=($first.Z+$second.Z)/2
 $height=[Math]::Sqrt([Math]::Max(0,1000000-$distance*$distance/4))
 $dx=($second.X-$first.X)/$distance;$dz=($second.Z-$first.Z)/$distance
 foreach($sign in @(-1,1)){$samples.Add([pscustomobject]@{X=$mx-$sign*$height*$dz;Z=$mz+$sign*$height*$dx})}
}}
$peak=@{}
foreach($sample in $samples){
 $near=@($sceneSites | Where-Object {(Distance $sample $_) -le 1000.001})
 $minimum=0;$maximum=0
 foreach($site in $near){$range=PersonnelRange $site.Type;$minimum+=$range[0];$maximum+=$range[1]}
 foreach($site in $near){if(!$peak.ContainsKey($site.Id) -or $maximum -gt $peak[$site.Id].Maximum){$peak[$site.Id]=[pscustomobject]@{Count=$near.Count;Minimum=$minimum;Maximum=$maximum}}}
}
$results=foreach($site in $sites){
 $root=$saved[$site.Id]
 if(!$root){throw "Saved root missing: $($site.Id)"}
 $rootIssues=New-Object 'Collections.Generic.List[string]'
 $fields=@('X','Y','Z')
 foreach($axis in 0..2){if([Math]::Abs([double]$root.Origin[$axis]-[double]$site.($fields[$axis])) -gt 0.02){$rootIssues.Add('Saved/catalogue coordinate differs: '+$fields[$axis])}}
 $yawDifference=[Math]::Abs((($root.Yaw-[double]$site.DefenceYaw+540)%360)-180)
 if($yawDifference -gt 0.02){$rootIssues.Add(('Saved yaw {0} differs from catalogue {1}; reconcile before regeneration' -f $root.Yaw,$site.DefenceYaw))}
 if($root.Text -notmatch ('m_sId "'+[regex]::Escape($site.Id)+'"')){throw "Wrong saved stable ID: $($site.Id)"}
 if($root.Text -notmatch ('m_eType\s+'+[regex]::Escape($site.Type)+'\b')){throw "Wrong military type: $($site.Id)"}
 if($root.Text -notmatch ('m_eInitialControllingRole\s+'+[regex]::Escape($site.Role)+'\b')){$rootIssues.Add('Saved initial role differs from catalogue: '+$site.Role)}
 $preset=$definitions | Where-Object Name -CEQ $site.DefencePreset
 if(!$preset){throw "Unknown defence preset: $($site.Id)"}
 $presetPath=Join-Path $addonRoot ('Prefabs/Compositions/Locations/AROR_Defence_'+$site.DefencePreset+'.et')
 $prefab=[IO.File]::ReadAllText($presetPath)
 $baseReference=[regex]::Match($prefab,'\{[A-F0-9]{16}\}(Prefabs/Locations/[^"\r\n]+\.et)')
 if(!$baseReference.Success){throw "Preset has no military location template: $($site.Id)"}
 $base=[IO.File]::ReadAllText((Join-Path $addonRoot $baseReference.Groups[1].Value))
 if(!$base.Contains('AROR_GarrisonComponent') -or !$base.Contains('AROR_Prop_CaptureFlag.et')){throw "Military template lacks garrison/capture flag: $($site.Id)"}
 $sceneSite=$sceneSites | Where-Object Id -EQ $site.Id
 $overlap=$peak[$site.Id]
 $origins=@($sceneSites | Where-Object {$_.Role -eq $sceneSite.Role -and $_.Id -ne $site.Id -and $_.Type -in @('OUTPOST','MILITARY_BASE','AIRPORT','STRONGHOLD')} | Sort-Object {Distance $sceneSite $_})
 $nearestOrigin='none after capture'
 if($origins.Count){$nearestOrigin=$origins[0].Id}
 $offset=[Math]::Sqrt([Math]::Pow(([double]$site.FlagX-[double]$site.X),2)+[Math]::Pow(([double]$site.FlagZ-[double]$site.Z),2))
 $walls=[regex]::Matches($prefab,'AROR_\w+_Wall_\d+_\d+').Count
 $notes=New-Object 'Collections.Generic.List[string]'
 foreach($issue in $rootIssues){$notes.Add($issue)}
 if($walls -eq 0){$notes.Add('Shared preset currently has no generated sandbag walls; preserve authored edits')}
 if($root.Text.Contains('MeshObject')){$notes.Add('Authored world geometry present; historical footprint audit needs visual recheck')}
 if(!$origins.Count){$notes.Add('Last initial role origin: physical QRF requires another friendly land origin or an explicitly allowed fallback')}
 if($overlap.Maximum -gt 96){$notes.Add('Overlapping activation demand can exceed the shared actor cap; staged admission is required')}
 if($offset -gt 25){$notes.Add('Catalogue flag differs from strategic centre; tactical objective must use actual flag position')}
 [pscustomobject][ordered]@{
  Id=$site.Id;Name=$site.Name;Type=$site.Type;Role=$sceneSite.Role;RootVerified=($rootIssues.Count -eq 0)
  SavedX=$root.Origin[0];SavedY=$root.Origin[1];SavedZ=$root.Origin[2];SavedYaw=$root.Yaw
  Preset=$site.DefencePreset;PresetWallChildren=$walls;AuthoredGeometry=$root.Text.Contains('MeshObject')
  CatalogueFlagOffsetMetres=[Math]::Round($offset,1);PeakNearbySites=$overlap.Count
  PeakNearbyMinimumPersonnel=$overlap.Minimum;PeakNearbyMaximumPersonnel=$overlap.Maximum;NearestSameRoleOrigin=$nearestOrigin
  Notes=($notes -join '; ');NativeAcceptance='Pending current-source compile and Play'
 }
}
$managers=[IO.File]::ReadAllText((Join-Path $addonRoot 'Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/Managers.layer'))
foreach($required in @('m_bOutpostCaptureQRFs 1','m_bInvaderOperationsEnabled 1','m_bCombatQRFs 0','m_bMinimumCaptureQRF 1','m_bInfantryFallback 1','m_bQRFRequireFriendlyOrigin 1','m_bReaperQRFTest 0')){
 if(!$managers.Contains($required)){throw "Unexpected Chicken policy: $required"}
}
if($WriteReport){
 $results | Export-Csv -LiteralPath (Join-Path $repoRoot $ReportPath) -NoTypeInformation -Encoding UTF8
}
$results | Format-Table Name,Role,PeakNearbySites,PeakNearbyMaximumPersonnel,PresetWallChildren -AutoSize
Write-Output ('Audited 23 catalogued military roots plus three placed port roots and the Invader stronghold; all 27 contribute to activation-overlap estimates. Catalogue garrison/flag bindings and Chicken capture-response policy checked. Catalogue transform differences: '+@($results | Where-Object RootVerified -eq $false).Count)
Write-Output 'Read-only scene audit. Terrain, current custom geometry, road reachability, native spawning and multiplayer are not certified.'
