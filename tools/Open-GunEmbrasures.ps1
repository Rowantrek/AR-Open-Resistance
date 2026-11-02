[CmdletBinding()]
param([switch]$Apply)
# Cuts gun embrasures into every defence composition (Prefabs/Compositions/Locations/AROR_Defence_*.et): removes the
# perimeter sandbag pieces lying in a static gun's firing lane (see GunEmbrasures.ps1), including guns inherited from a
# parent composition, then removes Chicken world-layer overrides that pointed at the removed pieces.
# Without -Apply it lists what would change and fails if anything still needs cutting.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'GunEmbrasures.ps1')
$repoRoot = Split-Path -Parent $PSScriptRoot
$addonRoot = Join-Path $repoRoot 'AROpenResistance/Open Resistance'
$folder = Join-Path $addonRoot 'Prefabs/Compositions/Locations'
$layerPath = Join-Path $addonRoot 'Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/MilitaryInstallations.layer'
$utf8 = New-Object Text.UTF8Encoding($false)
$culture = [Globalization.CultureInfo]::InvariantCulture

# Top-level children of a composition: two-space "GenericEntity" lines up to their closing "  }"
function Get-Children([string[]]$lines) {
 $children = @()
 for ($i = 0; $i -lt $lines.Count; $i++) {
  if ($lines[$i] -notmatch '^  GenericEntity\b') { continue }
  $end = $i + 1
  while ($end -lt $lines.Count -and $lines[$end] -ne '  }') { $end++ }
  $block = $lines[$i..$end]
  $x = 0.0; $z = 0.0; $yaw = 0.0; $id = ''
  foreach ($line in $block) {
   if ($line -match '^   coords (\S+) \S+ (\S+)') { $x = [double]::Parse($Matches[1],$culture); $z = [double]::Parse($Matches[2],$culture) }
   if ($line -match '^   angles \S+ (\S+) \S+') { $yaw = [double]::Parse($Matches[1],$culture) }
   if ($line -match '^   ID "([0-9A-F]+)"') { $id = $Matches[1] }
  }
  $children += [pscustomobject]@{Start=$i;End=$end;Header=$lines[$i];Text=($block -join "`n");X=$x;Z=$z;Yaw=$yaw;Id=$id}
  $i = $end
 }
 return $children
}

function Get-Guns([string]$path, $cache) {
 if ($cache.ContainsKey($path)) { return $cache[$path] }
 $lines = [IO.File]::ReadAllLines($path)
 $guns = @()
 if ($lines[0] -match 'Prefabs/Compositions/Locations/(AROR_Defence_\w+\.et)') { $guns += Get-Guns (Join-Path $folder $Matches[1]) $cache }
 foreach ($child in Get-Children $lines) {
  if ($child.Text -match 'AROR_DefencePositionComponent' -and $child.Text -match 'm_iKind 1\b') { $guns += ,@($child.X,$child.Z,$child.Yaw) }
 }
 $cache[$path] = $guns
 return $guns
}

$cache = @{}
$removedIds = New-Object 'Collections.Generic.HashSet[string]'
$pending = 0
# The stronghold generators cut their own lanes (Delaware's additions sit at a Chicken-specific offset from the parent)
$generated = @('AROR_Defence_Fortress_Vostok.et','AROR_Defence_Stronghold_Delaware.et')
foreach ($file in Get-ChildItem $folder -Filter 'AROR_Defence_*.et') {
 if ($generated -contains $file.Name) { continue }
 $guns = Get-Guns $file.FullName $cache
 if ($guns.Count -eq 0) { continue }
 $lines = [IO.File]::ReadAllLines($file.FullName)
 $cut = @()
 foreach ($child in Get-Children $lines) {
  # Perimeter sandbags only; gun flanks/traverses, gates, guard posts and pad revetments stay
  if ($child.Header -notmatch 'Sandbag_01_long(_high)?_burlap' -or $child.Header -notmatch '_(Perimeter|Wall|OuterWall)_') { continue }
  if (Test-InEmbrasure $child.X $child.Z $guns) { $cut += $child }
 }
 if ($cut.Count -eq 0) { continue }
 $pending += $cut.Count
 Write-Output ("{0}: {1} wall pieces in gun lanes" -f $file.Name,$cut.Count)
 foreach ($child in $cut) { [void]$removedIds.Add($child.Id) }
 if ($Apply) {
  $keep = New-Object 'Collections.Generic.List[string]'
  $skip = @{}
  foreach ($child in $cut) { for ($n = $child.Start; $n -le $child.End; $n++) { $skip[$n] = $true } }
  for ($n = 0; $n -lt $lines.Count; $n++) { if (!$skip.ContainsKey($n)) { $keep.Add($lines[$n]) } }
  [IO.File]::WriteAllText($file.FullName, (($keep -join "`r`n") + "`r`n"), $utf8)
 }
}

# Layer overrides of removed pieces: anonymous "GenericEntity {" child blocks whose ID was cut
$layer = [IO.File]::ReadAllLines($layerPath)
$keepLayer = New-Object 'Collections.Generic.List[string]'
$orphans = 0
for ($n = 0; $n -lt $layer.Count; $n++) {
 if ($layer[$n] -match '^(\s+)GenericEntity \{$' -and $n + 1 -lt $layer.Count -and $layer[$n + 1] -match '^\s+ID "([0-9A-F]+)"' -and $removedIds.Contains($Matches[1])) {
  $indent = ($layer[$n] -replace 'GenericEntity \{$','')
  $end = $n + 1
  while ($end -lt $layer.Count -and $layer[$end] -ne ($indent + '}')) { $end++ }
  $orphans++
  $n = $end
  continue
 }
 $keepLayer.Add($layer[$n])
}
if ($orphans -gt 0) {
 Write-Output ("MilitaryInstallations.layer: {0} overrides of removed pieces" -f $orphans)
 if ($Apply) { [IO.File]::WriteAllText($layerPath, (($keepLayer -join "`r`n") + "`r`n"), $utf8) }
}
if (!$Apply -and ($pending -gt 0 -or $orphans -gt 0)) { throw "Gun lanes are blocked; run with -Apply" }
Write-Output 'Gun lanes are clear of perimeter sandbags. Native fields of fire remain a Play check.'
