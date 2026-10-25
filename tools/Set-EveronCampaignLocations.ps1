[CmdletBinding()]
param([switch]$Apply)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$catalogPath = Join-Path $repoRoot 'docs/EVERON_CAMPAIGN_LOCATIONS.csv'
$layerRoot = Join-Path $repoRoot 'AROpenResistance/Open Resistance/Worlds/AROR_Everon/AROR_Everon_Chicken_Layers'
$civilianPath = Join-Path $layerRoot 'CivilianLocations.layer'
$militaryPath = Join-Path $layerRoot 'MilitaryInstallations.layer'
$rows = @(Import-Csv -LiteralPath $catalogPath)
$culture = [Globalization.CultureInfo]::InvariantCulture
$ids = @{}
$flagIds = @{
	TOWN = 'A17514DE7AEA4E5C'; CITY = '615E4069714445D5'
	OUTPOST = '6AACA42C3DE4F4A5'; MILITARY_BASE = '6AACA42C3DE4F4A5'
	AIRPORT = '6AACA42BFC76C266'; FACTORY = '6AACA42C2C775A3A'; RESOURCES = '6AACA42C07095538'
}
$templates = @{
	TOWN = @('FE40DD5F63A34B1E', 'Town_Flag', '6AAE72CBD95477C1')
	CITY = @('DA534E8B5C934410', 'City_Flag', '6AAE72CBC3214551')
	OUTPOST = @('3D1925CD1FA54115', 'Outpost', '')
	MILITARY_BASE = @('9B2CAF507D694BA5', 'MilitaryOutpost', '')
	AIRPORT = @('ED619C5A68ED5FAB', 'Airport', '')
	FACTORY = @('357CE0E8EC82AFED', 'Factory', '')
	RESOURCES = @('DFDDE00B63706BEF', 'Resources', '')
}
foreach ($row in $rows) {
	if ($row.Id -notmatch '^everon_[a-z0-9_]+$' -or $ids.ContainsKey($row.Id)) { throw "Invalid/duplicate ID: $($row.Id)" }
	$ids[$row.Id] = $true
	if (!$templates.ContainsKey($row.Type)) { throw "Unknown location type: $($row.Type)" }
	if ($row.Role -notin @('OCCUPIERS','INVADERS')) { throw "Unknown initial role: $($row.Role)" }
	if ($row.Name -match '["\r\n]' -or !$row.Name) { throw "Invalid display name: $($row.Id)" }
	foreach ($field in @('X','Y','Z','Radius','Population','FlagX','FlagY','FlagZ','FlagInteractionY')) {
		$value = [double]::Parse($row.$field, $culture)
		if ([double]::IsNaN($value) -or [double]::IsInfinity($value)) { throw "Nonfinite $field for $($row.Id)" }
	}
	if ([double]$row.X -le 0 -or [double]$row.X -ge 12800 -or [double]$row.Z -le 0 -or [double]$row.Z -ge 12800) { throw "Outside Everon bounds: $($row.Id)" }
	if ([int]$row.Radius -lt 50 -or [int]$row.Radius -gt 500) { throw "Invalid radius: $($row.Id)" }
	if ($row.GroundOnInit -notin @('0','1')) { throw "Invalid terrain-grounding flag: $($row.Id)" }
	if ($row.GroundOnInit -ne '0' -or [double]$row.Y -le 0 -or [double]$row.FlagY -le 0) { throw "Baked location/flag must have an authored above-sea-level height: $($row.Id)" }
	$flagDistance = [Math]::Sqrt([Math]::Pow(([double]$row.FlagX - [double]$row.X),2) + [Math]::Pow(([double]$row.FlagZ - [double]$row.Z),2))
	if ($flagDistance -gt 60) { throw "Flag outside placement search radius: $($row.Id)" }
	if ($row.Type -in @('OUTPOST','MILITARY_BASE') -and [int]$row.Population -ne 0) { throw "Military location cannot pay settlement income: $($row.Id)" }
	if ($row.Type -in @('TOWN','CITY') -and [int]$row.Population -le 0) { throw "Settlement needs a population: $($row.Id)" }
}

# Preserve the two existing Provins test soldiers verbatim. All generated locations follow them.
$oldCivilian = [IO.File]::ReadAllText($civilianPath)
$firstLocation = [regex]::Match($oldCivilian, '(?m)^GenericEntity(?:\s+\w+)?\s*:')
if (!$firstLocation.Success) { throw 'Cannot identify civilian location boundary; inspect the layer before regeneration.' }
$defenders = $oldCivilian.Substring(0, $firstLocation.Index).TrimEnd()
foreach ($name in @('AROR_Provins_QRFDefender','AROR_Provins_ManualQRFDefender')) {
	if (!$defenders.Contains($name)) { throw "Existing defender prefix missing $name" }
}
$civilian = [Text.StringBuilder]::new()
[void]$civilian.AppendLine($defenders)
$military = [Text.StringBuilder]::new()
foreach ($row in $rows) {
	$template = $templates[$row.Type]
	$builder = $military
	if ($row.Type -in @('TOWN','CITY')) { $builder = $civilian }
	# References match checked-in prefab .meta files; new template GUIDs are hand-authored.
	[void]$builder.AppendLine(('GenericEntity AROR_Location_{0} : "{{{1}}}Prefabs/Locations/AROR_Location_{2}.et" {{' -f $row.Id,$template[0],$template[1]))
	[void]$builder.AppendLine(' components {')
	[void]$builder.AppendLine('  AROR_LocationComponent "{6AA28237301A1CF9}" {')
	[void]$builder.AppendLine(('   m_sId "{0}"' -f $row.Id))
	[void]$builder.AppendLine(('   m_sName "{0}"' -f $row.Name))
	[void]$builder.AppendLine(('   m_eType {0}' -f $row.Type))
	[void]$builder.AppendLine(('   m_fRadius {0}' -f $row.Radius))
	[void]$builder.AppendLine(('   m_eInitialControllingRole {0}' -f $row.Role))
	[void]$builder.AppendLine(('   m_bGroundOnInit {0}' -f $row.GroundOnInit))
	[void]$builder.AppendLine('  }')
	if ($row.Type -in @('TOWN','CITY')) {
		[void]$builder.AppendLine(('  AROR_SettlementComponent "{{{0}}}" {{' -f $template[2]))
		[void]$builder.AppendLine(('   m_iPopulation {0}' -f $row.Population))
		[void]$builder.AppendLine('  }')
	}
	[void]$builder.AppendLine(' }')
	[void]$builder.AppendLine((' coords {0} {1} {2}' -f $row.X,$row.Y,$row.Z))
	# Override the inherited child by its existing ID. These are LOCAL offsets from the
	# saved parent, calculated from engine-reported WORLD coordinates (not guessed heights).
	$offset = @('X','Y','Z') | ForEach-Object { ([double]$row.('Flag'+$_) - [double]$row.$_).ToString('0.######',$culture) }
	[void]$builder.AppendLine(' {')
	[void]$builder.AppendLine('  GenericEntity {')
	[void]$builder.AppendLine(('   ID "{0}"' -f $flagIds[$row.Type]))
	[void]$builder.AppendLine('   components {')
	[void]$builder.AppendLine('    AROR_CaptureFlagPlacementComponent "{F9A38254AA9B454D}" {')
	[void]$builder.AppendLine('     m_bEnabled 0')
	[void]$builder.AppendLine('    }')
	# Raising the model base must not raise the capture context above the player.
	# Preserve its original 1.6-metre world height above physical terrain.
	[void]$builder.AppendLine('    ActionsManagerComponent "{6AAE5E8DE935135E}" {')
	[void]$builder.AppendLine('     ActionContexts {')
	[void]$builder.AppendLine('      UserActionContext "{6AAE5E8DEECDFE8B}" {')
	[void]$builder.AppendLine('       Position PointInfo "{6AAE5E8DE4DBC0C4}" {')
	[void]$builder.AppendLine(('        Offset 0 {0} 0' -f $row.FlagInteractionY))
	[void]$builder.AppendLine('       }')
	[void]$builder.AppendLine('      }')
	[void]$builder.AppendLine('     }')
	[void]$builder.AppendLine('    }')
	[void]$builder.AppendLine('   }')
	[void]$builder.AppendLine(('   coords {0}' -f ($offset -join ' ')))
	[void]$builder.AppendLine('  }')
	[void]$builder.AppendLine(' }')
	[void]$builder.AppendLine('}')
}
$outputs = @(@($civilianPath,$civilian.ToString()),@($militaryPath,$military.ToString()))
foreach ($output in $outputs) {
	if ($Apply) {
		[IO.File]::WriteAllText($output[0],$output[1],[Text.UTF8Encoding]::new($false))
	} else {
		$actual = [IO.File]::ReadAllText($output[0]).Replace("`r`n","`n").TrimEnd()
		$expected = $output[1].Replace("`r`n","`n").TrimEnd()
		if ($actual -cne $expected) { throw "Layer differs from catalogue: $($output[0]). Review local placement edits before using -Apply." }
	}
}
Write-Output "Validated $($rows.Count) Everon campaign locations; $(if ($Apply) {'wrote Chicken civilian/military layers'} else {'generated layers match catalogue'}). Native placement/capture/QRF routes require Workbench verification."
