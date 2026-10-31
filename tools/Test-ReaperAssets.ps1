param([Parameter(Mandatory = $true)][string]$ResourceDatabasePath)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $repoRoot 'AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_ReaperQRFExecutor.c'
$source = [IO.File]::ReadAllText($sourcePath)
$references = @([regex]::Matches($source, '\{([A-F0-9]{16})\}(Prefabs/[^"\r\n]*REAPER_[^"\r\n]+)') | ForEach-Object {
 [pscustomobject]@{ Guid=$_.Groups[1].Value; Path=$_.Groups[2].Value }
} | Sort-Object Path -Unique)
if ($references.Count -ne 5) { throw 'Expected two REAPER pilot and three flight-waypoint references' }
$bytes = [IO.File]::ReadAllBytes($ResourceDatabasePath)
$text = [Text.Encoding]::GetEncoding(28591).GetString($bytes)
foreach ($reference in $references) {
 $offset = $text.IndexOf($reference.Path + [char]0)
 if ($offset -lt 0) { throw "Missing installed REAPER resource: $($reference.Path)" }
 $guid = [BitConverter]::ToUInt64($bytes, $offset + $reference.Path.Length + 7).ToString('X16')
 if ($guid -cne $reference.Guid) { throw "Installed REAPER GUID differs: $($reference.Path)" }
}
Write-Output 'Verified all five installed REAPER pilot/flight-waypoint references. This does not verify native flight or script compilation.'
