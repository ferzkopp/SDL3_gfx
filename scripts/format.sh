#!/usr/bin/env bash
# Format (or with --check: verify) all C sources and headers with clang-format.
# Uses .clang-format in the repository root; files listed in .clang-format-ignore are skipped.
set -euo pipefail

repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo"
cf=${CLANG_FORMAT:-clang-format}

ignore=()
if [[ -f .clang-format-ignore ]]; then
    while IFS= read -r line; do
        line="${line%%#*}"; line="${line//[[:space:]]/}"
        [[ -n "$line" ]] && ignore+=("$line")
    done < .clang-format-ignore
fi

files=()
while IFS= read -r f; do
    skip=0
    for pat in "${ignore[@]}"; do
        # shellcheck disable=SC2053
        [[ "$f" == $pat ]] && skip=1
    done
    [[ $skip -eq 0 ]] && files+=("$f")
done < <(git ls-files '*.c' '*.h')

echo "$($cf --version) on ${#files[@]} files"
if [[ "${1:-}" == "--check" ]]; then
    "$cf" --dry-run --Werror "${files[@]}"
    echo "All files formatted."
else
    "$cf" -i "${files[@]}"
fi
