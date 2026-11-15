#!/bin/sh
# Pruebas de integracion de FOC: compila programas reales y comprueba el
# codigo de salida esperado.
#
# Cada programa tests/integration/<nombre>.<c|s|S|m4> tiene un fichero
# <nombre>.expected con el codigo de salida que debe producir.

set -u

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

FOC="${FOC:-$root/bin/foc}"
outdir="$root/build/integration"
mkdir -p "$outdir"

if [ ! -x "$FOC" ]; then
    echo "integracion: no se encuentra el compilador en $FOC" >&2
    exit 1
fi

passed=0
failed=0

for option in --help -h; do
    "$FOC" "$option" > "$outdir/help.log" 2>&1
    status=$?
    if [ "$status" -ne 0 ]; then
        echo "FALLO  $option: se esperaba el codigo 0 pero se obtuvo $status"
        failed=$((failed + 1))
    elif ! grep -q '^Uso: foc ' "$outdir/help.log"; then
        echo "FALLO  $option: no se mostro la ayuda"
        failed=$((failed + 1))
    else
        echo "OK     $option (codigo $status)"
        passed=$((passed + 1))
    fi
done

for source in tests/integration/*.c tests/integration/*.s tests/integration/*.S tests/integration/*.m4; do
    [ -e "$source" ] || continue
    name=$(basename "$source")
    name=${name%.*}
    expected_file="tests/integration/$name.expected"
    expected=0
    if [ -f "$expected_file" ]; then
        expected=$(cat "$expected_file")
    fi

    program="$outdir/$name"
    if ! "$FOC" "$source" -o "$program" > "$outdir/$name.log" 2>&1; then
        echo "FALLO  $name: la compilacion termino con error"
        sed -n '1,10p' "$outdir/$name.log"
        failed=$((failed + 1))
        continue
    fi

    "$program"
    status=$?
    if [ "$status" -ne "$expected" ]; then
        echo "FALLO  $name: se esperaba el codigo $expected pero se obtuvo $status"
        failed=$((failed + 1))
        continue
    fi
    echo "OK     $name (codigo $status)"
    passed=$((passed + 1))
done

echo "integracion: $passed correctas, $failed fallidas"
[ "$failed" -eq 0 ]
