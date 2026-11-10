[CmdletBinding()]
param(
	[string]$Scenario = 'AROR_Everon_Chicken',	# layer folder name under Worlds/AROR_Everon (without _Layers)
	[string]$Revision = '',	# e.g. HEAD: check a committed version instead of the working tree
	[double]$MinSiteSpacing = 600	# military and resource sites closer than this to a town centre are reported, metres
)
# Static checks of a town warfare scenario's sites, run before Workbench:
#  - every town and city has a settlement, a garrison post (m_bSettlementPost 1), town waves, town life and a
#    capture flag child (the flag carries the post site placement);
#  - location ids are unique across the scenario's layers, and the hand-written component ids of town warfare
#    components (garrison, waves, life, perimeter) are not reused between entities;
#  - every kept outpost (an "_outpost_" location that is not a base-game military outpost) carries a perimeter;
#  - military, resource and factory sites keep their distance from town centres, so a site is never fought over as
#    part of a town (ports and the airport are exempt: they sit on the coast by design).
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$addonRoot = Join-Path $repoRoot 'AROpenResistance/Open Resistance'
function Read-RepoFile([string]$path) {
	if (!$Revision) { return [IO.File]::ReadAllText($path) }
	$relative = $path.Substring($repoRoot.Length + 1) -replace '\\', '/'
	return (& git -C $repoRoot show "$($Revision):$relative" | Out-String)
}
$failures = New-Object System.Collections.Generic.List[string]
$warnings = New-Object System.Collections.Generic.List[string]

# Split a layer into entities: a frame is an entity when it directly holds a "components {" block. Each entity keeps
# its full text (children included, so a flag child shows up in its town) and its own coords line.
function Get-Entities([string]$text, [string]$layer) {
	$entities = @()
	$stack = New-Object System.Collections.Generic.List[object]
	foreach ($line in ($text -split "`r?`n")) {
		foreach ($frame in $stack) { [void]$frame.Lines.Add($line) }
		$trim = $line.Trim()
		if ($trim.EndsWith('{')) {
			if ($stack.Count -gt 0) {
				$parent = $stack[$stack.Count - 1]
				if ($trim -eq 'components {') { $parent.IsEntity = $true }
			}
			$stack.Add([pscustomobject]@{ Header = $trim; Lines = (New-Object System.Collections.Generic.List[string]); IsEntity = $false; Coords = $null; Depth = $stack.Count })
			continue
		}
		if ($trim -eq '}') {
			if ($stack.Count -eq 0) { continue }
			$frame = $stack[$stack.Count - 1]
			$stack.RemoveAt($stack.Count - 1)
			if ($frame.IsEntity) {
				$body = $frame.Lines -join "`n"
				$id = [regex]::Match($body, 'm_sId "([^"]+)"')
				$type = [regex]::Match($body, 'm_eType (\w+)')
				$entities += [pscustomobject]@{
					Layer = $layer; Header = $frame.Header; Body = $body; Coords = $frame.Coords
					Id = $(if ($id.Success) { $id.Groups[1].Value } else { '' })
					Type = $(if ($type.Success) { $type.Groups[1].Value } else { '' })
				}
			}
			continue
		}
		if ($stack.Count -gt 0 -and $trim -match '^coords (-?[\d.]+) (-?[\d.]+) (-?[\d.]+)$') {
			$frame = $stack[$stack.Count - 1]
			if (!$frame.Coords) { $frame.Coords = @([double]$Matches[1], [double]$Matches[3]) }
		}
	}
	return $entities
}

$layerDir = Join-Path $addonRoot "Worlds/AROR_Everon/$($Scenario)_Layers"
if (!(Test-Path $layerDir)) { throw "Layer folder not found: $layerDir" }
$all = @()
foreach ($file in Get-ChildItem $layerDir -Filter '*.layer') {
	$all += Get-Entities (Read-RepoFile $file.FullName) $file.Name
}
# A location entity owns its m_sId directly; a nested flag child has none
$locations = @($all | Where-Object { $_.Id -and $_.Type })
$locationIds = @{}
foreach ($l in $locations) {
	if (!$locationIds.ContainsKey($l.Id)) { $locationIds[$l.Id] = $l; continue }
	# The parent and its own body both match when an entity nests a location; only report true duplicates
	if ($locationIds[$l.Id].Body -ne $l.Body -and !$locationIds[$l.Id].Body.Contains($l.Body) -and !$l.Body.Contains($locationIds[$l.Id].Body)) {
		$failures.Add("Location id '$($l.Id)' is used twice ($($locationIds[$l.Id].Layer), $($l.Layer))")
	}
}
$locations = @($locationIds.Values)

# Towns and cities
$towns = @($locations | Where-Object { $_.Type -eq 'TOWN' -or $_.Type -eq 'CITY' })
if ($towns.Count -eq 0) { $failures.Add('No towns or cities found') }
foreach ($t in $towns) {
	$missing = @()
	foreach ($component in 'AROR_SettlementComponent', 'AROR_GarrisonComponent', 'AROR_TownWavesComponent', 'AROR_TownLifeComponent') {
		if ($t.Body -notmatch "\b$component\b") { $missing += $component }
	}
	if ($t.Body -match '\bAROR_GarrisonComponent\b' -and $t.Body -notmatch 'm_bSettlementPost 1') { $missing += 'm_bSettlementPost 1' }
	if ($t.Body -notmatch 'AROR_Prop_CaptureFlag\.et') { $missing += 'capture flag child' }
	if ($missing.Count) { $failures.Add("$($t.Id): missing $($missing -join ', ')") }
	if (!$t.Coords) { $failures.Add("$($t.Id): no coords") }
}

# Hand-written component ids of town warfare components must not repeat between entities (Workbench would merge them).
# Ids a prefab defines are overrides of the prefab's own component and are shared by design.
$prefabIds = @{}
foreach ($file in Get-ChildItem (Join-Path $addonRoot 'Prefabs') -Filter '*.et' -Recurse) {
	foreach ($m in [regex]::Matches([IO.File]::ReadAllText($file.FullName), '"\{([0-9A-F]{16})\}"')) { $prefabIds[$m.Groups[1].Value] = $true }
}
$componentOwners = @{}
foreach ($l in $locations) {
	foreach ($m in [regex]::Matches($l.Body, '(AROR_(?:GarrisonComponent|TownWavesComponent|TownLifeComponent|PerimeterComponent)) "\{([0-9A-F]{16})\}"')) {
		$key = $m.Groups[2].Value
		if ($prefabIds.ContainsKey($key)) { continue }
		if ($componentOwners.ContainsKey($key) -and $componentOwners[$key] -ne $l.Id) {
			$failures.Add("$($m.Groups[1].Value) id {$key} is shared by $($componentOwners[$key]) and $($l.Id)")
		}
		$componentOwners[$key] = $l.Id
	}
}

# Kept outposts carry a perimeter
$outposts = @($locations | Where-Object { $_.Id -match '_outpost_' -and $_.Id -notmatch '_military_outpost_' })
foreach ($o in $outposts) {
	if ($o.Body -notmatch '\bAROR_PerimeterComponent\b') { $failures.Add("$($o.Id): kept outpost has no AROR_PerimeterComponent") }
}

# Site spacing from town centres
$spaced = @($locations | Where-Object { $_.Coords -and ($_.Type -eq 'MILITARY_BASE' -or $_.Type -eq 'RESOURCES' -or $_.Type -eq 'FACTORY') })
foreach ($s in $spaced) {
	foreach ($t in $towns) {
		if (!$t.Coords) { continue }
		$dx = $s.Coords[0] - $t.Coords[0]; $dz = $s.Coords[1] - $t.Coords[1]
		$distance = [math]::Sqrt($dx * $dx + $dz * $dz)
		if ($distance -lt $MinSiteSpacing) { $warnings.Add(("{0} is {1:N0} m from {2} (under {3} m)" -f $s.Id, $distance, $t.Id, $MinSiteSpacing)) }
	}
}

# The capture flag prefab places the post site
$flagPrefab = Read-RepoFile (Join-Path $addonRoot 'Prefabs/Props/Locations/AROR_Prop_CaptureFlag.et')
foreach ($component in 'AROR_CaptureFlagPlacementComponent', 'SCR_FlagComponent') {
	if ($flagPrefab -notmatch "\b$component\b") { $failures.Add("AROR_Prop_CaptureFlag.et: missing $component") }
}

Write-Host ("{0}: {1} towns/cities, {2} kept outposts, {3} military/resource sites" -f $Scenario, $towns.Count, $outposts.Count, $spaced.Count)
foreach ($w in $warnings) { Write-Host "WARN  $w" -ForegroundColor Yellow }
if ($failures.Count) {
	foreach ($f in $failures) { Write-Host "FAIL  $f" -ForegroundColor Red }
	exit 1
}
Write-Host 'All site checks passed.' -ForegroundColor Green
