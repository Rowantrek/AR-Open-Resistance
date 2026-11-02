[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'CampaignGroundingData.ps1')
& (Join-Path $PSScriptRoot 'Set-EveronCampaignLocations.ps1')
$rows = @(Import-Csv -LiteralPath (Join-Path $repoRoot 'docs/EVERON_CAMPAIGN_LOCATIONS.csv'))
$source = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/third-party/everon-coordinates/source-coordinates.json') -Raw | ConvertFrom-Json
$placements = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/EVERON_FLAG_PLACEMENTS.json') -Raw | ConvertFrom-Json
$editorAudit = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/EVERON_FLAG_EDITOR_AUDIT.json') -Raw | ConvertFrom-Json
$defenceAudit = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/EVERON_DEFENCE_EDITOR_AUDIT.json') -Raw | ConvertFrom-Json
$denseAudit = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/EVERON_DEFENCE_EDITOR_AUDIT_DENSE.json') -Raw | ConvertFrom-Json
$groundedAudit = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/EVERON_DEFENCE_EDITOR_AUDIT_GROUNDED.json') -Raw | ConvertFrom-Json
$followupAudit = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/EVERON_DEFENCE_EDITOR_AUDIT_FOLLOWUP.json') -Raw | ConvertFrom-Json
$wallGrounding = (Get-Content -LiteralPath (Join-Path $repoRoot 'docs/EVERON_DEFENCE_WALL_PLACEMENTS.json') -Raw | ConvertFrom-Json).Sites
$placedAudit = Get-Content -LiteralPath (Join-Path $repoRoot 'docs/EVERON_DEFENCE_EDITOR_AUDIT_PLACED.json') -Raw | ConvertFrom-Json
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
	$expectedRole = 'OCCUPIERS'
	if ($row.Id -eq 'everon_outpost_durras_hill') { $expectedRole = 'INVADERS' }
	if ($row.Role -ne $expectedRole) { throw "Unexpected initial role for $($row.Id): expected $expectedRole" }
	$relocatedStrategic=($row.Type -in @('FACTORY','RESOURCES') -and $row.DefenceAnchored -eq '0' -and $row.DefenceAudit -in @('Original','Dense','Placed','Grounded','Followup'))
	if ($preserved.ContainsKey($row.Id) -and !$relocatedStrategic) {
		foreach ($axis in 0..2) { if ([double]$row.(@('X','Y','Z')[$axis]) -ne [double]$preserved[$row.Id][$axis]) { throw "Established anchor moved: $($row.Id)" } }
	}
	if ($row.DefencePreset) {
		$candidate = $defenceAudit.Candidates.($row.Id)
		if ($row.DefenceAudit -eq 'Dense') { $candidate=$denseAudit.Candidates.($row.Id) }
		if ($row.DefenceAudit -eq 'Grounded') { $candidate=$groundedAudit.Candidates.($row.Id) }
		if ($row.DefenceAudit -eq 'Followup') { $candidate=$followupAudit.Candidates.($row.Id) }
		if ($row.DefenceAudit -eq 'Placed') { $candidate=$placedAudit.Candidates.($row.Id) }
		if ($row.DefenceAudit -eq 'Pending') {
			$point=$placements.Flags.($row.Id)
			foreach($axis in 0..2) { if ([double]$row.(@('FlagX','FlagY','FlagZ')[$axis]) -ne [double]$point[$axis]) { throw "Pending draft moved an unaudited flag: $($row.Id)" } }
			if (!$preserved.ContainsKey($row.Id)) {
				$anchor=$source.($row.Source)
				if ([double]$row.X -ne [double]$anchor[0] -or [double]$row.Z -ne [double]$anchor[1]) { throw "Pending draft moved an unaudited root: $($row.Id)" }
			}
			continue
		}
		$limit=2
		if($row.DefenceMaxRelief){$limit=[double]$row.DefenceMaxRelief}
		if (!$candidate -or !$candidate.FootprintGatePassed -or [double]$candidate.Relief -gt $limit) { throw "Missing measured clearing: $($row.Id)" }
		if ($row.DefenceAnchored -ne '1') { foreach ($axis in 0..2) { if ([double]$row.(@('X','Y','Z')[$axis]) -ne [double]$candidate.Center[$axis]) { throw "Relocation differs from native audit: $($row.Id)" } } }
		if ([double]$row.FlagX -ne [double]$candidate.Center[0] -or [double]$row.FlagZ -ne [double]$candidate.Center[2] -or [Math]::Abs([double]$row.FlagY-[double]$candidate.Center[1]-0.905946) -gt 0.00001 -or [Math]::Abs([double]$row.FlagInteractionY-0.694054) -gt 0.00001) { throw "Relocated pole/model/context does not match measured centre: $($row.Id)" }
		continue # New native centre supersedes historical coordinate/pole evidence only for this adopted site.
	}
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
	$expectedFlags = 0
	if ($layer -eq 'MilitaryInstallations.layer') { $expectedFlags = 23 }
	if ([regex]::Matches($text,'m_bAutoPlace 0').Count -ne $expectedFlags) { throw "Every baked flag must disable random runtime search: $layer" }
	$blocks = [regex]::Split($text,'(?m)(?=^GenericEntity AROR_Location_)') | Where-Object { $_.StartsWith('GenericEntity AROR_Location_') }
	foreach ($block in $blocks) {
		$id = [regex]::Match($block,'m_sId "([^"]+)"').Groups[1].Value
		$row = $rows | Where-Object Id -eq $id
		$reliefOverride = [regex]::Match($block,'m_fMaxDefenceRelief ([\d.]+)')
		if ($row.DefenceMaxRelief) {
			if (!$reliefOverride.Success -or [double]$reliefOverride.Groups[1].Value -ne [double]$row.DefenceMaxRelief) { throw "Missing or wrong garrison terrain allowance: $id" }
		} elseif ($reliefOverride.Success) { throw "Unexpected garrison terrain override: $id" }
		if ($row.Type -in @('TOWN','CITY')) {
			if ($block.Contains('AROR_CaptureFlagPlacementComponent') -or [regex]::Matches($block,'(?m)^\s+coords ').Count -ne 1) { throw "Settlement must have no child flag: $id" }
			continue
		}
		$context = [regex]::Match($block,'Offset 0 ([\d.\-]+) 0')
		if (!$context.Success -or [double]$context.Groups[1].Value -ne [double]$row.FlagInteractionY) { throw "Missing or wrong capture context offset: $id" }
		$coords = [regex]::Matches($block,'(?m)^\s+coords ([\d.\-]+) ([\d.\-]+) ([\d.\-]+)')
		if ($coords.Count -lt 2) { throw "Expected parent and flag-child transform: $id" }
		foreach ($axis in 1..3) {
			$world = [double]$coords[0].Groups[$axis].Value + [double]$coords[1].Groups[$axis].Value
			if ($row.DefencePreset -and $axis -ne 2) {
				$angle = [double]$row.DefenceYaw * [Math]::PI / 180
				$lx=[double]$coords[1].Groups[1].Value; $lz=[double]$coords[1].Groups[3].Value
				if ($axis -eq 1) { $world=[double]$row.X+$lx*[Math]::Cos($angle)+$lz*[Math]::Sin($angle) }
				else { $world=[double]$row.Z-$lx*[Math]::Sin($angle)+$lz*[Math]::Cos($angle) }
			}
			$expected = [double]$row.(@('FlagX','FlagY','FlagZ')[$axis-1])
			if ([Math]::Abs($world-$expected) -gt 0.00001) { throw "Parent plus local flag offset does not reconstruct native world coordinate: $id axis=$axis" }
		}
		if($row.DefencePreset) {
			$prefab=[IO.File]::ReadAllText((Join-Path $addonRoot ('Prefabs/Compositions/Locations/AROR_Defence_'+$row.DefencePreset+'.et')))
			$delta=@([double]$coords[1].Groups[1].Value,([double]$row.FlagY-0.905946-[double]$row.Y),[double]$coords[1].Groups[3].Value)
			$translated=([Math]::Abs($delta[0])+[Math]::Abs($delta[1])+[Math]::Abs($delta[2]) -gt 0.00001)
			$children=[regex]::Matches($prefab,'(?ms)^  GenericEntity AROR_[^\r\n]+\r?\n(.*?)^  \}')
			$overrides=@{}
			foreach($child in [regex]::Matches($block,'(?ms)^  GenericEntity \{\r?\n(.*?)^  \}')) {
				$childId=[regex]::Match($child.Groups[1].Value,'ID "([A-F0-9]+)"').Groups[1].Value
				if($overrides.ContainsKey($childId)){throw "Duplicate inherited child override: $id"}
				$overrides[$childId]=[regex]::Match($child.Groups[1].Value,'(?m)^   coords ([\d.\-]+) ([\d.\-]+) ([\d.\-]+)')
			}
			$groundSite=$wallGrounding.($row.Id)
			$useGround=($groundSite -and $groundSite.Pose -ceq (Get-DefencePoseKey $row))
			$expectedCount=1
			foreach($child in $children){
				$childId=[regex]::Match($child.Groups[1].Value,'ID "([A-F0-9]+)"').Groups[1].Value
				$position=$null
				if($useGround){$position=$groundSite.Walls.$childId}
				if(!$translated -and $null -eq $position){continue}
				$expectedCount++
				$p=[regex]::Match($child.Groups[1].Value,'(?m)^   coords ([\d.\-]+) ([\d.\-]+) ([\d.\-]+)')
				if($null -eq $position){$position=@(1..3 | ForEach-Object {[double]$p.Groups[$_].Value+$delta[$_-1]})}
				if(!$overrides[$childId].Success){throw "Missing wall/marker override: $id child=$childId"}
				foreach($axis in 1..3){if([Math]::Abs([double]$overrides[$childId].Groups[$axis].Value-[double]$position[$axis-1]) -gt 0.00001){throw "Wall/marker differs from saved grounding or flag translation: $id child=$childId"}}
			}
			if($overrides.Count -ne $expectedCount){throw "Missing or stray inherited geometry override: $id"}
		}
	}
	foreach ($ref in [regex]::Matches($text,'"\{([A-F0-9]{16})\}([^"\r\n]+\.et)"')) {
		if (!$ref.Groups[2].Value.StartsWith('Prefabs/Locations/') -and !$ref.Groups[2].Value.StartsWith('Prefabs/Compositions/Locations/')) { continue }
		$meta = [IO.File]::ReadAllText((Join-Path $addonRoot ($ref.Groups[2].Value+'.meta')))
		if (!$meta.Contains('{'+$ref.Groups[1].Value+'}'+$ref.Groups[2].Value)) { throw "Prefab GUID mismatch: $($ref.Value)" }
	}
}
$militaryPrefab = [IO.File]::ReadAllText((Join-Path $addonRoot 'Prefabs/Locations/AROR_Location_MilitaryBase.et'))
if (!$militaryPrefab.Contains('AROR_Prop_CaptureFlag.et')) { throw 'Both outpost classes must inherit the existing capture flag' }
foreach ($name in @('Town_Flag','City_Flag','MilitaryOutpost')) {
	if (Test-Path -LiteralPath (Join-Path $addonRoot "Prefabs/Locations/AROR_Location_$name.et")) { throw "Obsolete location alias must be removed: $name" }
}
foreach ($name in @('Outpost','MilitaryBase')) {
	$prefab = [IO.File]::ReadAllText((Join-Path $addonRoot "Prefabs/Locations/AROR_Location_$name.et"))
	if (!$prefab.Contains('{D1387921EE9626CA}Prefabs/Locations/AROR_Location_Base.et') -or !$prefab.Contains('AROR_Prop_CaptureFlag.et') -or !$prefab.Contains('AROR_GarrisonComponent')) { throw "Military location must directly inherit Location_Base with its own flag/garrison: $name" }
}
$executor = [IO.File]::ReadAllText((Join-Path $addonRoot 'Scripts/Game/Operations/AROR_OperationExecutorComponent.c'))
if ($executor.Contains('GetLocationsOfType(AROR_ELocationType.OUTPOST, outposts)')) { throw 'QRF origin query still excludes military outposts' }
$uses = [regex]::Matches($executor,'AROR_QRFOriginPolicy\.IsOriginType\(').Count
if ($uses -ne 4) { throw "Expected origin gates in readiness, dispatch, commander binding and withdrawal: found $uses" }
$managerLayer = [IO.File]::ReadAllText((Join-Path $addonRoot 'Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/Managers.layer'))
if ($managerLayer -notmatch 'm_bOutpostCaptureQRFs\s+1\b' -or $managerLayer -notmatch 'm_bInvaderOperationsEnabled\s+0\b') { throw 'Expected outpost capture responses enabled while Invader admission remains disabled' }
if ($managerLayer -notmatch 'm_bCombatQRFs\s+0\b') { throw 'Chicken must request automatic QRFs from capture, not combat damage' }
$population = ($rows | Measure-Object -Property Population -Sum).Sum
$reliefOverrides = @($rows | Where-Object DefenceMaxRelief)
if (@($rows | Where-Object DefenceAudit -EQ 'Grounded').Count -ne 3 -or @($rows | Where-Object DefenceAudit -EQ 'Followup').Count -ne 1 -or @($rows | Where-Object DefenceAudit -EQ 'Pending').Count -ne 0 -or @($rows | Where-Object DefenceAudit -EQ 'Placed').Count -ne 3 -or @($rows | Where-Object DefenceAudit -EQ 'Dense').Count -ne 14 -or @($rows | Where-Object DefenceAudit -EQ 'Original').Count -ne 2) { throw 'Expected all 23 military bindings to have measured adoption evidence' }
if (@($rows | Where-Object DefenceAnchored -EQ '1').Count -ne 1 -or ($rows | Where-Object DefenceAnchored -EQ '1').Id -ne 'everon_airport') { throw 'Only the airport remains fixed after authorised factory/resource relocation' }
if ($reliefOverrides.Count -ne 1 -or $reliefOverrides[0].Id -ne 'everon_military_outpost_levie' -or [double]$reliefOverrides[0].DefenceMaxRelief -ne 3) { throw 'Only Levie military outpost should use the measured-site 3m allowance' }
if ($rows.Id -contains 'everon_military_outpost_airport') { throw 'Duplicate airport outpost must be replaced by Rowan airport' }
$flag = [IO.File]::ReadAllText((Join-Path $addonRoot 'Prefabs/Props/Locations/AROR_Prop_CaptureFlag.et'))
if (!$flag.Contains('AROR_CaptureFlagPlacementComponent')) { throw 'Capture poles must ground their own world transform' }
Write-Output "Verified 41 locations, coordinate provenance, fixed airport and authorised Chicken factory/resource bindings, prefab GUIDs, capture inheritance and 4 QRF origin gates. Provisional population: $population. Native flag access, launch clearance and routes remain unverified."
