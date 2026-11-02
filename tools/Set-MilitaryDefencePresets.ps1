[CmdletBinding()]
param([switch]$Apply)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$addonRoot = Join-Path $repoRoot 'AROpenResistance/Open Resistance'
$definition = Get-Content -Encoding utf8 -Raw (Join-Path $repoRoot 'docs/MILITARY_DEFENCE_PRESETS.json') | ConvertFrom-Json
$culture = [Globalization.CultureInfo]::InvariantCulture
$utf8 = New-Object Text.UTF8Encoding($false)
. (Join-Path $PSScriptRoot 'GunEmbrasures.ps1')
function Number($value) { ([double]$value).ToString('0.######',$culture) }
function EntityId([string]$name) {
 $digest = [Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($name))
 ([BitConverter]::ToString($digest).Replace('-','')).Substring(0,16)
}
foreach ($preset in $definition.Presets) {
 $name = $preset.Name
 $half = [double]$preset.HalfExtent
 $resourcePath = "Prefabs/Compositions/Locations/AROR_Defence_$name.et"
 $path = Join-Path $addonRoot $resourcePath
 $lines = New-Object 'Collections.Generic.List[string]'
 $lines.Add(('GenericEntity : "{{{0}}}Prefabs/Locations/AROR_Location_{1}.et" {{' -f $preset.SourceGuid,$preset.SourceName))
 $lines.Add((' ID "{0}"' -f (EntityId "$name/root")))
 $lines.Add(' components {')
 if ($preset.SourceName -eq 'MilitaryBase') {
  $lines.Add('  AROR_LocationComponent "{6AA28237301A1CF9}" {')
  $lines.Add('   m_fRadius 150') # Preserve the removed MilitaryOutpost alias default.
  $lines.Add('  }')
 }
 $lines.Add('  AROR_GarrisonComponent "{66D30571F98F4BE3}" {')
 $lines.Add(('   m_fDefenceHalfExtent {0}' -f (Number $half)))
 $lines.Add('   m_fMaxDefenceRelief 2')
 if ($preset.SearchRadius) { $lines.Add(('   m_fEditorSearchRadius {0}' -f (Number $preset.SearchRadius))) }
 $lines.Add('   m_fDefenceHoldRadius 6')
 $lines.Add('  }')
 $lines.Add(' }')
 $lines.Add(' coords 0 0 0')
 $lines.Add(' {')
 # Centre the flag in this new composition, keeping model-base and interaction clearance.
 $lines.Add('  GenericEntity {')
 $lines.Add(('   ID "{0}"' -f $preset.FlagId))
 $lines.Add('   components {')
 $lines.Add('    AROR_CaptureFlagPlacementComponent "{F9A38254AA9B454D}" {')
 $lines.Add('     m_bAutoPlace 0')
 $lines.Add('    }')
 $lines.Add('    ActionsManagerComponent "{6AAE5E8DE935135E}" {')
 $lines.Add('     ActionContexts {')
 $lines.Add('      UserActionContext "{6AAE5E8DEECDFE8B}" {')
 $lines.Add('       Position PointInfo "{6AAE5E8DE4DBC0C4}" {')
 $lines.Add('        Offset 0 0.694054 0')
 $lines.Add('       }')
 $lines.Add('      }')
 $lines.Add('     }')
 $lines.Add('    }')
 $lines.Add('   }')
 $lines.Add('   coords 0 0.905946 0')
 $lines.Add('  }')
 # Gun lanes, computed as for the markers below: walls in a gun's firing lane are left out (see GunEmbrasures.ps1)
 $laneCount = 4
 if ($preset.SourceName -eq 'Outpost') { $laneCount = 1 }
 if ($preset.SourceName -in @('MilitaryBase','Resources','Factory','Port')) { $laneCount = 3 }
 $lanePositions = @(@(0,($half-4),0),@(-($half-4),0,270),@(($half-4),0,90),@(0,-($half-4),180))
 if ($preset.CompactTeams) { $lanePositions=@(@(0,20,0),@(-20,6,270),@(20,6,90)) }
 if ($preset.MGPositions) { $lanePositions=$preset.MGPositions }
 $lanes = @($lanePositions | Select-Object -First $laneCount)
 # Low sandbag walls on all sides, with a ten-metre south entrance and open gun embrasures.
 $segments = [int][Math]::Ceiling((2 * $half) / [double]$definition.WallPitch)
 for ($side = 0; $side -lt 4; $side++) {
  for ($segment = 0; $segment -lt $segments; $segment++) {
   $along = -$half + ($segment + 0.5) * (2 * $half / $segments)
   if ($side -eq 2 -and [Math]::Abs($along) -lt ([double]$definition.EntranceWidth / 2 + 1.25)) { continue }
   $wallX = $along; $wallZ = $half; $wallYaw = 0
   if ($side -eq 1) { $wallX = $half; $wallZ = $along; $wallYaw = 90 }
   if ($side -eq 2) { $wallZ = -$half }
   if ($side -eq 3) { $wallX = -$half; $wallZ = $along; $wallYaw = 90 }
   if (Test-InEmbrasure $wallX $wallZ $lanes) { continue }
   $lines.Add(('  GenericEntity AROR_{0}_Wall_{1}_{2} : "{3}" {{' -f $name,$side,$segment,$definition.WallResource))
   $lines.Add(('   ID "{0}"' -f (EntityId "$name/wall/$side/$segment")))
   $lines.Add('   components {')
   $lines.Add('    Hierarchy {')
   $lines.Add('    }')
   $lines.Add('    AROR_DefenceWallComponent {')
   $lines.Add('    }')
   $lines.Add('   }')
   $lines.Add(('   coords {0} 0 {1}' -f (Number $wallX),(Number $wallZ)))
   if ($wallYaw -ne 0) { $lines.Add(('   angles 0 {0} 0' -f $wallYaw)) }
   $lines.Add('  }')
  }
 }
 # Standard editable marker entities. The garrison supplies faction-correct assets/crews at runtime.
 $positions = @()
 $teamCount = 8; $mgCount = 4; $vehicleCount = 3
 if ($preset.SourceName -eq 'Outpost') { $teamCount = 2; $mgCount = 1; $vehicleCount = 0 }
 if ($preset.SourceName -eq 'MilitaryBase') { $teamCount = 6; $mgCount = 3; $vehicleCount = 1 }
 if ($preset.SourceName -in @('Resources','Factory','Port')) { $teamCount = 6; $mgCount = 3; $vehicleCount = 1 }
 $bearings = @(45,225,135,315,0,180,90,270)
 for ($team=0; $team -lt $teamCount; $team++) {
  $angle = $bearings[$team] * [Math]::PI / 180
  if ($preset.CompactTeams) {
   $compact = @(@(-12,12),@(12,12),@(-16,-16),@(16,-16),@(0,10),@(0,-16))
   if($preset.TeamPositions){$compact=$preset.TeamPositions}
   $positions += ,@(0,$team,$compact[$team][0],$compact[$team][1],$bearings[$team])
  } else {
   $ring=$half-8
   if($team -ge 4){$ring=$half-12}
   $positions += ,@(0,$team,([Math]::Sin($angle)*$ring),([Math]::Cos($angle)*$ring),$bearings[$team])
  }
 }
 $mgPositions = @(@(0,($half-4),0),@(-($half-4),0,270),@(($half-4),0,90),@(0,-($half-4),180))
 if ($preset.CompactTeams) { $mgPositions=@(@(0,20,0),@(-20,6,270),@(20,6,90)) }
 if ($preset.MGPositions) { $mgPositions=$preset.MGPositions }
 for ($mg=0; $mg -lt $mgCount; $mg++) { $positions += ,@(1,$mg,$mgPositions[$mg][0],$mgPositions[$mg][1],$mgPositions[$mg][2]) }
 if ($vehicleCount -gt 0) { $positions += ,@(2,0,-10,-5,180) }
 $vehiclePads = @(@(10,-5),@(-10,8),@(10,8))
 for ($vehicle=0; $vehicle -lt $vehicleCount; $vehicle++) { $positions += ,@(3,$vehicle,$vehiclePads[$vehicle][0],$vehiclePads[$vehicle][1],180) }
 foreach ($position in $positions) {
  $kind=$position[0]; $ordinal=$position[1]
  $lines.Add(('  GenericEntity AROR_{0}_Defence_{1}_{2} {{' -f $name,$kind,$ordinal))
  $lines.Add(('   ID "{0}"' -f (EntityId "$name/position/$kind/$ordinal")))
  $lines.Add('   components {')
  $lines.Add('    Hierarchy {')
  $lines.Add('    }')
  $lines.Add('    AROR_DefencePositionComponent {')
  $lines.Add(('     m_iKind {0}' -f $kind))
  $lines.Add(('     m_iOrdinal {0}' -f $ordinal))
  $lines.Add('    }')
  $lines.Add('   }')
  $lines.Add(('   coords {0} 0 {1}' -f (Number $position[2]),(Number $position[3])))
  $lines.Add(('   angles 0 {0} 0' -f $position[4]))
  $lines.Add('  }')
 }
 $lines.Add(' }')
 $lines.Add('}')
 $text = ($lines -join "`r`n") + "`r`n"
 $meta = 'MetaFileClass {' + "`r`n" + (' Name "{{{0}}}{1}"' -f $preset.Guid,$resourcePath) + "`r`n Configurations {`r`n"
 foreach ($platform in @('PC','XBOX_ONE','XBOX_SERIES','PS4','PS5','HEADLESS')) {
  $parent = ''; if ($platform -ne 'PC') { $parent=' : PC' }
  $meta += "  EntityTemplateResourceClass $platform$parent {`r`n  }`r`n"
 }
 $meta += " }`r`n}`r`n"
 if ($Apply) {
  [IO.Directory]::CreateDirectory((Split-Path -Parent $path)) | Out-Null
  [IO.File]::WriteAllText($path,$text,$utf8)
  [IO.File]::WriteAllText($path+'.meta',$meta,$utf8)
 } else {
  foreach ($expected in @(@($path,$text),@(($path+'.meta'),$meta))) {
   if (!(Test-Path -LiteralPath $expected[0]) -or [IO.File]::ReadAllText($expected[0]).Replace("`r`n","`n") -cne $expected[1].Replace("`r`n","`n")) { throw "Preset differs from definitions: $($expected[0])" }
  }
 }
}
Write-Output "$($definition.Presets.Count) military defence presets match their definitions. Native resource discovery, wall joints, fields of fire and flat-site placement remain Workbench checks."
