param(
	[Parameter(Mandatory = $true)]
	[string]$ResourceDatabasePath
)

$ErrorActionPreference = 'Stop'
$repoPath = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -LiteralPath (Join-Path $repoPath 'docs/FORCE_ASSETS.json') -Raw | ConvertFrom-Json
$resourceBytes = [System.IO.File]::ReadAllBytes($ResourceDatabasePath)
$resourceText = [System.Text.Encoding]::GetEncoding(28591).GetString($resourceBytes)
$verified = 0
foreach ($entry in $manifest.resources.PSObject.Properties) {
	$resourceIndex = $resourceText.IndexOf($entry.Name + [char]0)
	if ($resourceIndex -lt 0) { throw "Installed resource missing: $($entry.Name)" }
	$resourceGuid = [System.BitConverter]::ToUInt64($resourceBytes, $resourceIndex + $entry.Name.Length + 7).ToString('X16')
	$actual = '{' + $resourceGuid + '}' + $entry.Name
	if ($actual -cne $entry.Value) { throw "GUID mismatch: $($entry.Name)" }
	$verified++
}

$layerPath = Join-Path $repoPath 'AROpenResistance/Open Resistance/Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/Managers.layer'
$layerText = [System.IO.File]::ReadAllText($layerPath)
$navigation = @{
	'5584F30E67F617AD' = 'worlds/GameMaster/Navmeshes/GM_Eden.nmn'
	'5584F30EEFEE1223' = 'worlds/GameMaster/Navmeshes/GM_Eden_vehicles.nmn'
	'5C8C9B750D124A63' = 'worlds/MP/Navmeshes/LowResEden.nmn'
}
foreach ($componentId in $navigation.Keys) {
	$pattern = 'NavmeshWorldComponent\s+"\{' + $componentId + '\}"\s*\{[\s\S]*?NavmeshFile\s+"([^"]+)"'
	$binding = [regex]::Match($layerText, $pattern)
	$expected = $manifest.resources.PSObject.Properties[$navigation[$componentId]]
	if (!$binding.Success -or !$expected -or $binding.Groups[1].Value -cne $expected.Value) { throw "Missing/incorrect Chicken navigation binding: $componentId" }
}
$profiles = [regex]::Matches($layerText, '(?ms)^\s{4}AROR_ForceProfile\s*\{(.*?)^\s{4}\}')
if ($profiles.Count -ne 8) { throw "Expected eight Chicken profiles, found $($profiles.Count)" }
$profileIds = @()
$airborneCount = 0
$roleFactions = @{'1' = 'AROR_OCCUPIERS'; '2' = 'AROR_INVADERS'}
$assetFactions = @{'1' = 'US'; '2' = 'USSR'}
foreach ($profile in $profiles) {
	$body = $profile.Groups[1].Value
	$roleId = [regex]::Match($body, 'm_eRole\s+(\d+)').Groups[1].Value
	$factionKey = [regex]::Match($body, 'm_sFactionKey\s+"([^"]+)"').Groups[1].Value
	$assetKey = [regex]::Match($body, 'm_sAssetFactionKey\s+"([^"]+)"').Groups[1].Value
	if (!$roleFactions.ContainsKey($roleId) -or $factionKey -cne $roleFactions[$roleId] -or $assetKey -cne $assetFactions[$roleId]) { throw 'Chicken profile must match core campaign and native asset factions' }
	$profileId = [regex]::Match($body, 'm_sId\s+"([^"]+)"').Groups[1].Value
	if (!$profileId -or $profileIds -contains $profileId) { throw "Empty/duplicate profile ID: $profileId" }
	$profileIds += $profileId
	if ($body -match 'm_eType\s+3\b') {
		$airborneCount++
		if ($body -notmatch 'm_bEnabled\s+0\b') { throw "Airborne profile must stay disabled until flight adapter exists: $profileId" }
	}
	foreach ($reference in [regex]::Matches($body, '\{[0-9A-F]{16}\}(Prefabs/[^"\r\n]+)')) {
		$manifestEntry = $manifest.resources.PSObject.Properties[$reference.Groups[1].Value]
		if (!$manifestEntry -or $manifestEntry.Value -cne $reference.Value) { throw "Profile resource absent from verified manifest: $($reference.Value)" }
	}
}
if ($airborneCount -ne 2) { throw 'Expected one disabled airborne profile per faction' }
if ($layerText -match 'm_sResistanceFactionKey|m_sOccupiersFactionKey|m_sInvadersFactionKey|m_sAffiliatedFaction\s+"FIA"') { throw 'Chicken must inherit shared role mappings and the Resistance loadout' }
$defenderPath = Join-Path $repoPath 'AROpenResistance/Open Resistance/Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/CivilianLocations.layer'
$defenderText = [IO.File]::ReadAllText($defenderPath)
if ([regex]::Matches($defenderText, 'm_bInitializeFromDefendingRole\s+1').Count -ne 2 -or $defenderText -match 'm_sInitialFactionKey') { throw 'Both Chicken defenders must initialize from the shared defending role' }
$maxGroups = [int][regex]::Match($layerText, 'm_iMaxGroups\s+(\d+)').Groups[1].Value
$maxMembers = [int][regex]::Match($layerText, 'm_iMaxMembers\s+(\d+)').Groups[1].Value
$maxVehicles = [int][regex]::Match($layerText, 'm_iMaxVehicles\s+(\d+)').Groups[1].Value
foreach ($profile in $profiles) {
	$body = $profile.Groups[1].Value
	$type = [int][regex]::Match($body, 'm_eType\s+(\d+)').Groups[1].Value
	# Disabled airborne entries are explicitly selected by the REAPER test adapter.
	if ($type -ne 3 -and $body -match 'm_bEnabled\s+0\b') { continue }
	$groupsPerPackage = 2
	if ($type -eq 2 -or $type -eq 4) { $groupsPerPackage = 1 }
	if ($type -eq 3) {
		$escorts = [int][regex]::Match($body, 'm_iEscortCount\s+(\d+)').Groups[1].Value
		if ($escorts -ne 1) { throw 'REAPER test adapter requires exactly one escort per package' }
		$groupsPerPackage = 3
	}
	$membersPerPackage = [int][regex]::Match($body, 'm_iPersonnelBudget\s+(\d+)').Groups[1].Value
	foreach ($level in 1..5) {
		$vehicles = $level
		if ($type -eq 3) { $vehicles = 2 * $level }
		if ($type -eq 4) { $vehicles = 0 }
		if ($groupsPerPackage * $level -gt $maxGroups -or $membersPerPackage * $level -gt $maxMembers -or $vehicles -gt $maxVehicles) { throw "Prepared profile cannot fit response level $level" }
	}
}
$levels = [regex]::Matches($layerText, '(?ms)^\s{4}AROR_ResponseLevel\s*\{(.*?)^\s{4}\}')
if ($levels.Count -ne 5) { throw 'Expected all five response levels in Chicken' }
$levelIds = @()
foreach ($level in $levels) {
	$levelId = [int][regex]::Match($level.Groups[1].Value, 'm_iLevel\s+(\d+)').Groups[1].Value
	if ($levelId -lt 1 -or $levelId -gt 5 -or $levelIds -contains $levelId) { throw 'Invalid or duplicate response level' }
	$levelIds += $levelId
	foreach ($weight in @('m_iAttackWeight','m_iQRFWeight')) {
		$weightMatch = [regex]::Match($level.Groups[1].Value, $weight + '\s+(\d+)')
		if (!$weightMatch.Success -or [int]$weightMatch.Groups[1].Value -lt 1) { throw "Preset level $levelId must be selectable for both attacks and QRFs" }
	}
}
$recipes = [regex]::Matches($layerText, '(?ms)^\s{4}AROR_GroundQRFRecipe\s*\{(.*?)^\s{4}\}')
if ($recipes.Count -ne 12) { throw 'Expected two fixed truck/IFV recipes plus five tiers for each opposing role' }
$invaderTiers = @()
$occupierTiers = @()
$priceSheet = Import-Csv -LiteralPath (Join-Path $PSScriptRoot '../docs/QRF_PRICES.csv')
$vehiclePrices = @{ us_motorized=200; us_mechanized=800; us_armour=800; us_airborne=1600; ussr_motorized=200; ussr_mechanized=700; ussr_armour=450; ussr_airborne=1800 }
foreach ($price in $vehiclePrices.GetEnumerator()) {
	$pricedProfile = $profiles | Where-Object { $_.Groups[1].Value -match ('m_sId\s+"' + $price.Key + '"') }
	$body = $pricedProfile.Groups[1].Value
	if ($body -notmatch 'm_iPersonnelLossValue\s+25\b' -or $body -notmatch ('m_iVehicleLossValue\s+' + $price.Value + '\b') -or $body -notmatch 'm_iEscortLossValue\s+2000\b') { throw "Incorrect authored price: $($price.Key)" }
}
foreach ($recipe in $recipes) {
	$body = $recipe.Groups[1].Value
	$roleId = [regex]::Match($body, 'm_eRole\s+(\d+)').Groups[1].Value
	$faction = [regex]::Match($body, 'm_sFactionKey\s+"([^"]+)"').Groups[1].Value
	if (!$roleFactions.ContainsKey($roleId) -or $faction -cne $roleFactions[$roleId]) { throw 'Composite recipe faction/role mismatch' }
	$recipeId = [regex]::Match($body, 'm_sId\s+"([^"]+)"').Groups[1].Value
	$tier = [int][regex]::Match($body, 'm_iLevel\s+(\d+)').Groups[1].Value
	if ($recipeId -eq 'combined_arms_l1') {
		if ($tier -ne 1) { throw 'Expected authored combined-arms tier 1' }
	} else {
		if ($tier -lt 1 -or $tier -gt 5) { throw 'Invalid strategic response tier' }
		if ($roleId -eq '2' -and $recipeId -ceq "invader_ground_l$tier") { $invaderTiers += $tier }
		elseif ($roleId -eq '1' -and $recipeId -ceq "occupier_ground_l$tier") { $occupierTiers += $tier }
		else { throw 'Invalid strategic recipe role or ID' }
	}
	$idBlock = [regex]::Match($body, '(?s)m_aProfileIds\s*\{(.*?)\}').Groups[1].Value
	$ids = [regex]::Matches($idBlock, '"([^"]+)"')
	if ($ids.Count -lt 1 -or $ids.Count -gt 8) { throw 'Ground recipes require one to eight elements' }
	$groups = 0; $members = 0; $vehicles = 0; $assetCollateral = 0
	$types = @()
	foreach ($id in $ids) {
		$profile = $profiles | Where-Object { $_.Groups[1].Value -match ('m_sId\s+"' + [regex]::Escape($id.Groups[1].Value) + '"') }
		if (@($profile).Count -ne 1) { throw 'Missing or ambiguous composite element profile' }
		$profileBody = $profile.Groups[1].Value
		if ($profileBody -notmatch ('m_eRole\s+' + $roleId + '\b') -or $profileBody -match 'm_bEnabled\s+0\b') { throw 'Composite element unavailable or wrong role' }
		$type = [int][regex]::Match($profileBody, 'm_eType\s+(\d+)').Groups[1].Value
		$types += $type
		if ($type -eq 2) { $groups++ } elseif ($type -eq 0 -or $type -eq 1) { $groups += 2 } else { throw 'Unexpected element in staged ground recipe' }
		$members += [int][regex]::Match($profileBody, 'm_iPersonnelBudget\s+(\d+)').Groups[1].Value
		$assetCollateral += 25 * [int][regex]::Match($profileBody, 'm_iPersonnelBudget\s+(\d+)').Groups[1].Value
		$assetCollateral += [int][regex]::Match($profileBody, 'm_iVehicleLossValue\s+(\d+)').Groups[1].Value
		$vehicles++
	}
	if ($recipeId -eq 'combined_arms_l1') {
		if (($types | Sort-Object) -join ',' -ne '0,1') { throw 'Composite tier 1 must contain motorized and mechanized elements' }
		if ($groups -ne 4 -or $members -ne 18 -or $vehicles -ne 2) { throw 'Tier 1 reservation must be four groups, 18 slots and two vehicles' }
	} else {
		$expected = @('','4,18,2','6,28,3','9,40,5','12,52,7','14,62,8')
		if ("$groups,$members,$vehicles" -cne $expected[$tier]) { throw "Unexpected Invader tier $tier reservation" }
	}
	if ($groups -gt $maxGroups -or $members -gt $maxMembers -or $vehicles -gt $maxVehicles) { throw 'Composite response exceeds executor capacity' }
	if ($recipeId -ne 'combined_arms_l1') {
		$priceRow = @($priceSheet | Where-Object { $_.Recipe -ceq $recipeId -and $_.Role -ceq $faction })
		if ($priceRow.Count -ne 1) { throw "Missing or ambiguous documented price: $recipeId" }
		$fee = 50 + 10 * $tier
		if ([int]$priceRow[0].AssetCollateral -ne $assetCollateral -or [int]$priceRow[0].Overhead -ne $fee -or [int]$priceRow[0].FundsRequired -ne ($assetCollateral + $fee)) { throw "Documented price differs from fixture: $recipeId" }
		if ([int]$priceRow[0].PersonnelSlots -ne $members) { throw "Documented personnel reservation differs: $recipeId" }
	}
}
if ($layerText -notmatch 'm_sGroundQRFRecipeId\s+"combined_arms_l1"' -or $layerText -notmatch 'm_bReaperQRFTest\s+0\b') { throw 'Chicken must dispatch the ground recipe instead of the helicopter override' }
if (($invaderTiers | Sort-Object -Unique) -join ',' -ne '1,2,3,4,5') { throw 'Staged Invader tiers must cover levels 1-5' }
if (($occupierTiers | Sort-Object -Unique) -join ',' -ne '1,2,3,4,5') { throw 'Occupier tiers must cover levels 1-5' }
foreach ($setting in @('m_bQRFStrategicEconomy','m_bQRFSpotting')) {
	if ($layerText -notmatch ($setting + '\s+1\b')) { throw "Expected enabled Chicken fixture: $setting" }
}
if ($layerText -notmatch 'm_bInvaderOperationsEnabled\s+0\b' -or $layerText -notmatch 'm_sInvadersGroundQRFRecipeId\s+"invader_ground_l1"') { throw 'Invader framework must remain staged with dispatch disabled' }
if ($layerText -notmatch 'm_sInvadersAttackTargetId\s+""' -or $layerText -notmatch 'm_sInvadersQRFTestTargetId\s+""') { throw 'Invader automatic/test targets must remain empty' }
$tacticalFixture = @{
	m_bQRFTacticalCommander = '1'; m_bQRFTwoWaves = '1'; m_iQRFSplitTier = '3'
	m_iQRFReinforcementDelay = '60'; m_iQRFReserveWait = '300'; m_iQRFMaxLiveGround = '6'; m_iQRFMaxLiveAircraft = '6'
	m_iQRFTacticalDuration = '600'; m_iQRFTownHoldDuration = '900'; m_iQRFOrderInterval = '15'; m_iQRFApproachAttempts = '12'; m_fQRFThreatDismount = '200'
	m_fQRFVehicleArrivalTolerance = '20'; m_fQRFObjectiveDeliveryRadius = '100'; m_iQRFIngressTimeout = '600'; m_iQRFExitTimeout = '30'; m_bTownCaptureQRFs = '1'
	m_iQRFInfantryLegTimeout = '180'; m_fQRFInfantryPlanningSpeed = '1.5'
	m_fQRFRecoveryRadius = '100'; m_fQRFVehicleRecoveryRadius = '125'
	m_iStartingWarLevel = '2'; m_iQRFBaseOverhead = '50'; m_iQRFTierOverhead = '10'; m_iQRFThreatLifetime = '180'
	m_fQRFSpotGroundRange = '800'; m_fQRFSpotAirRange = '1500'; m_fQRFSpotMinimumDot = '0.9'; m_iQRFSpotTraceLimit = '8'; m_iQRFSpotLifetime = '90'
	m_fQRFWithdrawFraction = '0.4'; m_fQRFLocalWithdrawFraction = '0.5'; m_iQRFGroundRetention = '120'; m_fQRFGroundCleanupDistance = '1200'
}
foreach ($setting in $tacticalFixture.GetEnumerator()) {
	if ($layerText -notmatch ('(?m)^\s+' + $setting.Key + '\s+' + [regex]::Escape($setting.Value) + '\s*$')) { throw "Unexpected tactical fixture setting: $($setting.Key)" }
}
Write-Output "Verified $verified installed GUIDs, navigation, eight profiles, homogeneous levels, two fixed recipes, five capacity-checked tiers per AI role, strategic/spotting opt-ins and tactical/wave settings; Invader dispatch remains disabled. Configuration checks only, not Enforce compilation or gameplay testing."
