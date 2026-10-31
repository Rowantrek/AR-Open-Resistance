[CmdletBinding()]
param([switch]$Apply)
# Generates the Fortress Vostok stronghold composition (Prefabs/Compositions/Locations/AROR_Defence_Fortress_Vostok.et):
# an 80 x 46 m high-sandbag fortress along the dry quay strip of Saint-Pierre harbour (the basin lies south of it).
# Two north corner bastions, two north-wall guns and two flank guns, marksmen on guard towers in the seaward corners, level-4 garrison
# markers, three manned hull pads and a parked hull, two helipads with unmanned Mi-8s, a bunkered keep with USSR stores
# against the seaward wall, a north gate towards the town and an outer hedgehog/wire belt on the three landward sides.
# Without -Apply it only checks that the files on disk match.
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$addonRoot = Join-Path $repoRoot 'AROpenResistance/Open Resistance'
$culture = [Globalization.CultureInfo]::InvariantCulture
$utf8 = New-Object Text.UTF8Encoding($false)
. (Join-Path $PSScriptRoot 'GunEmbrasures.ps1')
function Number($value) { ([double]$value).ToString('0.######',$culture) }
function EntityId([string]$name) {
 $digest = [Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes("FortressVostok/$name"))
 ([BitConverter]::ToString($digest).Replace('-','')).Substring(0,16)
}

$name = 'Fortress_Vostok'
$guid = '7C1E5A0D93B4F268' # Hand-authored resource GUID, recorded in the sibling .meta
$resourcePath = "Prefabs/Compositions/Locations/AROR_Defence_$name.et"
$halfWidth = 40 # East-west, along the quay
$halfDepth = 23 # North-south: the quay strip is only about 50 m deep
$pitch = 2.4
$high = '{BE16EE8FAA315FE2}Prefabs/Props/Military/Sandbags/Sandbag_01_long_high_burlap.et'
$low = '{21D83950DF36F250}Prefabs/Props/Military/Sandbags/Sandbag_01_long_burlap.et'
$bunker = '{5821AF9613353C80}Prefabs/Props/Military/Sandbags/Sandbag_01_bunker_burlap_camonet.et'
$hedgehog = '{AE28D47CF26E91EA}Prefabs/Props/Military/Fortification/CzechHedgehog_01_rusty.et'
$wire = '{BE7B5BAFE1A16A50}Prefabs/Props/Military/Fortification/BarbedTape_Coil.et'
$tower = '{DFBF655559915333}Prefabs/Compositions/Slotted/SlotFlatSmall/GuardTower_S_USSR_01.et'
$helipad = '{525C793E1943EA06}Prefabs/Compositions/Slotted/SlotFlatLarge/Helipad_L_Conflict_USSR_01.et'
$helicopter = '{DF5CCB7C0FF049F4}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_unarmed_transport.et'
$stores = @(
 @('Weapons','{8C7C140EEB0ECF36}Prefabs/Props/Military/AmmoBoxes/USSR/EquipmentBoxWooden_Weapon_01_USSR.et'),
 @('Ammunition','{A48EDE9CBA71B5A3}Prefabs/Props/Military/AmmoBoxes/USSR/EquipmentBoxWooden_Ammunition_01_USSR.et'),
 @('Equipment','{CD62E36CFDCB8FD8}Prefabs/Props/Military/AmmoBoxes/USSR/EquipmentBoxWooden_Equipment_01_USSR.et'),
 @('Medical','{70837F44CD03FD05}Prefabs/Props/Military/AmmoBoxes/USSR/EquipmentBoxWooden_Medical_01_USSR.et'))

# Guns: x, z, facing, platform height. All face the landward approaches; the harbour covers the south.
$guns = @(@(-36,19,315,0),@(36,19,45,0),@(-18,19,0,0),@(18,19,0,0),@(-37,0,270,0),@(37,0,90,0))
# Marksman towers: name, x, z, facing
$towers = @(@('SouthWest',-36,-19,225),@('SouthEast',36,-19,135))
# Helipads in the east and west courtyard, each with a parked Mi-8 (rotor about 21 m across), lengthwise along the quay
$pads = @(@(1,-25,8),@(2,25,8))
# Low wall spans per side (0 north, 1 east, 2 south, 3 west): from, to along the wall
$lowSpans = @{ 0 = @(@(-40,-30),@(-24,-12),@(12,24),@(30,40)); 1 = @(@(-6,6),@(12,23)); 2 = @(); 3 = @(@(-6,6),@(12,23)) }
$entrance = @(-6,6) # North gate towards the town, wide enough for the manned hulls
# Infantry teams clear of the helicopters' rotor discs
$teams = @(@(0,14,0),@(-8,14,315),@(8,14,45),@(0,-4,180),@(-12,0,270),@(12,0,90),@(-31,-4,270),@(31,-4,90))
$hulls = @(@(-24,-10),@(-8,-10),@(8,-10))
$parked = @(24,-10)

$lines = New-Object 'Collections.Generic.List[string]'
function Child([string]$entity, [string]$prefab, $x, $z, $yaw, [string[]]$components) {
 $header = '  GenericEntity {0}' -f $entity
 if ($prefab) { $header += (' : "{0}"' -f $prefab) }
 $lines.Add($header + ' {')
 $lines.Add(('   ID "{0}"' -f (EntityId $entity)))
 $lines.Add('   components {')
 # Composite prefabs (slotted compositions, the sandbag bunker) bring their own Hierarchy; a second one is rejected
 if ($prefab -notmatch 'Compositions/Slotted|_bunker_') {
  $lines.Add('    Hierarchy {')
  $lines.Add('    }')
 }
 foreach ($component in $components) { $lines.Add($component) }
 $lines.Add('   }')
 $lines.Add(('   coords {0} 0 {1}' -f (Number $x),(Number $z)))
 if ($yaw -ne 0) { $lines.Add(('   angles 0 {0} 0' -f (Number $yaw))) }
 $lines.Add('  }')
}
$wall = @('    AROR_DefenceWallComponent {','    }')

$lines.Add('GenericEntity : "{118F89378FB3D213}Prefabs/Locations/AROR_Location_MilitaryBase.et" {')
$lines.Add((' ID "{0}"' -f (EntityId 'root')))
$lines.Add(' components {')
$lines.Add('  AROR_LocationComponent "{6AA28237301A1CF9}" {')
$lines.Add('   m_sName "Fortress"')
$lines.Add('   m_fRadius 200')
$lines.Add('   m_bGroundOnInit 0')
$lines.Add('  }')
$lines.Add('  AROR_GarrisonComponent "{66D30571F98F4BE3}" {')
$lines.Add('   m_iDefenceLevel 4')
$lines.Add('   m_iStaticWeaponCountOverride 6')
$lines.Add('   m_aExtraParkedVehicles {')
foreach ($pad in $pads) { $lines.Add(('    "{0}"' -f $helicopter)) }
$lines.Add('   }')
$lines.Add(('   m_fDefenceHalfExtent {0}' -f $halfWidth))
$lines.Add(('   m_fDefenceHalfDepth {0}' -f $halfDepth))
$lines.Add('   m_fMaxDefenceRelief 4')
$lines.Add('   m_fEditorSearchRadius 1600')
$lines.Add('   m_fDefenceHoldRadius 6')
$lines.Add('  }')
$lines.Add('  AROR_StrongholdComponent {')
$lines.Add('  }')
$lines.Add(' }')
$lines.Add(' coords 0 0 0')
$lines.Add(' {')
# Inherited capture flag at the centre, keeping the model-base and interaction offsets of the other compositions
$lines.Add('  GenericEntity {')
$lines.Add('   ID "6AACA42C3DE4F4A5"')
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

# Perimeter: high sandbags, low across gun fields of fire, open at the north gate
for ($side = 0; $side -lt 4; $side++) {
 $length = $halfWidth; if ($side -eq 1 -or $side -eq 3) { $length = $halfDepth }
 $segments = [int][Math]::Ceiling((2 * $length) / $pitch)
 for ($segment = 0; $segment -lt $segments; $segment++) {
  $along = -$length + ($segment + 0.5) * (2 * $length / $segments)
  if ($side -eq 0 -and $along -gt $entrance[0] -and $along -lt $entrance[1]) { continue }
  $prefab = $high
  foreach ($span in $lowSpans[$side]) { if ($along -ge $span[0] -and $along -le $span[1]) { $prefab = $low } }
  $x = $along; $z = $halfDepth; $yaw = 0
  if ($side -eq 1) { $x = $halfWidth; $z = $along; $yaw = 90 }
  if ($side -eq 2) { $z = -$halfDepth }
  if ($side -eq 3) { $x = -$halfWidth; $z = $along; $yaw = 90 }
  # Open embrasure: tripod guns fire from below even a low sandbag wall
  if (Test-InEmbrasure $x $z $guns) { continue }
  Child "AROR_Fortress_Vostok_Perimeter_${side}_$segment" $prefab $x $z $yaw $wall
 }
}

# Guard towers in the empty seaward corners, clear of the rotors: each is a marksman's post (the tower's sandbag
# parapet masks a tripod gun, so the guns stay in their pits)
$post = @('    AROR_DefenceWallComponent {','    }','    AROR_SentryPostComponent {','     m_fFloorHeight 3.5','    }')
foreach ($corner in $towers) { Child ("AROR_Fortress_Vostok_Tower_{0}" -f $corner[0]) $tower $corner[1] $corner[2] $corner[3] $post }

# Helipads: tagged so their deck and lights do not count against the parked helicopter
$padComponents = @('    AROR_DefenceWallComponent {','    }','    AROR_LandingPadComponent {','     m_fRotorRadius 10.7','     m_fLength 25.3','    }')
foreach ($pad in $pads) { Child ("AROR_Fortress_Vostok_Helipad_{0}" -f $pad[0]) $helipad $pad[1] $pad[2] 90 $padComponents }

# Gun pits on the ground: a low traverse either side of each gun, parallel to its line of fire
for ($gun = 0; $gun -lt $guns.Count; $gun++) {
 if ($guns[$gun][3] -gt 0) { continue }
 $yaw = $guns[$gun][2]; $angle = $yaw * [Math]::PI / 180
 $rightX = [Math]::Cos($angle); $rightZ = -[Math]::Sin($angle)
 foreach ($side in @(-1,1)) {
  $x = $guns[$gun][0] + $rightX * 3.5 * $side
  $z = $guns[$gun][1] + $rightZ * 3.5 * $side
  $label = 'L'; if ($side -gt 0) { $label = 'R' }
  Child "AROR_Fortress_Vostok_Gun_${gun}_Traverse_$label" $low $x $z ($yaw + 90) $wall
 }
}

# The keep against the seaward wall: command bunker between two barracks bunkers, screened from the courtyard.
# Gate guard posts and obstacles at the north gate.
Child 'AROR_Fortress_Vostok_Command_Bunker' $bunker 0 -18 0 $wall
Child 'AROR_Fortress_Vostok_Barracks_Bunker' $bunker -16 -18 0 $wall
Child 'AROR_Fortress_Vostok_Barracks_Bunker_East' $bunker 16 -18 0 $wall
foreach ($screen in @(@('West',-4),@('East',4))) {
 Child ("AROR_Fortress_Vostok_Keep_Screen_{0}" -f $screen[0]) $low $screen[1] -14 0 $wall
}
Child 'AROR_Fortress_Vostok_Gate_Guard_Left' $low ($entrance[0] - 2) ($halfDepth - 3) 90 $wall
Child 'AROR_Fortress_Vostok_Gate_Guard_Right' $low ($entrance[1] + 2) ($halfDepth - 3) 90 $wall
Child 'AROR_Fortress_Vostok_Gate_Obstacle_Left' $hedgehog ($entrance[0] - 2) ($halfDepth + 6) 15 $wall
Child 'AROR_Fortress_Vostok_Gate_Obstacle_Right' $hedgehog ($entrance[1] + 2) ($halfDepth + 6) -15 $wall

# Outer belt on the landward sides: hedgehogs 7 m out, wire coils between them 4 m out, the gate approach left open.
# Nothing south of the seaward wall line, which is the harbour edge.
for ($side = 0; $side -lt 4; $side++) {
 if ($side -eq 2) { continue }
 $reach = $halfWidth + 4; if ($side -ne 0) { $reach = $halfDepth + 4 }
 $offset = $halfDepth; if ($side -ne 0) { $offset = $halfWidth }
 for ($step = 0; $step -le [int][Math]::Floor(2 * $reach / 8); $step++) {
  $along = -$reach + $step * 8
  foreach ($ring in @(@('Hedgehog',$hedgehog,($offset + 7),0),@('Wire',$wire,($offset + 4),4))) {
   $position = $along + $ring[3]
   if ($position -gt $reach) { continue }
   if ($side -ne 0 -and $position -lt -($halfDepth - 4)) { continue }
   if ($side -eq 0 -and $position -gt ($entrance[0] - 4) -and $position -lt ($entrance[1] + 4)) { continue }
   $x = $position; $z = $ring[2]; $yaw = 0
   if ($side -eq 1) { $x = $ring[2]; $z = $position; $yaw = 90 }
   if ($side -eq 3) { $x = -$ring[2]; $z = $position; $yaw = 90 }
   Child ("AROR_Fortress_Vostok_{0}_{1}_{2}" -f $ring[0],$side,$step) $ring[1] $x $z $yaw $wall
  }
 }
}

# USSR stores in the keep's south-west corner
$storeX = @(-31,-28,-25,-22)
for ($store = 0; $store -lt $stores.Count; $store++) {
 $supply = @('    AROR_DefenceWallComponent {','    }','    AROR_OutpostSupplyComponent {',('     m_sCategory "{0}"' -f $stores[$store][0]),'    }')
 Child ("AROR_Fortress_Vostok_Loot_{0}" -f $stores[$store][0]) $stores[$store][1] $storeX[$store] -20 0 $supply
}

# Garrison markers. Kind: 0 infantry team, 1 gun, 2 parked hull, 3 manned hull, 4 crew staging (ordinal = group)
$positions = @()
for ($team = 0; $team -lt $teams.Count; $team++) { $positions += ,@(0,$team,$teams[$team][0],$teams[$team][1],$teams[$team][2]) }
for ($gun = 0; $gun -lt $guns.Count; $gun++) { $positions += ,@(1,$gun,$guns[$gun][0],$guns[$gun][1],$guns[$gun][2],$guns[$gun][3]) }
$positions += ,@(2,0,$parked[0],$parked[1],0)
foreach ($pad in $pads) { $positions += ,@(2,$pad[0],$pad[1],$pad[2],90) }
for ($hull = 0; $hull -lt $hulls.Count; $hull++) { $positions += ,@(3,$hull,$hulls[$hull][0],$hulls[$hull][1],0) }
# Groups 8-13 are the gun crews and 14-16 the hull crews; stage them in two rows in the courtyard, clear of the hull pads
for ($group = 8; $group -le 16; $group++) {
 $rank = $group - 8
 if ($rank -lt 5) { $positions += ,@(4,$group,(-8 + $rank * 4),8,180) } else { $positions += ,@(4,$group,(-6 + ($rank - 5) * 4),1,180) }
}
foreach ($position in $positions) {
 $marker = @('    AROR_DefencePositionComponent {',('     m_iKind {0}' -f $position[0]),('     m_iOrdinal {0}' -f $position[1]))
 if ($position.Count -gt 5 -and $position[5] -gt 0) { $marker += ('     m_fElevation {0}' -f (Number $position[5])) }
 $marker += '    }'
 Child ("AROR_Fortress_Vostok_Defence_{0}_{1}" -f $position[0],$position[1]) '' $position[2] $position[3] $position[4] $marker
}
$lines.Add(' }')
$lines.Add('}')

$path = Join-Path $addonRoot $resourcePath
$text = ($lines -join "`r`n") + "`r`n"
$meta = 'MetaFileClass {' + "`r`n" + (' Name "{{{0}}}{1}"' -f $guid,$resourcePath) + "`r`n Configurations {`r`n"
foreach ($platform in @('PC','XBOX_ONE','XBOX_SERIES','PS4','PS5','HEADLESS')) {
 $parent = ''; if ($platform -ne 'PC') { $parent = ' : PC' }
 $meta += "  EntityTemplateResourceClass $platform$parent {`r`n  }`r`n"
}
$meta += " }`r`n}`r`n"
if ($Apply) {
 [IO.File]::WriteAllText($path,$text,$utf8)
 [IO.File]::WriteAllText($path + '.meta',$meta,$utf8)
 Write-Output "Wrote $resourcePath"
} else {
 foreach ($expected in @(@($path,$text),@(($path + '.meta'),$meta))) {
  if (!(Test-Path -LiteralPath $expected[0]) -or [IO.File]::ReadAllText($expected[0]).Replace("`r`n","`n") -cne $expected[1].Replace("`r`n","`n")) { throw "Fortress composition differs from this generator: $($expected[0])" }
 }
 Write-Output 'Fortress Vostok composition matches its generator. Native discovery, grounding, gun arcs and placement remain Workbench checks.'
}
