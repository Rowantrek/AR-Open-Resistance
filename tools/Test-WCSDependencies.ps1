$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'WCSDependencyGraph.ps1')
$script:assertions = 0
function Assert-ARORCheck([bool]$Condition, [string]$Message) {
	$script:assertions++
	if (!$Condition) { throw $Message }
}
function Assert-ARORFailure([scriptblock]$Action, [string]$Expected) {
	$failed = $false
	try { & $Action | Out-Null }
	catch { $failed = $_.Exception.Message.Contains($Expected) }
	Assert-ARORCheck $failed "Expected failure containing '$Expected'"
}
function Write-ARORFixture([string]$Path, [string]$Id, [string]$Guid, [string[]]$Dependencies = @()) {
	[IO.Directory]::CreateDirectory((Split-Path -Parent $Path)) | Out-Null
	$lines = @('GameProject {', " ID `"$Id`"", " GUID `"$Guid`"", ' Dependencies {')
	foreach ($dependency in $Dependencies) { $lines += "  `"$dependency`"" }
	$lines += @(' }', '}')
	[IO.File]::WriteAllLines($Path, $lines)
}

$testRoot = Join-Path $env:TEMP ('AROR-WCSDependencyTests-' + [Guid]::NewGuid().ToString('N'))
try {
	$downloads = Join-Path $testRoot 'downloads'
	$root = Join-Path $testRoot 'local/addon.gproj'
	$first = Join-Path $downloads 'arbitrary-folder-name/addon.gproj'
	$second = Join-Path $downloads 'Second_0000000000000002/addon.gproj'
	$rootGuid = 'FFFFFFFFFFFFFFFF'
	$firstGuid = '0000000000000001'
	$secondGuid = '0000000000000002'
	Write-ARORFixture $root 'TestRoot' $rootGuid @($firstGuid)
	Write-ARORFixture $first 'First' $firstGuid @($secondGuid)
	Write-ARORFixture $second 'Second' $secondGuid
	$projects = @(Resolve-ARORAddonDependencies -RootProject $root -SearchRoots @($downloads))
	Assert-ARORCheck ($projects.Count -eq 3) 'Must traverse root plus both dependency levels'
	Assert-ARORCheck ($projects[0].Guid -eq $secondGuid -and $projects[-1].Guid -eq $rootGuid) 'Dependencies must precede their consumers'
	Assert-ARORCheck ($projects[1].Id -eq 'First') 'Read project GUIDs instead of trusting folder suffixes'

	Remove-Item -LiteralPath $second
	Assert-ARORFailure { Resolve-ARORAddonDependencies -RootProject $root -SearchRoots @($downloads) } 'Missing dependency 0000000000000002'
	Write-ARORFixture $second 'WrongAddonInRightFolder' '0000000000000003'
	Assert-ARORFailure { Resolve-ARORAddonDependencies -RootProject $root -SearchRoots @($downloads) } 'Missing dependency 0000000000000002'
	Write-ARORFixture $second 'Second' $secondGuid @($firstGuid)
	Assert-ARORFailure { Resolve-ARORAddonDependencies -RootProject $root -SearchRoots @($downloads) } 'dependency cycle'
	Write-ARORFixture $second 'Second' $secondGuid

	$duplicate = Join-Path $downloads 'duplicate/addon.gproj'
	Write-ARORFixture $duplicate 'DuplicateFirst' $firstGuid
	Assert-ARORFailure { Resolve-ARORAddonDependencies -RootProject $root -SearchRoots @($downloads) } 'Ambiguous dependency'
	$projects = @(Resolve-ARORAddonDependencies -RootProject $root -SearchRoots @($downloads) -ExplicitProjects @($first))
	Assert-ARORCheck ($projects.Count -eq 3 -and $projects[1].Id -eq 'First') 'Explicit repository project must override stale downloaded copies'
	$invalid = Join-Path $downloads 'unrelated/addon.gproj'
	[IO.Directory]::CreateDirectory((Split-Path -Parent $invalid)) | Out-Null
	[IO.File]::WriteAllText($invalid, 'Not a project')
	$projects = @(Resolve-ARORAddonDependencies -RootProject $root -SearchRoots @($downloads) -ExplicitProjects @($first))
	Assert-ARORCheck ($projects.Count -eq 3) 'An unrelated malformed download must not invalidate the required closure'
	Assert-ARORFailure { Read-ARORAddonProject -Path $invalid } 'Invalid addon project'
	Write-Output "PASS: $script:assertions dependency checks. Fixtures only; no Workbench process launched."
}
finally {
	$absoluteTarget = [IO.Path]::GetFullPath($testRoot)
	$absoluteTemp = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
	if (!$absoluteTarget.StartsWith($absoluteTemp, [StringComparison]::OrdinalIgnoreCase) -or
		!(Split-Path -Leaf $absoluteTarget).StartsWith('AROR-WCSDependencyTests-')) { throw "Unsafe fixture cleanup target: $absoluteTarget" }
	if (Test-Path -LiteralPath $absoluteTarget) { Remove-Item -LiteralPath $absoluteTarget -Recurse -Force }
}
