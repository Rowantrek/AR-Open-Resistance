[CmdletBinding()]
param([string]$Revision = '')	# e.g. HEAD: check a committed version instead of the working tree
# Static checks of each scenario's Operations executor settings, run before Workbench:
#  - every value the tactical commander validator range-checks (TacticalSettingsValid) is inside its range, taking the
#    attribute default, then the game mode prefab, then the scenario layer (a failed range silently disables every
#    QRF and town wave in that scenario);
#  - every force profile a ground recipe names exists in the same layer;
#  - town wave recipes cover waves 1-3 for each army and Readiness 1-5, and each army has a helicopter assault recipe.
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$addonRoot = Join-Path $repoRoot 'AROpenResistance/Open Resistance'
function Read-RepoFile([string]$path) {
	if (!$Revision) { return [IO.File]::ReadAllText($path) }
	$relative = $path.Substring($repoRoot.Length + 1) -replace '\\', '/'
	return (& git -C $repoRoot show "$($Revision):$relative" | Out-String)
}
$executorPath = Join-Path $addonRoot 'Scripts/Game/Operations/AROR_OperationExecutorComponent.c'
$executor = Read-RepoFile $executorPath
$failures = New-Object System.Collections.Generic.List[string]

# Attribute defaults of the executor and its modded extensions
$defaults = @{}
foreach ($file in Get-ChildItem (Join-Path $addonRoot 'Scripts/Game/Operations') -Filter '*.c') {
	$text = Read-RepoFile $file.FullName
	if ($text -notmatch 'class AROR_OperationExecutorComponent\b') { continue }
	foreach ($m in [regex]::Matches($text, '\[Attribute\("(-?[\d.]+)"[^\]]*\]\s*(?:protected\s+)?(?:int|float|bool)\s+(m_\w+);')) {
		$defaults[$m.Groups[2].Value] = [double]$m.Groups[1].Value
	}
}

# The validator's checks: "m_X < 4 || m_X > 8" style terms
$start = $executor.IndexOf('protected bool TacticalSettingsValid()')
if ($start -lt 0) { throw 'TacticalSettingsValid not found' }
$body = $executor.Substring($start, $executor.IndexOf("`n`t}", $start) - $start)
$checks = @()
foreach ($m in [regex]::Matches($body, 'if \((.+)\)\r?\n\t\t\treturn')) {
	foreach ($term in $m.Groups[1].Value -split '\|\|') {
		$t = [regex]::Match($term.Trim(), '^(m_\w+)\s*(<=|>=|<|>)\s*(-?[\d.]+)$')
		if ($t.Success) { $checks += [pscustomobject]@{ Name = $t.Groups[1].Value; Op = $t.Groups[2].Value; Limit = [double]$t.Groups[3].Value } }
	}
}
if ($checks.Count -lt 40) { throw "Parsed only $($checks.Count) validator terms; the validator format changed" }

# Component block (balanced braces) after its header line
function Get-Block([string]$text, [string]$header) {
	$i = $text.IndexOf($header)
	if ($i -lt 0) { return $null }
	# The block opens at the brace ending the header line (an ID like "{6ABA...}" may come first)
	$open = [regex]::Match($text.Substring($i), '\{\s*\r?\n').Index + $i
	$depth = 0
	for ($j = $open; $j -lt $text.Length; $j++) {
		if ($text[$j] -eq '{') { $depth++ }
		elseif ($text[$j] -eq '}') { $depth--; if ($depth -eq 0) { return $text.Substring($open, $j - $open + 1) } }
	}
	return $null
}
function Get-Values([string]$block) {
	$values = @{}
	if (!$block) { return $values }
	# Top-level members only: nested containers (recipes, profiles) also use m_ names
	$depth = 0
	foreach ($line in $block -split "\r?\n") {
		if ($depth -eq 1) {
			$v = [regex]::Match($line, '^\s+(m_\w+)\s+(-?[\d.]+)\s*$')
			if ($v.Success) { $values[$v.Groups[1].Value] = [double]$v.Groups[2].Value }
		}
		$depth += ([regex]::Matches($line, '\{')).Count - ([regex]::Matches($line, '\}')).Count
	}
	return $values
}

$gameMode = Read-RepoFile ((Join-Path $addonRoot 'Prefabs/GameModes/AROR_GameMode_Base.et'))
$prefabValues = Get-Values (Get-Block $gameMode 'AROR_OperationExecutorComponent')

$layers = Get-ChildItem (Join-Path $addonRoot 'Worlds') -Recurse -Filter 'Managers.layer'
foreach ($layerFile in $layers) {
	$scenario = $layerFile.Directory.Name
	$layer = Read-RepoFile $layerFile.FullName
	$block = Get-Block $layer 'AROR_OperationExecutorComponent'
	if (!$block) { continue }
	$values = @{}
	foreach ($k in $defaults.Keys) { $values[$k] = $defaults[$k] }
	foreach ($k in $prefabValues.Keys) { $values[$k] = $prefabValues[$k] }
	$own = Get-Values $block
	foreach ($k in $own.Keys) { $values[$k] = $own[$k] }

	foreach ($c in $checks) {
		if (!$values.ContainsKey($c.Name)) { continue }
		$v = $values[$c.Name]
		$bad = switch ($c.Op) { '<' { $v -lt $c.Limit } '>' { $v -gt $c.Limit } '<=' { $v -le $c.Limit } '>=' { $v -ge $c.Limit } }
		if ($bad) { $failures.Add("$scenario`: $($c.Name) = $v fails the validator check '$($c.Name) $($c.Op) $($c.Limit)' (executor would be disabled)") }
	}

	# Recipes name existing profiles
	$profileIds = @([regex]::Matches($block, 'AROR_ForceProfile[^{]*\{\s*m_sId "([^"]+)"') | ForEach-Object { $_.Groups[1].Value })
	foreach ($recipe in [regex]::Matches($block, 'AROR_GroundQRFRecipe\s*\{(?:[^{}]|\{[^{}]*\})*\}')) {
		$id = [regex]::Match($recipe.Value, 'm_sId "([^"]+)"').Groups[1].Value
		$list = [regex]::Match($recipe.Value, 'm_aProfileIds\s*\{([^}]*)\}')
		foreach ($p in [regex]::Matches($list.Groups[1].Value, '"([^"]+)"')) {
			if ($profileIds.Count -gt 0 -and $p.Groups[1].Value -notin $profileIds) { $failures.Add("$scenario`: recipe $id names unknown profile $($p.Groups[1].Value)") }
		}
	}

	# Town waves: each army, waves 1-3, every Readiness level, plus the helicopter assault
	if ($block -match 'm_iAssaultWave') {
		$recipes = foreach ($recipe in [regex]::Matches($block, 'AROR_GroundQRFRecipe\s*\{(?:[^{}]|\{[^{}]*\})*\}')) {
			$r = $recipe.Value
			$wave = [regex]::Match($r, 'm_iAssaultWave (\d+)')
			if (!$wave.Success) { continue }
			$min = [regex]::Match($r, 'm_iMinimumReadinessLevel (\d+)'); $max = [regex]::Match($r, 'm_iMaximumReadinessLevel (\d+)')
			[pscustomobject]@{ Role = [regex]::Match($r, 'm_eRole (\d+)').Groups[1].Value; Wave = [int]$wave.Groups[1].Value
				Min = $(if ($min.Success) { [int]$min.Groups[1].Value } else { 1 }); Max = $(if ($max.Success) { [int]$max.Groups[1].Value } else { 5 }) }
		}
		foreach ($role in @('1', '2')) {
			foreach ($wave in 1..3) {
				foreach ($readiness in 1..5) {
					if (!($recipes | Where-Object { $_.Role -eq $role -and $_.Wave -eq $wave -and $_.Min -le $readiness -and $_.Max -ge $readiness })) { $failures.Add("$scenario`: role $role has no wave $wave recipe at Readiness $readiness") }
				}
			}
			if (!($recipes | Where-Object { $_.Role -eq $role -and $_.Wave -eq 9 })) { $failures.Add("$scenario`: role $role has no helicopter assault (wave 9) recipe") }
		}
	}
	Write-Host "$scenario`: $($checks.Count) validator terms, $($own.Count) layer overrides checked"
}

if ($failures.Count) { $failures | ForEach-Object { Write-Host "FAIL $_" -ForegroundColor Red }; throw "$($failures.Count) executor setting failure(s)" }
Write-Host 'Executor settings OK'
