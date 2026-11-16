#!/usr/bin/env bash
# Run clang-tidy and cppcheck on the library sources; reports go to build/analysis/.
# Usage: ./scripts/analyze.sh [--preset linux-debug] [--strict]
set -euo pipefail

repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo"
preset=linux-debug strict=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --preset) preset="$2"; shift 2 ;;
        --strict) strict=1; shift ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

out="$repo/build/analysis"
mkdir -p "$out"
mapfile -t sources < <(git ls-files '*.c' | grep -v /)

cmake --preset "$preset" >/dev/null
db="$repo/build/$preset"

clang-tidy -p "$db" --quiet "${sources[@]}" > "$out/clang-tidy.txt" 2>&1 || true
tidy=$(grep -cE ': (warning|error): ' "$out/clang-tidy.txt" || true)

filters=()
for s in "${sources[@]}"; do filters+=("--file-filter=*$s"); done
cppcheck --project="$db/compile_commands.json" --enable=warning,performance,portability --std=c99 \
    --inline-suppr --quiet --template='{file}:{line}: {severity}: {message} [{id}]' "${filters[@]}" \
    > "$out/cppcheck.txt" 2>&1 || true
cpp=$(grep -cE ': (error|warning|performance|portability): ' "$out/cppcheck.txt" || true)

echo "Static analysis summary (reports in $out):"
printf '  %-30s %s\n' clang-tidy "$tidy" cppcheck "$cpp"
if [[ $strict -eq 1 && $((tidy + cpp)) -gt 0 ]]; then exit 1; fi
