# Read-only validation shared by the WCS launcher and its offline tests.
function Read-ARORAddonProject {
	param([Parameter(Mandatory = $true)][string]$Path)
	$resolved = (Get-Item -LiteralPath $Path -ErrorAction Stop).FullName
	$text = [IO.File]::ReadAllText($resolved)
	$guidMatch = [regex]::Match($text, '(?m)^\s*GUID\s+"([0-9A-Fa-f]{16})"')
	$idMatch = [regex]::Match($text, '(?m)^\s*ID\s+"([^"]+)"')
	if (!$guidMatch.Success -or !$idMatch.Success) { throw "Invalid addon project: $resolved" }
	$dependencyBlock = [regex]::Match($text, '\bDependencies\s*\{([^{}]*)\}')
	$dependencies = @()
	if ($dependencyBlock.Success) {
		$dependencies = @([regex]::Matches($dependencyBlock.Groups[1].Value, '"([0-9A-Fa-f]{16})"') |
			ForEach-Object { $_.Groups[1].Value.ToUpperInvariant() } | Select-Object -Unique)
	}
	[pscustomobject]@{
		Guid = $guidMatch.Groups[1].Value.ToUpperInvariant()
		Id = $idMatch.Groups[1].Value
		Path = $resolved
		Dependencies = $dependencies
	}
}

function Resolve-ARORAddonDependencies {
	param(
		[Parameter(Mandatory = $true)][string]$RootProject,
		[string[]]$SearchRoots = @(),
		[string[]]$ExplicitProjects = @()
	)
	$projects = @{}
	foreach ($root in $SearchRoots) {
		if (!(Test-Path -LiteralPath $root -PathType Container)) { throw "Addon search root not found: $root" }
		foreach ($folder in (Get-ChildItem -LiteralPath $root -Directory)) {
			foreach ($file in (Get-ChildItem -LiteralPath $folder.FullName -File -Filter '*.gproj')) {
				try { $project = Read-ARORAddonProject -Path $file.FullName }
				catch { continue } # Unrelated broken downloads are not part of this dependency closure.
				if (!$projects.ContainsKey($project.Guid)) { $projects[$project.Guid] = @() }
				$projects[$project.Guid] += $project
			}
		}
	}

	# Repository/base-game projects take precedence over downloaded copies of the same GUID.
	foreach ($path in @($ExplicitProjects) + @($RootProject)) {
		$project = Read-ARORAddonProject -Path $path
		$projects[$project.Guid] = @($project)
	}
	$rootInfo = Read-ARORAddonProject -Path $RootProject
	$states = @{}
	$ordered = New-Object 'System.Collections.Generic.List[object]'
	$visit = {
		param([string]$Guid, [string[]]$Chain)
		if ($states[$Guid] -eq 2) { return }
		if ($states[$Guid] -eq 1) { throw "Addon dependency cycle: $(($Chain + $Guid) -join ' -> ')" }
		if (!$projects.ContainsKey($Guid)) { throw "Missing dependency $Guid, required by $(($Chain) -join ' -> ')" }
		$candidates = @($projects[$Guid])
		if ($candidates.Count -ne 1) { throw "Ambiguous dependency $Guid has multiple installed projects: $(($candidates.Path) -join ', ')" }
		$current = $candidates[0]
		$states[$Guid] = 1
		foreach ($dependency in $current.Dependencies) {
			& $visit $dependency @($Chain + $current.Id)
		}
		$states[$Guid] = 2
		$ordered.Add($current)
	}
	& $visit $rootInfo.Guid @()
	$ordered.ToArray()
}
