<# blah
.SYNOPSIS

Build script for bugspaceport project

.EXAMPLE

powershell .\build.ps1
#>
$CurrDir = Split-Path $Script:MyInvocation.MyCommand.Path

Function build {
  $BuildFolder = $CurrDir + '\build\'
  &cmake --build "$BuildFolder" --config "Release"
  if (-not($LASTEXITCODE -eq 0)) {
    Write-Host "[ERROR] While building leakspaceport " -ForegroundColor Red
    return
  }
  Write-Host "[INFO] leakspaceport built correctly" -ForegroundColor Green

  return
}

function configure {
  $BuildFolder = $CurrDir + '\build\'
  &cmake  -A "x64" -B"$BuildFolder" -S"$CurrDir"
  if (-not($LASTEXITCODE -eq 0)) {
    Write-Host "[ERROR] while configuring leakspaceport" -ForegroundColor Red
    Return
  }
  Write-Host "[INFO] leakspaceport configured correctly" -ForegroundColor Green
}

Write-Host "[INFO] Started Cmake Configuration " -ForegroundColor Cyan
configure
Write-Host "[INFO] Cmake Configuration Done" -ForegroundColor Cyan

Write-Host "[INFO] Started building" -ForegroundColor Cyan
build $InputConfig
Write-Host "[INFO] Building Done" -ForegroundColor Cyan