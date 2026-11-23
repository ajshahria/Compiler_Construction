#!/bin/sh
# run_tests.sh - runs the compiler on every tests/<category>/*.tri program and
# compares the combined output (stdout + error messages) and exit status with
# the matching .expected file. A test can set compiler options on its first
# line, e.g.  // flags: --run
#
# The program's input for --run can be given on the second line:  // input: 41 1.25
#
#   sh tests/run_tests.sh            check all tests (also: make test)
#   sh tests/run_tests.sh --update   regenerate every .expected file
#
# The first line of each .expected file is the exit status ("exit 0" for a
# program that compiled, "exit 1" for one that was rejected); the rest is
# exactly what the compiler printed.

cd "$(dirname "$0")/.." || exit 2

BIN=build/tricora
[ -x "$BIN.exe" ] && BIN="$BIN.exe"
if [ ! -x "$BIN" ]; then
    echo "compiler not built: run 'make' first" >&2
    exit 2
fi

update=0
[ "$1" = "--update" ] && update=1

pass=0
fail=0
for src in tests/valid/*.tri tests/lexical/*.tri tests/syntax/*.tri tests/semantic/*.tri; do
    expected="${src%.tri}.expected"
    # A test can pass compiler options on its first line:  // flags: --run
    flags=$(sed -n '1s#^// *flags: *##p' "$src" | tr -d '\r')
    # ... and the program's input (for --run) on its second line:  // input: 41 1.25
    input=$(sed -n '2s#^// *input: *##p' "$src" | tr -d '\r')
    actual=$(printf '%s\n' "$input" | "$BIN" $flags "$src" 2>&1; echo "exit $?")
    # Put the exit status first and drop Windows line endings.
    actual=$(printf '%s\n' "$actual" | tr -d '\r' | sed -n '$p'; printf '%s\n' "$actual" | tr -d '\r' | sed '$d')

    if [ $update -eq 1 ]; then
        printf '%s\n' "$actual" > "$expected"
        echo "updated $expected"
        continue
    fi

    if [ -f "$expected" ] && [ "$actual" = "$(tr -d '\r' < "$expected")" ]; then
        echo "PASS  $src"
        pass=$((pass + 1))
    else
        echo "FAIL  $src"
        if [ -f "$expected" ]; then
            printf '%s\n' "$actual" | diff "$expected" - | sed 's/^/      /'
        else
            echo "      missing $expected"
        fi
        fail=$((fail + 1))
    fi
done

[ $update -eq 1 ] && exit 0
echo
echo "$pass passed, $fail failed"
[ $fail -eq 0 ]
