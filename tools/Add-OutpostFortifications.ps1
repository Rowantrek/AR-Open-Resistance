[CmdletBinding()]
param([switch]$Apply)
# Adds fortifications for the AI to the shared outpost, military base, factory and depot compositions
# (Prefabs/Compositions/Locations/AROR_Defence_*.et; the strongholds have their own generators):
# - a US guard tower beside the north-wall gun pit (and the east flank gun at larger sites), tagged as a sentry post:
#   the garrison posts a marksman on it (the tower's sandbag parapet masks a tripod gun, so guns stay in their pits);
# - a low sandbag fighting position in front of every infantry team position, facing its sector;
# - a camouflaged sandbag bunker where the layout has none, in the most open spot; every bunker faces out from the
#   compound (nearest quarter turn) and is a ground-level sentry post with its own soldier.
# Everything is placed clear of spawn positions, gun lanes and other props (the layout test's estimated envelopes).
# Additions are named *_Fort_* with deterministic IDs.
# Without -Apply it only checks that every listed composition already carries its fortifications.
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$folder = Join-Path $repoRoot 'AROpenResistance/Open Resistance/Prefabs/Compositions/Locations'
$layerPath = Join-Path $repoRoot 'AROpenResistance/Open Resistance/Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/MilitaryInstallations.layer'
$culture = [Globalization.CultureInfo]::InvariantCulture
$utf8 = New-Object Text.UTF8Encoding($false)
function Number($value) { ([double]$value).ToString('0.######',$culture) }
function EntityId([string]$name) {
 $digest = [Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes("OutpostFortifications/$name"))
 ([BitConverter]::ToString($digest).Replace('-','')).Substring(0,16)
}

$tower = '{AB87B392A989CA95}Prefabs/Compositions/Slotted/SlotFlatSmall/GuardTower_S_US_01.et'
$low = '{21D83950DF36F250}Prefabs/Props/Military/Sandbags/Sandbag_01_long_burlap.et'
$bunker = '{5821AF9613353C80}Prefabs/Props/Military/Sandbags/Sandbag_01_bunker_burlap_camonet.et'
$floorHeight = 3.5 # Guard tower platform above the terrain; the garrison finds the actual floor near it
$composite = 'Compositions/Slotted|_bunker_'

# Composite children (bunkers, slotted compositions) must not repeat their prefab's Hierarchy component: Workbench
# rejects the whole component block, dropping the wall/sentry tags. The stronghold generators handle their own files.
$duplicateHierarchy = '(?m)(^  GenericEntity \w+ : "[^"]*(?:Compositions/Slotted|_bunker_)[^"]*" \{\r?\n   ID "[0-9A-F]+"\r?\n   components \{\r?\n)    Hierarchy \{\r?\n    \}\r?\n'
foreach ($file in Get-ChildItem $folder -Filter 'AROR_Defence_*.et') {
 if ($file.Name -in @('AROR_Defence_Fortress_Vostok.et','AROR_Defence_Stronghold_Delaware.et')) { continue }
 $text = [IO.File]::ReadAllText($file.FullName)
 $count = [regex]::Matches($text,$duplicateHierarchy).Count
 if (!$count) { continue }
 if (!$Apply) { throw ("{0}: {1} composite child(ren) repeat Hierarchy. Run with -Apply." -f $file.Name,$count) }
 [IO.File]::WriteAllText($file.FullName,[regex]::Replace($text,$duplicateHierarchy,'$1'),$utf8)
 Write-Output ("{0}: removed {1} duplicate Hierarchy component(s)" -f $file.Name,$count)
}
# Composition, number of towers (1: north gun; 2: north gun and east flank gun)
$sites = @(
 @('Outpost_CompactFortified',1),
 @('Outpost_CheckpointFortified',1),
 @('MilitaryOutpost_CompactFortified',2),
 @('MilitaryOutpost_CoastalFortified',2),
 @('MilitaryOutpost_Fortified',2),
 @('MilitaryOutpost',2),
 @('FactoryCompact',2),
 @('Resources',2),
 @('ResourcesCompact',2))

# Direct children of the composition root: header line, extent, name, prefab, position and marker data
function Get-Children([string[]]$lines) {
 $children = @()
 for ($n = 0; $n -lt $lines.Count; $n++) {
  if ($lines[$n] -notmatch '^  GenericEntity ?(\w*)(?: : "([^"]+)")? \{$') { continue }
  $child = [pscustomobject]@{ Start=$n; End=$n; Name=$Matches[1]; Prefab=$Matches[2]; X=0.0; Z=0.0; Yaw=0.0; Id=''; Kind=-1; Ordinal=-1; Elevated=$false }
  for ($m = $n + 1; $m -lt $lines.Count; $m++) {
   $line = $lines[$m]
   if ($line -match '^   coords (\S+) \S+ (\S+)') { $child.X = [double]::Parse($Matches[1],$culture); $child.Z = [double]::Parse($Matches[2],$culture) }
   if ($line -match '^   angles \S+ (\S+) \S+') { $child.Yaw = [double]::Parse($Matches[1],$culture) }
   if ($line -match '^   ID "([0-9A-F]+)"') { $child.Id = $Matches[1] }
   if ($line -match '^     m_iKind (\d+)') { $child.Kind = [int]$Matches[1] }
   if ($line -match '^     m_iOrdinal (\d+)') { $child.Ordinal = [int]$Matches[1] }
   if ($line -match '^     m_fElevation ') { $child.Elevated = $true }
   if ($line -eq '  }') { $child.End = $m; break }
  }
  $children += $child
 }
 return $children
}
function Distance($a, $x, $z) { [Math]::Sqrt(($a.X - $x) * ($a.X - $x) + ($a.Z - $z) * ($a.Z - $z)) }
# Estimated horizontal half-sizes, as in tools/Test-FortifiedOutpostLayouts.js
$sizes = @{ 'Sandbag_01_long_burlap' = @(1.3,0.45); 'Sandbag_01_long_high_burlap' = @(1.3,0.5); 'Sandbag_01_bunker_burlap_camonet' = @(5,5); 'GuardTower_S_US_01' = @(1.75,1.75) }
function Envelope([string]$prefab, $yaw) {
 $size = $sizes[($prefab -replace '.*/','' -replace '\.et$','')]
 if (!$size) { $size = @(1,1) }
 $r = $yaw * [Math]::PI / 180
 @(([Math]::Abs([Math]::Cos($r)) * $size[0] + [Math]::Abs([Math]::Sin($r)) * $size[1]), ([Math]::Abs([Math]::Sin($r)) * $size[0] + [Math]::Abs([Math]::Cos($r)) * $size[1]))
}
# A prop clears a marker's spawn box at any root yaw (same rule as the layout test); raised guns stand on their tower
function ClearsMarkers([string]$prefab, $x, $z, $yaw, $markers) {
 $e = Envelope $prefab $yaw
 $pads = @{ 0 = 2.5; 1 = 1.5; 2 = 5; 3 = 5; 4 = 0.6 }
 foreach ($m in $markers) {
  $pad = $pads[$m.Kind] * [Math]::Sqrt(2)
  if ([Math]::Abs($m.X - $x) -lt $e[0] + $pad + 0.2 -and [Math]::Abs($m.Z - $z) -lt $e[1] + $pad + 0.2) { return $false }
 }
 return $true
}
# A prop stays out of every ground gun's 20 m outward rays (centre and 15 degrees either side)
function ClearsGunRays([string]$prefab, $x, $z, $yaw, $guns) {
 $e = Envelope $prefab $yaw
 foreach ($g in $guns) {
  foreach ($delta in @(-15,0,15)) {
   $angle = ($g.Yaw + $delta) * [Math]::PI / 180
   for ($t = 0.25; $t -le 20; $t += 0.25) {
    if ([Math]::Abs($g.X + [Math]::Sin($angle) * $t - $x) -lt $e[0] + 0.15 -and [Math]::Abs($g.Z + [Math]::Cos($angle) * $t - $z) -lt $e[1] + 0.15) { return $false }
   }
  }
 }
 return $true
}
# Two props do not overlap (with a metre to walk between them)
function ClearsProps([string]$prefab, $x, $z, $yaw, $props) {
 $e = Envelope $prefab $yaw
 foreach ($p in $props) {
  $f = Envelope $p.Prefab $p.Yaw
  if ([Math]::Abs($p.X - $x) -lt $e[0] + $f[0] + 1 -and [Math]::Abs($p.Z - $z) -lt $e[1] + $f[1] + 1) { return $false }
 }
 return $true
}
function ChildLines([string]$entity, [string]$prefab, $x, $z, $yaw, [string[]]$extra = @()) {
 $out = @(('  GenericEntity {0} : "{1}" {{' -f $entity,$prefab), ('   ID "{0}"' -f (EntityId $entity)), '   components {')
 # Composite prefabs (slotted compositions, the sandbag bunker) bring their own Hierarchy; a second one is rejected
 if ($prefab -notmatch $composite) { $out += @('    Hierarchy {', '    }') }
 $out += @('    AROR_DefenceWallComponent {', '    }') + $extra + @('   }', ('   coords {0} 0 {1}' -f (Number $x),(Number $z)))
 if ($yaw -ne 0) { $out += ('   angles 0 {0} 0' -f (Number $yaw)) }
 return $out + '  }'
}

$removedIds = New-Object 'Collections.Generic.HashSet[string]'
$missing = @()
foreach ($site in $sites) {
 $file = Join-Path $folder ("AROR_Defence_{0}.et" -f $site[0])
 $lines = [IO.File]::ReadAllLines($file)
 $children = Get-Children $lines
 if (@($children | Where-Object { $_.Name -match '_Fort_' }).Count -gt 0) { continue }
 if (!$Apply) { $missing += $site[0]; continue }
 $prefix = ($children | Where-Object { $_.Name -match '^AROR_\w+_Defence_\d+_\d+$' } | Select-Object -First 1).Name -replace '_Defence_\d+_\d+$',''
 $half = [double]([regex]::Match(($lines -join "`n"),'m_fDefenceHalfExtent (\S+)').Groups[1].Value)
 $guns = @($children | Where-Object { $_.Kind -eq 1 })
 $pads = @($children | Where-Object { $_.Kind -ge 1 -and $_.Kind -le 3 })
 $teams = @($children | Where-Object { $_.Kind -eq 0 })
 $markers = @($children | Where-Object { $_.Kind -ge 0 })
 $sandbags = @($children | Where-Object { $_.Prefab -match 'Sandbag' })

 # Towers beside the north-wall gun (largest z near the centre line), then the east flank gun (largest x)
 $anchors = @($guns | Sort-Object { -$_.Z + [Math]::Abs($_.X) } | Select-Object -First 1)
 if ($site[1] -gt 1) { $anchors += @($guns | Where-Object { $anchors -notcontains $_ } | Sort-Object { -$_.X } | Select-Object -First 1) }
 $drop = @{}

 $added = New-Object 'Collections.Generic.List[string]'
 # Props already in the layout plus everything added here, for overlap checks
 $props = New-Object 'Collections.Generic.List[object]'
 foreach ($p in $children) { if ($p.Prefab) { $props.Add($p) } }
 $groundGuns = $guns
 $towerIndex = 0
 foreach ($gun in $anchors) {
  # Nearest clear spot beside or behind the gun pit: sideways along the wall first, then back into the compound
  $angle = $gun.Yaw * [Math]::PI / 180
  $forward = @([Math]::Sin($angle), [Math]::Cos($angle)); $right = @([Math]::Cos($angle), -[Math]::Sin($angle))
  $spot = $null
  foreach ($offset in @(@(6,0),@(-6,0),@(7,0),@(-7,0),@(8,0),@(-8,0),@(6,-3),@(-6,-3),@(0,-6),@(7,-4),@(-7,-4),@(0,-8),@(9,-2),@(-9,-2))) {
   $x = [Math]::Round($gun.X + $right[0] * $offset[0] + $forward[0] * $offset[1], 2)
   $z = [Math]::Round($gun.Z + $right[1] * $offset[0] + $forward[1] * $offset[1], 2)
   $e = Envelope $tower $gun.Yaw
   if ([Math]::Abs($x) + $e[0] -gt $half - 1.5 -or [Math]::Abs($z) + $e[1] -gt $half - 1.5) { continue }
   if (!(ClearsMarkers $tower $x $z $gun.Yaw $markers) -or !(ClearsProps $tower $x $z $gun.Yaw $props) -or !(ClearsGunRays $tower $x $z $gun.Yaw $groundGuns)) { continue }
   $spot = @($x,$z); break
  }
  if (!$spot) { Write-Warning ("{0}: no room for a tower beside gun {1}" -f $site[0],$gun.Ordinal); continue }
  $post = @('    AROR_SentryPostComponent {',('     m_fFloorHeight {0}' -f (Number $floorHeight)),'    }')
  foreach ($line in (ChildLines ("{0}_Fort_Tower_{1}" -f $prefix,$towerIndex) $tower $spot[0] $spot[1] $gun.Yaw $post)) { $added.Add($line) }
  $props.Add([pscustomobject]@{ X=$spot[0]; Z=$spot[1]; Yaw=$gun.Yaw; Prefab=$tower })
  $towerIndex++
 }
 # Fighting positions in front of each team, as close as the team's spawn room allows (5-8 m), inside the walls and
 # clear of the other positions, the gun lanes and every prop
 foreach ($team in $teams) {
  $angle = $team.Yaw * [Math]::PI / 180
  foreach ($reach in @(5,6,7,8)) {
   $x = [Math]::Round($team.X + [Math]::Sin($angle) * $reach, 2); $z = [Math]::Round($team.Z + [Math]::Cos($angle) * $reach, 2)
   $e = Envelope $low $team.Yaw
   if ([Math]::Abs($x) + $e[0] -gt $half - 1.5 -or [Math]::Abs($z) + $e[1] -gt $half - 1.5) { break }
   if (!(ClearsMarkers $low $x $z $team.Yaw $markers) -or !(ClearsProps $low $x $z $team.Yaw $props) -or !(ClearsGunRays $low $x $z $team.Yaw $groundGuns)) { continue }
   foreach ($line in (ChildLines ("{0}_Fort_Position_{1}" -f $prefix,$team.Ordinal) $low $x $z $team.Yaw)) { $added.Add($line) }
   $props.Add([pscustomobject]@{ X=$x; Z=$z; Yaw=$team.Yaw; Prefab=$low })
   break
  }
 }
 # A bunker where there is none: the open spot nearest the middle of the rear (south) half
 if (@($sandbags | Where-Object { $_.Prefab -match 'bunker' }).Count -eq 0) {
  $best = $null; $bestScore = [double]::MaxValue
  for ($x = -($half - 6); $x -le $half - 6; $x++) {
   for ($z = -($half - 6); $z -le $half - 6; $z++) {
    if ([Math]::Abs($x) -lt 7 -and [Math]::Abs($z) -lt 7) { continue } # Clear of the flag and its interaction area
    $score = [Math]::Abs($x) + [Math]::Abs($z + $half / 2)
    if ($score -ge $bestScore) { continue }
    if (!(ClearsMarkers $bunker $x $z 0 $markers) -or !(ClearsProps $bunker $x $z 0 $props) -or !(ClearsGunRays $bunker $x $z 0 $groundGuns)) { continue }
    $best = @($x,$z); $bestScore = $score
   }
  }
  if ($best) {
   foreach ($line in (ChildLines ("{0}_Fort_Bunker" -f $prefix) $bunker $best[0] $best[1] 0)) { $added.Add($line) }
  } else { Write-Warning ("{0}: no open spot for a bunker" -f $site[0]) }
 }

 # Existing bunkers face out from the compound and become ground-level sentry posts (same entity ID)
 $replace = @{}
 $bunkers = 0
 foreach ($b in @($children | Where-Object { $_.Prefab -match '_bunker_' })) {
  # Nearest quarter turn: a square bunker turned diagonally needs about 40% more room around it
  $outward = [Math]::Round([Math]::Atan2($b.X, $b.Z) * 2 / [Math]::PI) * 90
  if ($outward -lt 0) { $outward += 360 }
  $block = @(('  GenericEntity {0} : "{1}" {{' -f $b.Name,$b.Prefab), ('   ID "{0}"' -f $b.Id), '   components {', '    AROR_DefenceWallComponent {', '    }', '    AROR_SentryPostComponent {', '     m_fFloorHeight 0', '    }', '   }', ('   coords {0} 0 {1}' -f (Number $b.X),(Number $b.Z)))
  if ($outward -ne 0) { $block += ('   angles 0 {0} 0' -f (Number $outward)) }
  $replace[$b.Start] = @($b.End, ($block + '  }'))
  $bunkers++
 }

 # Rewrite: posted bunkers replaced in place, additions before the root's closing child brace
 $out = New-Object 'Collections.Generic.List[string]'
 $closing = -1
 for ($n = $lines.Count - 1; $n -ge 0; $n--) { if ($lines[$n] -eq ' }') { $closing = $n; break } }
 $skipUntil = -1
 for ($n = 0; $n -lt $lines.Count; $n++) {
  if ($n -eq $closing) { foreach ($line in $added) { $out.Add($line) } }
  if ($replace.ContainsKey($n)) { foreach ($line in $replace[$n][1]) { $out.Add($line) }; $skipUntil = $replace[$n][0] }
  if ($n -le $skipUntil) { continue }
  $out.Add($lines[$n])
 }
 [IO.File]::WriteAllText($file,(($out -join "`r`n") + "`r`n"),$utf8)
 Write-Output ("{0}: {1} sentry tower(s), {2} posted bunker(s), {3} line(s) added" -f $site[0],$towerIndex,$bunkers,$added.Count)
}

if (!$Apply) {
 if ($missing.Count) { throw ("Fortifications missing: " + ($missing -join ', ') + ". Run with -Apply.") }
 Write-Output 'Outpost fortifications are present. Native tower heights, clearance and grounding remain Workbench checks.'
 return
}

# Chicken layer overrides of removed traverses: anonymous "GenericEntity {" child blocks whose ID was cut
if ($removedIds.Count) {
 $layer = [IO.File]::ReadAllLines($layerPath)
 $keep = New-Object 'Collections.Generic.List[string]'
 $orphans = 0
 for ($n = 0; $n -lt $layer.Count; $n++) {
  if ($layer[$n] -match '^(\s+)GenericEntity \{$' -and $n + 1 -lt $layer.Count -and $layer[$n + 1] -match '^\s+ID "([0-9A-F]+)"' -and $removedIds.Contains($Matches[1])) {
   $indent = ([regex]::Match($layer[$n],'^\s+')).Value
   while ($n -lt $layer.Count -and $layer[$n] -ne ($indent + '}')) { $n++ }
   $orphans++
   continue
  }
  $keep.Add($layer[$n])
 }
 $newline = "`n"; if ([IO.File]::ReadAllText($layerPath).Contains("`r`n")) { $newline = "`r`n" }
 [IO.File]::WriteAllText($layerPath,(($keep -join $newline) + $newline),$utf8)
 Write-Output ("MilitaryInstallations.layer: {0} overrides of removed pieces" -f $orphans)
}
