[CmdletBinding()]
param([switch]$Apply)
# Generates the Stronghold Delaware composition (Prefabs/Compositions/Locations/AROR_Defence_Stronghold_Delaware.et): the
# Occupiers' home base at Everon International Airport. It inherits the 88 m airbase compound and adds an outer 128 m ring
# of high sandbags, two corner gun bastions, three revetted US helipads with unmanned UH1Hs, four extra infantry teams,
# an outer hedgehog/wire belt and the stronghold component. Without -Apply it only checks that the files on disk match.
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$addonRoot = Join-Path $repoRoot 'AROpenResistance/Open Resistance'
$culture = [Globalization.CultureInfo]::InvariantCulture
$utf8 = New-Object Text.UTF8Encoding($false)
. (Join-Path $PSScriptRoot 'GunEmbrasures.ps1')
function Number($value) { ([double]$value).ToString('0.######',$culture) }
function EntityId([string]$name) {
 $digest = [Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes("StrongholdDelaware/$name"))
 ([BitConverter]::ToString($digest).Replace('-','')).Substring(0,16)
}

$guid = '5D2A8E41C7B03F96' # Hand-authored resource GUID, recorded in the sibling .meta
$resourcePath = 'Prefabs/Compositions/Locations/AROR_Defence_Stronghold_Delaware.et'
$outer = 64 # Outer wall; the garrison half-extent matches it so pads in the outer band are inside the footprint
# Chicken's airport keeps its location root fixed and moves the inherited airbase compound (flag, walls, markers) to
# this offset in the world layer. Everything added here is centred on the compound, not the root.
$centerX = -49.996
$centerZ = 65.005
$pitch = 2.4
$gate = @(-8,8)
$high = '{BE16EE8FAA315FE2}Prefabs/Props/Military/Sandbags/Sandbag_01_long_high_burlap.et'
$low = '{21D83950DF36F250}Prefabs/Props/Military/Sandbags/Sandbag_01_long_burlap.et'
$hedgehog = '{AE28D47CF26E91EA}Prefabs/Props/Military/Fortification/CzechHedgehog_01_rusty.et'
$wire = '{BE7B5BAFE1A16A50}Prefabs/Props/Military/Fortification/BarbedTape_Coil.et'
$helicopter = '{70BAEEFC2D3FEE64}Prefabs/Vehicles/Helicopters/UH1H/UH1H.et'
$helipad = '{A13F6A7F82DF031C}Prefabs/Compositions/Slotted/SlotFlatLarge/Helipad_L_Conflict_US_01.et'
# Corner bastions in the outer band (the airbase's inherited inner wall cannot be opened from here); the inherited
# airbase guns 0-3 face out from each side, and every gun needs an embrasure through the outer ring
$guns = @(@(4,-56,56,315),@(5,56,56,45))
$lanes = @(@(0,40,0),@(-40,0,270),@(40,0,90),@(0,-40,180),@(-56,56,315),@(56,56,45))
# Helicopter pads on the open south apron between the walls, clear of the gate lane and every gun's field of fire
# (the east band runs into the hangars and fence). Ordinal 0 is the spare armour.
$pads = @(@(1,-42,-54),@(2,-20,-54),@(3,32,-54))
# Extra infantry teams (ordinals after the airbase's eight) in the outer band, clear of the guns and pads
$teams = @(@(8,-24,54,0),@(9,24,54,0),@(10,-54,-30,270),@(11,54,-54,135))

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
 $lines.Add(('   coords {0} 0 {1}' -f (Number ($x + $centerX)),(Number ($z + $centerZ))))
 if ($yaw -ne 0) { $lines.Add(('   angles 0 {0} 0' -f (Number $yaw))) }
 $lines.Add('  }')
}
$wall = @('    AROR_DefenceWallComponent {','    }')

$lines.Add('GenericEntity : "{37786477B1914A61}Prefabs/Compositions/Locations/AROR_Defence_Airbase.et" {')
$lines.Add((' ID "{0}"' -f (EntityId 'root')))
$lines.Add(' components {')
$lines.Add('  AROR_GarrisonComponent "{66D30571F98F4BE3}" {')
$lines.Add('   m_iStaticWeaponCountOverride 6')
$lines.Add('   m_iExtraInfantryTeams 4')
$lines.Add('   m_aExtraParkedVehicles {')
foreach ($pad in $pads) { $lines.Add(('    "{0}"' -f $helicopter)) }
$lines.Add('   }')
$lines.Add(('   m_fDefenceHalfExtent {0}' -f $outer))
$lines.Add('   m_fMaxDefenceRelief 3')
$lines.Add('  }')
$lines.Add('  AROR_StrongholdComponent {')
$lines.Add('   m_eHomeRole OCCUPIERS')
$lines.Add('   m_sRetaliationRecipeId "occupier_ground_l5"')
$lines.Add('   m_sFinalDefenceRecipeId "occupier_ground_l5"')
$lines.Add('   m_bLaunchFromFriendlyBase 1')
$lines.Add('   m_fLandingArc 360')
$lines.Add('   m_fLandingMinDistance 800')
$lines.Add('  }')
$lines.Add(' }')
$lines.Add(' coords 0 0 0')
$lines.Add(' {')

# Outer ring of high sandbags, open at the south gate in line with the airbase entrance
$segments = [int][Math]::Ceiling((2 * $outer) / $pitch)
for ($side = 0; $side -lt 4; $side++) {
 for ($segment = 0; $segment -lt $segments; $segment++) {
  $along = -$outer + ($segment + 0.5) * (2 * $outer / $segments)
  if ($side -eq 2 -and $along -gt $gate[0] -and $along -lt $gate[1]) { continue }
  $x = $along; $z = $outer; $yaw = 0
  if ($side -eq 1) { $x = $outer; $z = $along; $yaw = 90 }
  if ($side -eq 2) { $z = -$outer }
  if ($side -eq 3) { $x = -$outer; $z = $along; $yaw = 90 }
  # Open embrasure: tripod guns fire from below even a low sandbag wall
  if (Test-InEmbrasure $x $z $lanes) { continue }
  Child "AROR_Stronghold_Delaware_OuterWall_${side}_$segment" $high $x $z $yaw $wall
 }
}

# Corner bastions: a low traverse either side of each gun, parallel to its line of fire
foreach ($gun in $guns) {
 $angle = $gun[3] * [Math]::PI / 180
 $rightX = [Math]::Cos($angle); $rightZ = -[Math]::Sin($angle)
 foreach ($side in @(-1,1)) {
  $label = 'L'; if ($side -gt 0) { $label = 'R' }
  Child ("AROR_Stronghold_Delaware_Gun_{0}_Traverse_{1}" -f $gun[0],$label) $low ($gun[1] + $rightX * 3.5 * $side) ($gun[2] + $rightZ * 3.5 * $side) ($gun[3] + 90) $wall
 }
}

# Helipads under the parked helicopters: tagged so their deck and lights do not count against the helicopter
$padComponents = @('    AROR_DefenceWallComponent {','    }','    AROR_LandingPadComponent {','     m_fRotorRadius 7.4','     m_fLength 17.6','    }')
foreach ($pad in $pads) { Child ("AROR_Stronghold_Delaware_Helipad_{0}" -f $pad[0]) $helipad $pad[1] $pad[2] 0 $padComponents }

# Helicopter revetments: low sandbag runs either side of each pad, along the helicopter's length
foreach ($pad in $pads) {
 foreach ($side in @(-1,1)) {
  for ($piece = -2; $piece -le 2; $piece++) {
   $label = 'W'; if ($side -gt 0) { $label = 'E' }
   Child ("AROR_Stronghold_Delaware_Pad_{0}_Revetment_{1}_{2}" -f $pad[0],$label,($piece + 2)) $low ($pad[1] + 9 * $side) ($pad[2] + $piece * 2.4) 90 $wall
  }
 }
}

# Outer gate obstacles and guard posts
Child 'AROR_Stronghold_Delaware_Gate_Guard_Left' $low ($gate[0] - 2) (-$outer + 3) 90 $wall
Child 'AROR_Stronghold_Delaware_Gate_Guard_Right' $low ($gate[1] + 2) (-$outer + 3) 90 $wall
Child 'AROR_Stronghold_Delaware_Gate_Obstacle_Left' $hedgehog ($gate[0] - 2) (-$outer - 6) 15 $wall
Child 'AROR_Stronghold_Delaware_Gate_Obstacle_Right' $hedgehog ($gate[1] + 2) (-$outer - 6) -15 $wall

# Outer belt: hedgehogs 7 m out, wire coils between them 4 m out, the gate approach left open
for ($side = 0; $side -lt 4; $side++) {
 for ($step = 0; $step -le 14; $step++) {
  $along = -70 + $step * 10
  foreach ($ring in @(@('Hedgehog',$hedgehog,($outer + 7),0),@('Wire',$wire,($outer + 4),5))) {
   $position = $along + $ring[3]
   if ($position -gt 70) { continue }
   if ($side -eq 2 -and $position -gt ($gate[0] - 4) -and $position -lt ($gate[1] + 4)) { continue }
   $x = $position; $z = $ring[2]; $yaw = 0
   if ($side -eq 1) { $x = $ring[2]; $z = $position; $yaw = 90 }
   if ($side -eq 2) { $z = -$ring[2] }
   if ($side -eq 3) { $x = -$ring[2]; $z = $position; $yaw = 90 }
   Child ("AROR_Stronghold_Delaware_{0}_{1}_{2}" -f $ring[0],$side,$step) $ring[1] $x $z $yaw $wall
  }
 }
}

# Garrison markers. Kind: 0 infantry team, 1 gun, 2 parked vehicle
$positions = @()
foreach ($team in $teams) { $positions += ,@(0,$team[0],$team[1],$team[2],$team[3]) }
foreach ($gun in $guns) { $positions += ,@(1,$gun[0],$gun[1],$gun[2],$gun[3]) }
foreach ($pad in $pads) { $positions += ,@(2,$pad[0],$pad[1],$pad[2],0) }
foreach ($position in $positions) {
 $marker = @('    AROR_DefencePositionComponent {',('     m_iKind {0}' -f $position[0]),('     m_iOrdinal {0}' -f $position[1]),'    }')
 Child ("AROR_Stronghold_Delaware_Defence_{0}_{1}" -f $position[0],$position[1]) '' $position[2] $position[3] $position[4] $marker
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
  if (!(Test-Path -LiteralPath $expected[0]) -or [IO.File]::ReadAllText($expected[0]).Replace("`r`n","`n") -cne $expected[1].Replace("`r`n","`n")) { throw "Delaware composition differs from this generator: $($expected[0])" }
 }
 Write-Output 'Stronghold Delaware composition matches its generator. Native discovery, grounding, gun arcs, pads and placement remain Workbench checks.'
}
