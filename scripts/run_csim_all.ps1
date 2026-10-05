$ErrorActionPreference='Stop'
$Root=Split-Path -Parent $PSScriptRoot
$Build=Join-Path $Root 'reports\host_build'
New-Item -ItemType Directory -Force -Path $Build | Out-Null
$exe=Join-Path $Build 'fd_bb_slbb_tb.exe'
$src=Join-Path $Root 'src\fd_bb_slbb.cpp'
$tb=Join-Path $Root 'tb\tb_slbb.cpp'
$inc=Join-Path $Root 'src'
$gxx=Get-Command g++ -ErrorAction SilentlyContinue
if (-not $gxx) { throw 'g++ was not found. Run this from the Vitis HLS command prompt.' }
& g++ -O2 -std=c++17 -I$inc $src $tb -o $exe
if ($LASTEXITCODE -ne 0) { throw 'Host compilation failed.' }
$rootData=Join-Path $Root 'data\taillard'
$out=Join-Path $Root 'reports\slbb_csim_120.csv'
& $exe $rootData $out all
if ($LASTEXITCODE -ne 0) { throw 'Batch C simulation failed.' }
Write-Host "[FD-BB] Batch CSV: $out"
