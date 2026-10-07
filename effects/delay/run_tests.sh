#!/usr/bin/env bash
# Build and run the delay tests, then play the delay live.
#
#   effects/delay/run_tests.sh          run checks, then live mode (n = pluck, Ctrl-C = quit)
#   effects/delay/run_tests.sh --once   run checks only, write build-host/delay_demo.wav

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
BUILD="$ROOT/build-host"
mkdir -p "$BUILD"

CFLAGS=(-std=c11 -O2 -Wall -Wextra -Werror -I "$HERE" -I "$ROOT/common")

cc "${CFLAGS[@]}" "$HERE/delay.c" "$HERE/test_delay.c" \
    -lm -o "$BUILD/test_delay"

cc "${CFLAGS[@]}" "$HERE/delay.c" "$HERE/live_delay.c" "$ROOT/common/live_host.c" \
    -framework AudioToolbox -o "$BUILD/live_delay"

"$BUILD/test_delay" "$BUILD/delay_demo.wav"

if [[ "${1:-}" != "--once" ]]; then
    echo
    "$BUILD/live_delay"
fi
