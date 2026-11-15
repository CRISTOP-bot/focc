# Ejecuta las pruebas unitarias y de integracion de FOC en Windows.
param(
    [string]$CC = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

if (-not $CC) {
    foreach ($candidate in @("gcc", "clang", "cc")) {
        $compiler = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($compiler) {
            $CC = $compiler.Source
            break
        }
    }
}
if (-not $CC) {
    Write-Error "No se encontro GCC o Clang. Instala MinGW-w64/LLVM o define -CC <ruta>."
    exit 1
}

$testSources = @(Get-ChildItem -Recurse -Path "tests\unit" -Filter *.c |
    Sort-Object FullName | ForEach-Object { $_.FullName })
$mainSource = [System.IO.Path]::GetFullPath((Join-Path $root "foc\src\driver\main.c"))
$implementationSources = @(Get-ChildItem -Recurse -Path "foc\src" -Filter *.c |
    Where-Object { $_.FullName -ne $mainSource } |
    Sort-Object FullName | ForEach-Object { $_.FullName })
if ($testSources.Count -eq 0 -or $implementationSources.Count -eq 0) {
    Write-Error "No se encontraron fuentes de pruebas o del compilador."
    exit 1
}

New-Item -ItemType Directory -Force -Path "build" | Out-Null
$testArgs = @("-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Ifoc\include", "-Itests") + $testSources + $implementationSources + @("-o", "build\foc_tests.exe")
Write-Host "Compilando pruebas unitarias con $CC"
& $CC @testArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& ".\build\foc_tests.exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Probando libc/stdio"
$stdioArgs = @("-std=c11", "-O1", "-g", "-Wall", "-Wextra", "libc\src\stdio.c", "tests\libc\test_stdio.c", "-o", "build\foc_stdio_tests.exe")
& $CC @stdioArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ".\build\foc_stdio_tests.exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not (Test-Path ".\bin\foc.exe")) {
    Write-Error "No se encontro bin\foc.exe; ejecuta primero build.ps1."
    exit 1
}

Write-Host "Ejecutando pruebas de integracion"
& ".\tests\run-integration.ps1" -Foc ".\bin\foc.exe"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
