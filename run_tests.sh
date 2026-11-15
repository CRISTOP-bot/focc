#!/usr/bin/env bash
# Ejecuta la suite de pruebas de FOC.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$root"

CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c11 -O1 -g -Wall -Wextra -Ifoc/include -Itests}"

echo "== Compilando las pruebas unitarias =="
mkdir -p build
test_srcs=()
while IFS= read -r f; do test_srcs+=("$f"); done < <(find tests/unit -name '*.c' | sort)
impl_srcs=()
while IFS= read -r f; do impl_srcs+=("$f"); done < <(find foc/src -name '*.c' ! -path 'foc/src/driver/main.c' | sort)
# shellcheck disable=SC2086
"$CC" $CFLAGS "${test_srcs[@]}" "${impl_srcs[@]}" -o build/foc_tests
echo "== Ejecutando =="
./build/foc_tests
echo "== Probando libc/stdio =="
# shellcheck disable=SC2086
"$CC" $CFLAGS libc/src/stdio.c tests/libc/test_stdio.c -o build/foc_stdio_tests
./build/foc_stdio_tests
echo "== Pruebas de integracion =="
sh ./tests/run-integration.sh
