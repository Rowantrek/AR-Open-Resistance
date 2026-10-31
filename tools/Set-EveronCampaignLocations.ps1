[CmdletBinding()]
param([switch]$Apply)

$ErrorActionPreference = 'Stop'

# Fortified trials are authored world overrides, not accepted catalogue replacements.
# Refuse destructive regeneration until they and the manually authored castle are reconciled.
if ($Apply -and (Test-Path -LiteralPath (Join-Path $PSScriptRoot '../docs/FORTIFIED_OUTPOST_TRIALS.json'))) {
	throw 'Chicken has authored fortified outpost trials. Reconcile their prefab bindings, native grounding and saved custom geometry before regenerating the campaign layers.'
}
$repoRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'CampaignGroundingData.ps1')
$catalogPath = Join-Path $repoRoot 'docs/EVERON_CAMPAIGN_LOCATIONS.csv'
$layerRoot = Join-Path $repoRoot 'AROpenResistance/Open Resistance/Worlds/AROR_Everon/AROR_Everon_Chicken_Layers'
$civilianPath = Join-Path $layerRoot 'CivilianLocations.layer'
$militaryPath = Join-Path $layerRoot 'MilitaryInstallations.layer'
$rows = @(Import-Csv -LiteralPath $catalogPath)
$defenceDefinitions = (Get-Content -Encoding utf8 -Raw (Join-Path $repoRoot 'docs/MILITARY_DEFENCE_PRESETS.json') | ConvertFrom-Json).Presets
$defenceEvidence = (Get-Content -Encoding utf8 -Raw (Join-Path $repoRoot 'docs/EVERON_DEFENCE_EDITOR_AUDIT.json') | ConvertFrom-Json).Candidates
$denseEvidence = (Get-Content -Encoding utf8 -Raw (Join-Path $repoRoot 'docs/EVERON_DEFENCE_EDITOR_AUDIT_DENSE.json') | ConvertFrom-Json).Candidates
$placedEvidence = (Get-Content -Encoding utf8 -Raw (Join-Path $repoRoot 'docs/EVERON_DEFENCE_EDITOR_AUDIT_PLACED.json') | ConvertFrom-Json).Candidates
$groundedEvidence = (Get-Content -Encoding utf8 -Raw (Join-Path $repoRoot 'docs/EVERON_DEFENCE_EDITOR_AUDIT_GROUNDED.json') | ConvertFrom-Json).Candidates
$followupEvidence = (Get-Content -Encoding utf8 -Raw (Join-Path $repoRoot 'docs/EVERON_DEFENCE_EDITOR_AUDIT_FOLLOWUP.json') | ConvertFrom-Json).Candidates
$wallGrounding = (Get-Content -Encoding utf8 -Raw (Join-Path $repoRoot 'docs/EVERON_DEFENCE_WALL_PLACEMENTS.json') | ConvertFrom-Json).Sites
$culture = [Globalization.CultureInfo]::InvariantCulture
$ids = @{}
$flagIds = @{
	TOWN = 'A17514DE7AEA4E5C'; CITY = '615E4069714445D5'
	OUTPOST = '6AACA42C3DE4F4A5'; MILITARY_BASE = '6AACA42C3DE4F4A5'
	AIRPORT = '6AACA42BFC76C266'; FACTORY = '6AACA42C2C775A3A'; RESOURCES = '6AACA42C07095538'
}
$templates = @{
	TOWN = @('A744BF492EFD2B54', 'Town', '6AAE72CBD95477C1')
	CITY = @('CCD444F168F300B9', 'City', '6AAE72CBC3214551')
	OUTPOST = @('3D1925CD1FA54115', 'Outpost', '')
	MILITARY_BASE = @('118F89378FB3D213', 'MilitaryBase', '')
	AIRPORT = @('ED619C5A68ED5FAB', 'Airport', '')
	FACTORY = @('357CE0E8EC82AFED', 'Factory', '')
	RESOURCES = @('DFDDE00B63706BEF', 'Resources', '')
}
foreach ($row in $rows) {
	if ($row.Id -notmatch '^everon_[a-z0-9_]+$' -or $ids.ContainsKey($row.Id)) { throw "Invalid/duplicate ID: $($row.Id)" }
	$ids[$row.Id] = $true
	if (!$templates.ContainsKey($row.Type)) { throw "Unknown location type: $($row.Type)" }
	if ($row.DefencePreset) {
		$preset = @($defenceDefinitions | Where-Object { $_.Name -ceq $row.DefencePreset -and $_.Type -ceq $row.Type })
		$measured = $defenceEvidence.($row.Id)
		if ($row.DefenceAudit -eq 'Dense') { $measured=$denseEvidence.($row.Id) }
		if ($row.DefenceAudit -eq 'Placed') { $measured=$placedEvidence.($row.Id) }
		if ($row.DefenceAudit -eq 'Grounded') { $measured=$groundedEvidence.($row.Id) }
		if ($row.DefenceAudit -eq 'Followup') { $measured=$followupEvidence.($row.Id) }
		if ($preset.Count -ne 1 -or $row.DefenceAudit -notin @('Original','Dense','Placed','Grounded','Followup','Pending')) { throw "Invalid defence binding: $($row.Id)" }
		if ($row.DefenceAudit -eq 'Pending') {
			if (!$row.DefencePreset.EndsWith('Compact')) { throw "Unmeasured drafts must select a compact tailored layout: $($row.Id)" }
		} else {
			if (!$measured -or !$measured.FootprintGatePassed -or [double]$preset[0].HalfExtent -ne [double]$measured.HalfExtent) { throw "Preset adoption requires matching native footprint evidence: $($row.Id)" }
			if ([double]$row.FlagX -ne [double]$measured.Center[0] -or [double]$row.FlagZ -ne [double]$measured.Center[2] -or [Math]::Abs([double]$row.FlagY-[double]$measured.Center[1]-0.905946) -gt 0.00001 -or [double]$row.DefenceYaw -ne [double]$measured.Yaw) { throw "Defence centre/yaw differs from audit: $($row.Id)" }
			if ($row.DefenceAnchored -ne '1' -and ([double]$row.X -ne [double]$measured.Center[0] -or [double]$row.Y -ne [double]$measured.Center[1] -or [double]$row.Z -ne [double]$measured.Center[2])) { throw "Adopted root differs from audit: $($row.Id)" }
		}
	}
	if ($row.Role -notin @('OCCUPIERS','INVADERS')) { throw "Unknown initial role: $($row.Role)" }
	if ($row.Name -match '["\r\n]' -or !$row.Name) { throw "Invalid display name: $($row.Id)" }
	if ($row.RegionId -and $row.RegionId -notmatch '^[a-z0-9_]+$') { throw "Invalid stable region ID: $($row.Id)" }
	if ($row.DefenceMaxRelief) {
		$limit = [double]::Parse($row.DefenceMaxRelief, $culture)
		if ($row.Type -in @('TOWN','CITY') -or [double]::IsNaN($limit) -or [double]::IsInfinity($limit) -or $limit -lt 0.25 -or $limit -gt 4) { throw "Invalid military relief override: $($row.Id)" }
	}
	foreach ($field in @('X','Y','Z','Radius','Population','FlagX','FlagY','FlagZ','FlagInteractionY')) {
		$value = [double]::Parse($row.$field, $culture)
		if ([double]::IsNaN($value) -or [double]::IsInfinity($value)) { throw "Nonfinite $field for $($row.Id)" }
	}
	if ([double]$row.X -le 0 -or [double]$row.X -ge 12800 -or [double]$row.Z -le 0 -or [double]$row.Z -ge 12800) { throw "Outside Everon bounds: $($row.Id)" }
	if ([int]$row.Radius -lt 50 -or [int]$row.Radius -gt 500) { throw "Invalid radius: $($row.Id)" }
	if ($row.GroundOnInit -notin @('0','1')) { throw "Invalid terrain-grounding flag: $($row.Id)" }
	if ($row.GroundOnInit -ne '0' -or [double]$row.Y -le 0 -or [double]$row.FlagY -le 0) { throw "Baked location/flag must have an authored above-sea-level height: $($row.Id)" }
	$flagDistance = [Math]::Sqrt([Math]::Pow(([double]$row.FlagX - [double]$row.X),2) + [Math]::Pow(([double]$row.FlagZ - [double]$row.Z),2))
	if ($row.DefenceAnchored -eq '1') {
		if (!$row.DefencePreset -or $flagDistance + ([double]$preset[0].HalfExtent+2)*[Math]::Sqrt(2) -gt [double]$row.Radius) { throw "Anchored defence footprint escapes canonical capture radius: $($row.Id)" }
	} elseif ($flagDistance -gt 60) { throw "Flag outside placement search radius: $($row.Id)" }
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
	$prefabReference = '{{{0}}}Prefabs/Locations/AROR_Location_{1}.et' -f $template[0],$template[1]
	if ($row.DefencePreset) {
		$preset = $defenceDefinitions | Where-Object Name -CEQ $row.DefencePreset
		$prefabReference = '{{{0}}}Prefabs/Compositions/Locations/AROR_Defence_{1}.et' -f $preset.Guid,$preset.Name
	}
	$builder = $military
	if ($row.Type -in @('TOWN','CITY')) { $builder = $civilian }
	# References match checked-in prefab .meta files; new template GUIDs are hand-authored.
	[void]$builder.AppendLine(('GenericEntity AROR_Location_{0} : "{1}" {{' -f $row.Id,$prefabReference))
	[void]$builder.AppendLine(' components {')
	[void]$builder.AppendLine('  AROR_LocationComponent "{6AA28237301A1CF9}" {')
	[void]$builder.AppendLine(('   m_sId "{0}"' -f $row.Id))
	[void]$builder.AppendLine(('   m_sName "{0}"' -f $row.Name))
	[void]$builder.AppendLine(('   m_eType {0}' -f $row.Type))
	[void]$builder.AppendLine(('   m_fRadius {0}' -f $row.Radius))
	[void]$builder.AppendLine(('   m_eInitialControllingRole {0}' -f $row.Role))
	[void]$builder.AppendLine(('   m_bGroundOnInit {0}' -f $row.GroundOnInit))
	if ($row.RegionId) { [void]$builder.AppendLine(('   m_sSupportRegionId "{0}"' -f $row.RegionId)) }
	[void]$builder.AppendLine('  }')
	if ($row.Type -in @('TOWN','CITY')) {
		[void]$builder.AppendLine(('  AROR_SettlementComponent "{{{0}}}" {{' -f $template[2]))
		[void]$builder.AppendLine(('   m_iPopulation {0}' -f $row.Population))
		[void]$builder.AppendLine('  }')
	}
	if ($row.DefenceMaxRelief -or $row.DefenceAnchored -eq '1') {
		[void]$builder.AppendLine('  AROR_GarrisonComponent "{66D30571F98F4BE3}" {')
		if ($row.DefenceMaxRelief) { [void]$builder.AppendLine(('   m_fMaxDefenceRelief {0}' -f $row.DefenceMaxRelief)) }
		if ($row.DefenceAnchored -eq '1') { [void]$builder.AppendLine('   m_bFixedDefenceAnchor 1') }
		[void]$builder.AppendLine('  }')
	}
	[void]$builder.AppendLine(' }')
	[void]$builder.AppendLine((' coords {0} {1} {2}' -f $row.X,$row.Y,$row.Z))
	if ($row.DefencePreset -and [double]$row.DefenceYaw -ne 0) { [void]$builder.AppendLine((' angles 0 {0} 0' -f $row.DefenceYaw)) }
	if ($row.Type -in @('TOWN','CITY')) {
		[void]$builder.AppendLine('}')
		continue # Settlements have no pole or interaction hierarchy.
	}
	# Override the inherited child by its existing ID. These are LOCAL offsets from the
	# saved parent, calculated from engine-reported WORLD coordinates (not guessed heights).
	$offset = @('X','Y','Z') | ForEach-Object { ([double]$row.('Flag'+$_) - [double]$row.$_).ToString('0.######',$culture) }
	if ($row.DefencePreset) {
		$angle = [double]$row.DefenceYaw * [Math]::PI / 180
		$dx = [double]$row.FlagX - [double]$row.X; $dz = [double]$row.FlagZ - [double]$row.Z
		$offset[0] = ($dx*[Math]::Cos($angle)-$dz*[Math]::Sin($angle)).ToString('0.######',$culture)
		$offset[2] = ($dx*[Math]::Sin($angle)+$dz*[Math]::Cos($angle)).ToString('0.######',$culture)
	}
	[void]$builder.AppendLine(' {')
	[void]$builder.AppendLine('  GenericEntity {')
	[void]$builder.AppendLine(('   ID "{0}"' -f $flagIds[$row.Type]))
	[void]$builder.AppendLine('   components {')
	[void]$builder.AppendLine('    AROR_CaptureFlagPlacementComponent "{F9A38254AA9B454D}" {')
	[void]$builder.AppendLine('     m_bAutoPlace 0')
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
	# Shift inherited wall/marker children with the flag while retaining strategic
	# root anchors. One location, one roster, one pole: no nested location prefab.
	if ($row.DefencePreset) {
		$dy=[double]$row.FlagY-0.905946-[double]$row.Y
		$dx=[double]$offset[0]; $dz=[double]$offset[2]
		$groundSite=$wallGrounding.($row.Id)
		$useGround=($groundSite -and $groundSite.Pose -ceq (Get-DefencePoseKey $row))
		$translated=([Math]::Abs($dx)+[Math]::Abs($dy)+[Math]::Abs($dz) -gt 0.00001)
		if ($translated -or $useGround) {
			$prefabPath=Join-Path $repoRoot ('AROpenResistance/Open Resistance/Prefabs/Compositions/Locations/AROR_Defence_'+$row.DefencePreset+'.et')
			foreach ($child in [regex]::Matches([IO.File]::ReadAllText($prefabPath),'(?ms)^  GenericEntity AROR_[^\r\n]+\r?\n(.*?)^  \}')) {
				$childId=[regex]::Match($child.Groups[1].Value,'ID "([A-F0-9]+)"').Groups[1].Value
				$p=[regex]::Match($child.Groups[1].Value,'(?m)^   coords ([\d.\-]+) ([\d.\-]+) ([\d.\-]+)')
				$groundPosition=$null
				if($useGround){$groundPosition=$groundSite.Walls.$childId}
				if(!$translated -and $null -eq $groundPosition){continue}
				$position=@(([double]$p.Groups[1].Value+$dx),([double]$p.Groups[2].Value+$dy),([double]$p.Groups[3].Value+$dz))
				if($null -ne $groundPosition){$position=$groundPosition}
				[void]$builder.AppendLine('  GenericEntity {')
				[void]$builder.AppendLine(('   ID "{0}"' -f $childId))
				[void]$builder.AppendLine(('   coords {0} {1} {2}' -f ([double]$position[0]).ToString('0.######',$culture),([double]$position[1]).ToString('0.######',$culture),([double]$position[2]).ToString('0.######',$culture)))
				[void]$builder.AppendLine('  }')
			}
		}
	}
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
