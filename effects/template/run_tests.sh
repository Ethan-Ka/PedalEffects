#!/usr/bin/env bash
# Build and run the template tests, then play the template live.
#
#   effects/template/run_tests.sh          run checks, then live mode (n = pluck, Ctrl-C = quit)
#   effects/template/run_tests.sh --once   run checks only

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
BUILD="$ROOT/build-host"
mkdir -p "$BUILD"

CFLAGS=(-std=c11 -O2 -Wall -Wextra -Werror -I "$HERE" -I "$ROOT/common")

cc "${CFLAGS[@]}" "$HERE/template.c" "$HERE/test_template.c" \
    -lm -o "$BUILD/test_template"

cc "${CFLAGS[@]}" "$HERE/template.c" "$HERE/live_template.c" "$ROOT/common/live_host.c" \
    -framework AudioToolbox -o "$BUILD/live_template"

"$BUILD/test_template"

if [[ "${1:-}" != "--once" ]]; then
    echo
    "$BUILD/live_template"
fi
