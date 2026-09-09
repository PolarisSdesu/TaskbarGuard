$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot 'TaskbarGuard.cpp'
$output = Join-Path $PSScriptRoot 'TaskbarGuard-x64.exe'
& g++ -std=c++17 -Os -s -mwindows -municode -static $source -o $output -lshell32 -ladvapi32
if ($LASTEXITCODE -ne 0) { throw "g++ failed with exit code $LASTEXITCODE" }
Get-Item $output | Select-Object FullName, Length
