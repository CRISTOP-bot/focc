# Pruebas de integracion de FOC en Windows.
# Compila cada programa de tests/integration y comprueba el codigo de salida.
#
#   powershell -ExecutionPolicy Bypass -File tests/run-integration.ps1

param(
    [string]$Foc = ""
)

$ErrorActionPreference = "Continue"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

if (-not $Foc) {
    $Foc = Join-Path $root "bin\foc.exe"
}
if (-not (Test-Path $Foc)) {
    Write-Error "no se encuentra el compilador en $Foc"
    exit 1
}

$outdir = Join-Path $root "build\integration"
New-Item -ItemType Directory -Force -Path $outdir | Out-Null

$passed = 0
$failed = 0

foreach ($source in Get-ChildItem (Join-Path $root "tests\integration") -Filter *.c) {
    $name = $source.BaseName
    $expectedFile = Join-Path $root "tests\integration\$name.expected"
    $expected = 0
    if (Test-Path $expectedFile) {
        $expected = [int](Get-Content $expectedFile -Raw).Trim()
    }
    $program = Join-Path $outdir "$name.exe"
    $log = Join-Path $outdir "$name.log"

    & $Foc $source.FullName -o $program *> $log
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FALLO  ${name}: la compilacion termino con error"
        Get-Content $log -TotalCount 10
        $failed++
        continue
    }

    & $program
    $status = $LASTEXITCODE
    if ($status -ne $expected) {
        Write-Host "FALLO  ${name}: se esperaba el codigo $expected pero se obtuvo $status"
        $failed++
        continue
    }
    Write-Host "OK     $name (codigo $status)"
    $passed++
}

Write-Host "integracion: $passed correctas, $failed fallidas"
if ($failed -ne 0) { exit 1 }
exit 0
