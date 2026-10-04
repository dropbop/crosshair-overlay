param(
  [string]$Compiler = ''
)
$ErrorActionPreference = 'Stop'
$projectDirectory = $PSScriptRoot
if (-not $Compiler) {
  $found = Get-Command g++.exe -ErrorAction SilentlyContinue
  if ($found) { $Compiler = $found.Source }
  elseif (Test-Path -LiteralPath 'C:\msys64\ucrt64\bin\g++.exe') {
    $Compiler = 'C:\msys64\ucrt64\bin\g++.exe'
  } else { throw 'An x64 MinGW-w64 C++ compiler is required.' }
}
$compilerDirectory = Split-Path -Parent $Compiler
$windres = Join-Path $compilerDirectory 'windres.exe'
$objdump = Join-Path $compilerDirectory 'objdump.exe'
$originalPath = $env:PATH
try {
  $env:PATH = "$compilerDirectory;$originalPath"
  Push-Location -LiteralPath $projectDirectory
  New-Item -ItemType Directory -Force -Path 'build', 'dist' | Out-Null
  & $windres 'app.rc' '-O' 'coff' '-o' 'build\resources.o'
  if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed.' }
  $flags = @('-std=c++17', '-O2', '-Wall', '-Wextra', '-Wno-missing-field-initializers',
    '-fno-exceptions', '-fno-rtti', '-ffunction-sections', '-fdata-sections',
    '-static', '-static-libgcc', '-static-libstdc++', '-Wl,--gc-sections', '-s')
  & $Compiler @flags '-municode' '-mwindows' 'main.cpp' 'build\resources.o' '-o' 'dist\CrosshairOverlay.exe' '-lcomctl32' '-lshell32' '-lole32' '-luuid' '-lgdi32' '-luser32' '-ladvapi32' '-lwtsapi32'
  if ($LASTEXITCODE -ne 0) { throw 'Application compilation failed. Exit the running app before rebuilding.' }
  $imports = & $objdump '-p' 'dist\CrosshairOverlay.exe'
  if ($LASTEXITCODE -ne 0) { throw 'Dependency inspection failed.' }
  $dlls = @($imports | Select-String 'DLL Name:' | ForEach-Object { ($_.Line -split 'DLL Name:\s*')[1].Trim() })
  $systemDlls = @('ADVAPI32.dll', 'COMCTL32.dll', 'GDI32.dll', 'KERNEL32.dll',
    'msvcrt.dll', 'ole32.dll', 'SHELL32.dll', 'USER32.dll', 'WTSAPI32.dll', 'ucrtbase.dll')
  foreach ($dll in $dlls) {
    if (($dll -notin $systemDlls) -and ($dll -notlike 'api-ms-win-*.dll')) {
      throw "Unexpected runtime dependency: $dll"
    }
  }
  $bytes = (Get-Item -LiteralPath 'dist\CrosshairOverlay.exe').Length
  if ($bytes -ge 1000000) { throw "Executable exceeds the 1 MB budget: $bytes bytes." }
  Write-Output "Built dist\CrosshairOverlay.exe ($bytes bytes)"
  Write-Output "Windows-only imports: $($dlls -join ', ')"
} finally {
  Pop-Location
  $env:PATH = $originalPath
}
