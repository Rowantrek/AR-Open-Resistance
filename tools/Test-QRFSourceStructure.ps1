# Lexical and known native-API source guards only. This is not an Enforce parser or Workbench compiler.
$ErrorActionPreference = 'Stop'
function Get-InvalidOwnedArrayTransfers([string] $sourceText) {
 $code = [regex]::Replace($sourceText, '(?s)/\*.*?\*/|//[^\r\n]*|"(?:\\.|[^"\\])*"', '')
 $owned = @([regex]::Matches($code, '\barray<\s*ref\s+\w+\s*>\s+(\w+)') | ForEach-Object { $_.Groups[1].Value })
 foreach ($call in [regex]::Matches($code, '\.InsertAll\(\s*(?:\w+\.)?(\w+)\s*\)')) {
  if ($owned -contains $call.Groups[1].Value) { $call.Value }
 }
}
if (@(Get-InvalidOwnedArrayTransfers 'ref array<ref AROR_Guard> source = {}; target.InsertAll(source);').Count -ne 1) { throw 'Owned-array regression guard missed the native type mismatch' }
if (@(Get-InvalidOwnedArrayTransfers 'ref array<AROR_Guard> source = {}; target.InsertAll(source);').Count -ne 0) { throw 'Owned-array guard rejected a weak-reference array' }
if (@(Get-InvalidOwnedArrayTransfers 'ref array<ref AROR_Guard> source = {}; foreach (AROR_Guard item : source) target.Insert(item);').Count -ne 0) { throw 'Owned-array guard rejected per-element transfer' }
$repoRoot = Split-Path -Parent $PSScriptRoot
$scriptRoot = Join-Path $repoRoot 'AROpenResistance/Open Resistance/Scripts'
$checked = 0
$files = Get-ChildItem -LiteralPath $scriptRoot -Recurse -Filter '*.c'
foreach ($file in $files) {
 $source = [IO.File]::ReadAllText($file.FullName)
 $invalidTransfers = @(Get-InvalidOwnedArrayTransfers $source)
 if ($invalidTransfers.Count) { throw "Native InsertAll cannot accept owned-reference arrays in $($file.Name): $($invalidTransfers -join ', ')" }
 $stack = New-Object 'Collections.Generic.Stack[char]'
 $quoted = $false; $lineComment = $false; $blockComment = $false
 for ($i=0; $i -lt $source.Length; $i++) {
  $ch = $source[$i]; $next = [char]0
  if ($i+1 -lt $source.Length) { $next = $source[$i+1] }
  if ($lineComment) { if ($ch -eq "`n") { $lineComment=$false }; continue }
  if ($blockComment) { if ($ch -eq '*' -and $next -eq '/') { $blockComment=$false; $i++ }; continue }
  if ($quoted) { if ($ch -eq '\') { $i++; continue }; if ($ch -eq '"') { $quoted=$false }; continue }
  if ($ch -eq '/' -and $next -eq '/') { $lineComment=$true; $i++; continue }
  if ($ch -eq '/' -and $next -eq '*') { $blockComment=$true; $i++; continue }
  if ($ch -eq '"') { $quoted=$true; continue }
  if ($ch -in @('{','(','[')) { $stack.Push($ch); continue }
  if ($ch -in @('}',')',']')) {
   if ($stack.Count -eq 0) { throw "Unexpected closing delimiter in $($file.Name) at character $i" }
   $opening = $stack.Pop()
   if (("$opening$ch") -notin @('{}','()','[]')) { throw "Mismatched delimiter in $($file.Name) at character $i" }
  }
 }
 if ($quoted -or $blockComment -or $stack.Count) { throw "Unclosed lexical structure in $($file.Name)" }
 $depth = 0
 foreach ($directive in [regex]::Matches($source, '(?m)^\s*#(ifdef|ifndef|if|else|elif|endif)\b')) {
  $kind = $directive.Groups[1].Value
  if ($kind -in @('if','ifdef','ifndef')) { $depth++ }
  elseif ($kind -eq 'endif') { $depth-- }
  elseif ($depth -eq 0) { throw "Orphan preprocessor branch in $($file.Name)" }
  if ($depth -lt 0) { throw "Orphan #endif in $($file.Name)" }
 }
 if ($depth -ne 0) { throw "Unclosed preprocessor conditional in $($file.Name)" }
 $checked++
}
Write-Output "Checked balanced delimiters, strings, comments, preprocessor nesting and owned-reference array transfers in $checked addon scripts, including Workbench plugins. Not an Enforce compilation."
