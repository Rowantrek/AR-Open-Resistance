[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'Set-EveronCampaignLocations.ps1')
$rows = @(Import-Csv -LiteralPath (Join-Path $repoRoot 'docs/EVERON_CAMPAIGN_LOCATIONS.csv'))
$source = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/third-party/everon-coordinates/source-coordinates.json') -Raw | ConvertFrom-Json
$placements = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/EVERON_FLAG_PLACEMENTS.json') -Raw | ConvertFrom-Json
$editorAudit = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/EVERON_FLAG_EDITOR_AUDIT.json') -Raw | ConvertFrom-Json
$addonRoot = Join-Path $repoRoot 'AROpenResistance/Open Resistance'
$counts = @{ CITY = 7; TOWN = 11; OUTPOST = 14; MILITARY_BASE = 5; AIRPORT = 1; FACTORY = 1; RESOURCES = 2 }
foreach ($type in $counts.Keys) {
	if (@($rows | Where-Object Type -eq $type).Count -ne $counts[$type]) { throw "Unexpected $type count" }
}
$preserved = @{
	everon_provins = @(5503.833,96.354,6081.837)
	everon_morton_town = @(5132.015,16.575,4008.557)
	everon_lamentin_town = @(1308.509,38.292,5985.583)
	everon_airport = @(4913.476,28.251,11787.395)
	everon_factory_north_ridge = @(5591.082,205.067,7235.609)
	everon_resources_north_ridge_west = @(5526.961,200.03,7115.338)
	everon_resources_north_ridge_east = @(5568.75,205.11,7071.537)
}
foreach ($row in $rows) {
	if ($row.Role -ne 'OCCUPIERS') { throw "Initial map should remain Occupier controlled: $($row.Id)" }
	if ($preserved.ContainsKey($row.Id)) {
		$old = $preserved[$row.Id]
		if ([double]$row.X -ne $old[0] -or [double]$row.Y -ne $old[1] -or [double]$row.Z -ne $old[2]) { throw "Established placement moved: $($row.Id)" }
		if ($row.GroundOnInit -ne '0') { throw "Established height unexpectedly replaced: $($row.Id)" }
	} else {
		$anchor = $source.($row.Source)
		if (!$anchor -or [double]$row.X -ne $anchor[0] -or [double]$row.Z -ne $anchor[1]) { throw "Coordinate provenance mismatch: $($row.Id)" }
		if ($row.GroundOnInit -ne '0' -or [double]$row.Y -le 0) { throw "2D source needs baked native elevation: $($row.Id)" }
	}
	$point = $placements.Flags.($row.Id)
	if (!$point -or [double]$row.FlagX -ne [double]$point[0] -or [double]$row.FlagY -ne [double]$point[1] -or [double]$row.FlagZ -ne [double]$point[2]) { throw "Flag differs from native placement evidence: $($row.Id)" }
	$sample = $editorAudit.Measurements.($row.Id)
	if (!$sample -or !$sample.Clear) { throw "Missing clear native editor measurement: $($row.Id)" }
	$bottom = [double]$row.FlagY + [double]$sample.BoundsMin[1]
	if ([Math]::Abs($bottom - [double]$sample.Ground - 0.1) -gt 0.00001) { throw "Model base is not 0.1m above measured editor terrain: $($row.Id)" }
	$interaction = [double]$row.FlagY + [double]$row.FlagInteractionY
	if ([Math]::Abs($interaction - [double]$sample.Ground - 1.6) -gt 0.00001) { throw "Capture context moved away from reachable world height: $($row.Id)" }
}
# Verify generated location references against their actual checked-in sibling .meta.
foreach ($layer in @('CivilianLocations.layer','MilitaryInstallations.layer')) {
	$text = [IO.File]::ReadAllText((Join-Path $addonRoot "Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/$layer"))
	$expectedFlags = @($rows | Where-Object { ($_.Type -in @('TOWN','CITY')) -eq ($layer -eq 'CivilianLocations.layer') }).Count
	if ([regex]::Matches($text,'m_bEnabled 0').Count -ne $expectedFlags) { throw "Every baked flag must disable random runtime search: $layer" }
	$blocks = [regex]::Split($text,'(?m)(?=^GenericEntity AROR_Location_)') | Where-Object { $_.StartsWith('GenericEntity AROR_Location_') }
	foreach ($block in $blocks) {
		$id = [regex]::Match($block,'m_sId "([^"]+)"').Groups[1].Value
		$row = $rows | Where-Object Id -eq $id
		$context = [regex]::Match($block,'Offset 0 ([\d.\-]+) 0')
		if (!$context.Success -or [double]$context.Groups[1].Value -ne [double]$row.FlagInteractionY) { throw "Missing or wrong capture context offset: $id" }
		$coords = [regex]::Matches($block,'(?m)^\s+coords ([\d.\-]+) ([\d.\-]+) ([\d.\-]+)')
		if ($coords.Count -ne 2) { throw "Expected parent and flag-child transform: $id" }
		foreach ($axis in 1..3) {
			$world = [double]$coords[0].Groups[$axis].Value + [double]$coords[1].Groups[$axis].Value
			$expected = [double]$row.(@('FlagX','FlagY','FlagZ')[$axis-1])
			if ([Math]::Abs($world-$expected) -gt 0.00001) { throw "Parent plus local flag offset does not reconstruct native world coordinate: $id axis=$axis" }
		}
	}
	foreach ($ref in [regex]::Matches($text,'"\{([A-F0-9]{16})\}([^"\r\n]+\.et)"')) {
		if (!$ref.Groups[2].Value.StartsWith('Prefabs/Locations/')) { continue } # Existing base-game test defenders are unchanged.
		$meta = [IO.File]::ReadAllText((Join-Path $addonRoot ($ref.Groups[2].Value+'.meta')))
		if (!$meta.Contains('{'+$ref.Groups[1].Value+'}'+$ref.Groups[2].Value)) { throw "Prefab GUID mismatch: $($ref.Value)" }
	}
}
$militaryPrefab = [IO.File]::ReadAllText((Join-Path $addonRoot 'Prefabs/Locations/AROR_Location_MilitaryBase.et'))
if (!$militaryPrefab.Contains('AROR_Prop_CaptureFlag.et')) { throw 'Both outpost classes must inherit the existing capture flag' }
foreach ($name in @('Town_Flag','City_Flag')) {
	$prefab = [IO.File]::ReadAllText((Join-Path $addonRoot "Prefabs/Locations/AROR_Location_$name.et"))
	if (!$prefab.Contains('AROR_Prop_CaptureFlag.et') -or $prefab -notmatch 'm_bSettlementFlagCapture\s+1\b') { throw "Missing capturable settlement flag: $name" }
}
foreach ($name in @('Outpost','MilitaryOutpost')) {
	$prefab = [IO.File]::ReadAllText((Join-Path $addonRoot "Prefabs/Locations/AROR_Location_$name.et"))
	if (!$prefab.Contains('{118F89378FB3D213}Prefabs/Locations/AROR_Location_MilitaryBase.et')) { throw "Outpost must inherit the checked capture-flag hierarchy: $name" }
}
$executor = [IO.File]::ReadAllText((Join-Path $addonRoot 'Scripts/Game/Operations/AROR_OperationExecutorComponent.c'))
if ($executor.Contains('GetLocationsOfType(AROR_ELocationType.OUTPOST, outposts)')) { throw 'QRF origin query still excludes military outposts' }
$uses = [regex]::Matches($executor,'AROR_QRFOriginPolicy\.IsOriginType\(').Count
if ($uses -ne 4) { throw "Expected origin gates in readiness, dispatch, commander binding and withdrawal: found $uses" }
$managerLayer = [IO.File]::ReadAllText((Join-Path $addonRoot 'Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/Managers.layer'))
if ($managerLayer -notmatch 'm_bOutpostCaptureQRFs\s+1\b' -or $managerLayer -notmatch 'm_bInvaderOperationsEnabled\s+0\b') { throw 'Expected outpost capture responses enabled while Invader admission remains disabled' }
$population = ($rows | Measure-Object -Property Population -Sum).Sum
if ($rows.Id -contains 'everon_military_outpost_airport') { throw 'Duplicate airport outpost must be replaced by Rowan airport' }
$flag = [IO.File]::ReadAllText((Join-Path $addonRoot 'Prefabs/Props/Locations/AROR_Prop_CaptureFlag.et'))
if (!$flag.Contains('AROR_CaptureFlagPlacementComponent')) { throw 'Capture poles must ground their own world transform' }
Write-Output "Verified 41 locations, coordinate provenance, original anchors plus Rowan airport/factory/resources, prefab GUIDs, capture inheritance and 4 QRF origin gates. Provisional population: $population. Native flag access, launch clearance and routes remain unverified."
