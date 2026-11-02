[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'CampaignGroundingData.ps1')
$checks=0
function Check($condition,$message){if(!$condition){throw $message};$script:checks++}
$entity=@'
GenericEntity AROR_Location_everon_fixture : "{ABCDEF0123456789}Prefabs/example.et" {
 coords 10 20 30
 angles 0 315 0
 {
  GenericEntity {
   ID "0123456789ABCDEF"
   coords 1 -0.25 2
  }
 }
}
'@
$plain=Read-CampaignLayerLocations $entity
Check ($plain.Count -eq 1 -and $plain.everon_fixture.Origin[1] -eq 20) 'Generated location root not read'
Check ($plain.everon_fixture.Children['0123456789ABCDEF'][1] -eq -0.25) 'Saved wall offset lost'
$grouped='$grp GenericEntity : "{ABCDEF0123456789}Prefabs/example.et" {'+"`n"+($entity.Replace('GenericEntity AROR_Location_everon_fixture : "{ABCDEF0123456789}Prefabs/example.et" {','AROR_Location_everon_fixture {').Split("`n") | ForEach-Object {' '+$_} | Out-String)+"`n}"
$parsed=Read-CampaignLayerLocations $grouped
Check ($parsed.Count -eq 1 -and $parsed.everon_fixture.Yaw -eq 315) 'Workbench grouped prefab syntax not read'
Check ($parsed.everon_fixture.Children['0123456789ABCDEF'][2] -eq 2) 'Grouped saved child offset lost'
$authored=$entity.Replace('  GenericEntity {', '  GenericEntity CustomCastle {').Replace('   ID "0123456789ABCDEF"', '   components { MeshObject { Object "{ABCDEF0123456789}Assets/Castle.xob" } }')
$roots=Read-CampaignLayerLocations $authored -RootsOnly
Check ($roots.Count -eq 1 -and $roots.everon_fixture.Origin[0] -eq 10 -and $roots.everon_fixture.Children.Count -eq 0) 'Read-only root audit must tolerate authored children without inventing grounding IDs'
$renamed=$entity.Replace(' coords 10 20 30', " components {`n  AROR_LocationComponent {`n   m_sId `"everon_stable_id`"`n  }`n }`n coords 10 20 30")
$identity=Read-CampaignLayerLocations $renamed -RootsOnly
Check ($identity.ContainsKey('everon_stable_id') -and $identity.everon_stable_id.EntityName -eq 'everon_fixture') 'Saved campaign identity must use the component stable ID, not an editable entity name'
foreach($bad in @(($entity+"`n"+$entity),($entity.Substring(0,$entity.LastIndexOf('}'))),($entity.Replace('angles 0 315 0','angles 5 315 0')))){
 $rejected=$false;try{$null=Read-CampaignLayerLocations $bad}catch{$rejected=$true}
 Check $rejected 'Ambiguous, incomplete or tilted saved geometry must be rejected'
}
$row=[pscustomobject]@{DefencePreset='Outpost';X=10;Y=20;Z=30;DefenceYaw=315;FlagX=10;FlagY=20.905946;FlagZ=30}
$pose=Get-DefencePoseKey $row
$row.DefenceYaw=-45
Check ((Get-DefencePoseKey $row) -ceq $pose) 'Equivalent yaw should retain baked offsets'
$row.DefenceYaw=135
Check ((Get-DefencePoseKey $row) -cne $pose) 'Rotating a perimeter must invalidate old baked offsets'
$row.DefenceYaw=315;$row.X=11
Check ((Get-DefencePoseKey $row) -cne $pose) 'Relocating a root must invalidate old baked offsets'
$row.X=10;$row.FlagZ=31
Check ((Get-DefencePoseKey $row) -cne $pose) 'Moving a flag must invalidate old baked offsets'
$row.FlagZ=30;$row.DefencePreset='OutpostCompact'
Check ((Get-DefencePoseKey $row) -cne $pose) 'Changing a preset must invalidate old baked offsets'
Write-Output "$checks saved-grounding parser and pose-invalidation checks pass."
