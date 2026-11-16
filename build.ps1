# Construye FOC en Windows.
# Uso: powershell -ExecutionPolicy Bypass -File build.ps1 [-Out bin/foc.exe]
param(
    [string]$Out = "bin/foc.exe",
    [string]$CC = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

if (-not $CC) {
    $gcc = Get-Command gcc -ErrorAction SilentlyContinue
    if ($gcc) {
        $CC = $gcc.Source
    } else {
        $clang = Get-Command clang -ErrorAction SilentlyContinue
        if ($clang) {
            $CC = $clang.Source
        } else {
            $cc = Get-Command cc -ErrorAction SilentlyContinue
            if ($cc) { $CC = $cc.Source }
        }
    }
}
if (-not $CC) {
    Write-Error "No se encontro GCC o Clang. Instala MinGW-w64/LLVM o define -CC <ruta al compilador>."
    exit 1
}

$srcs = Get-ChildItem -Recurse -Path "foc\src" -Filter *.c | Sort-Object FullName | ForEach-Object { $_.FullName }
$srcs += ([System.IO.Path]::GetFullPath((Join-Path $root "libcc1\src\passes.c")))
if ($srcs.Count -eq 0) { Write-Error "No hay fuentes en foc\src/"; exit 1 }

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Out) | Out-Null
Write-Host "Compilando $($srcs.Count) ficheros -> $Out (con $CC)"
& $CC -std=c11 -O2 -Wall -Wextra -Ifoc\include @srcs -o $Out
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Listo: $Out"
