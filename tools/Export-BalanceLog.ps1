[CmdletBinding()]
param(
	[string[]]$LogPath = @(),	# console.log files or log folders; default: the newest Workbench log folder
	[string]$OutDir = ''		# where the CSVs go; default: next to the first log
)
# Turns "[AROR BALANCE] <event> key=value ..." lines from Reforger console logs into one CSV per event, for tuning
# town responses and escalation from playtests:
#   town_response_start / town_wave / town_contest / town_escalation / town_response_end / town_response_effect /
#   town_response_deferred / post_taken
# Each row also carries the log's file name and the line's clock time.
$ErrorActionPreference = 'Stop'

if ($LogPath.Count -eq 0) {
	$roots = @("$env:USERPROFILE\OneDrive\Documents\My Games\ArmaReforgerWorkbench\logs", "$env:USERPROFILE\Documents\My Games\ArmaReforgerWorkbench\logs")
	$newest = $roots | Where-Object { Test-Path $_ } | ForEach-Object { Get-ChildItem $_ -Directory } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
	if (!$newest) { throw 'No Workbench log folder found; pass -LogPath' }
	$LogPath = @($newest.FullName)
}

$files = foreach ($path in $LogPath) {
	if (Test-Path $path -PathType Container) { Get-ChildItem $path -Recurse -Filter 'console.log' } else { Get-Item $path }
}
if (!$files) { throw 'No console.log found' }
if (!$OutDir) { $OutDir = Join-Path $files[0].DirectoryName 'balance' }
New-Item -ItemType Directory -Force $OutDir | Out-Null

$rows = @{}
foreach ($file in $files) {
	foreach ($line in [IO.File]::ReadLines($file.FullName)) {
		$m = [regex]::Match($line, '^(?<time>\S+).*\[AROR BALANCE\] (?<event>\w+) (?<fields>.*)$')
		if (!$m.Success) { continue }
		$row = [ordered]@{ log = $file.Directory.Name; time = $m.Groups['time'].Value }
		foreach ($pair in [regex]::Matches($m.Groups['fields'].Value, '(\w+)=(\S+)')) { $row[$pair.Groups[1].Value] = $pair.Groups[2].Value }
		$event = $m.Groups['event'].Value
		if (!$rows.ContainsKey($event)) { $rows[$event] = New-Object System.Collections.Generic.List[object] }
		$rows[$event].Add([pscustomobject]$row)
	}
}
if ($rows.Count -eq 0) { Write-Host 'No [AROR BALANCE] lines in these logs'; return }
foreach ($event in $rows.Keys | Sort-Object) {
	$target = Join-Path $OutDir "$event.csv"
	$rows[$event] | Export-Csv -NoTypeInformation -Path $target
	Write-Host ("{0,-26} {1,5} rows  {2}" -f $event, $rows[$event].Count, $target)
}
