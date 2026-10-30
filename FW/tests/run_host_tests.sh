#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_binary=$(mktemp "${TMPDIR:-/tmp}/ir-protocol-test.XXXXXX")
trap 'rm -f "$test_binary"' EXIT HUP INT TERM
test_cc=${CC:-cc}
test_sanitizers=${IR_TEST_SANITIZERS:-address,undefined}
set --
if [ "$(uname -s)" = Darwin ] && [ -x /Library/Developer/CommandLineTools/usr/bin/clang ]; then
    test_cc=${CC:-/Library/Developer/CommandLineTools/usr/bin/clang}
    # The local macOS 26 ASan runtime stalls at startup; allow explicit opt-in.
    test_sanitizers=${IR_TEST_SANITIZERS:-undefined}
    set -- -isysroot "${SDKROOT:-/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk}"
fi
"$test_cc" "$@" -std=c11 -Wall -Wextra -Werror \
    -fsanitize="$test_sanitizers" -fno-sanitize-recover=all -fno-omit-frame-pointer \
    -I tests/host_stubs -I main main/ir_protocol.c tests/test_protocol.c \
    -o "$test_binary"
"$test_binary"
"$test_cc" "$@" -std=c11 -Wall -Wextra -Werror \
    -fsanitize="$test_sanitizers" -fno-sanitize-recover=all -fno-omit-frame-pointer \
    -I tests/host_stubs -I main main/ir_rx.c tests/test_rx.c \
    -o "$test_binary"
"$test_binary"
