#!/usr/bin/env bash
# Construye FOC (Fucking Optimized C Compiler).
# Uso: ./build.sh [salida]
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$root"

CC="${CC:-cc}"
CFLAGS="${CFLAGS:--std=c11 -O2 -Wall -Wextra -Wpedantic -Ifoc/include}"
out="${1:-bin/foc}"

if ! command -v "$CC" >/dev/null 2>&1; then
    echo "error: no se encontró el compilador C '$CC'. Define CC=..." >&2
    exit 1
fi

mkdir -p "$(dirname "$out")"

srcs=()
while IFS= read -r f; do srcs+=("$f"); done < <(find foc/src -name '*.c' | sort)

echo "Compilando ${#srcs[@]} ficheros -> $out"
# shellcheck disable=SC2086
"$CC" $CFLAGS "${srcs[@]}" -o "$out"
echo "Listo: $out"
