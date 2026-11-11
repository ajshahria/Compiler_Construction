#!/bin/bash
# Runs every tests/<category>/*.cpp through the compiler and compares with the .expected file.
#   bash tests/run_tests.sh            run all tests
#   bash tests/run_tests.sh --update   regenerate .expected files (review them with git diff!)
cd "$(dirname "$0")/.."
COMPILER=build/compiler.exe
pass=0; fail=0
for f in tests/*/*.cpp; do
    exp="${f%.cpp}.expected"
    # a test can opt out of the header rule with a first line:  // flags: --allow-no-header
    extra=$(head -1 "$f" | sed -n 's#^// flags: ##p' | tr -d '\r')
    # a test can give 'cin' input with a line near the top:  // stdin: 2 5
    input=$(head -3 "$f" | sed -n 's#^// stdin: ##p' | tr -d '\r')
    actual=$(echo "$input" | $COMPILER $extra --headers --tac --opt --run "$f" 2>&1)
    if [ "$1" == "--update" ]; then echo "$actual" > "$exp"; continue; fi
    if [ "$actual" == "$(cat "$exp" 2>/dev/null)" ]; then
        pass=$((pass+1)); echo "PASS  $f"
    else
        fail=$((fail+1)); echo "FAIL  $f"
    fi
done
[ "$1" == "--update" ] && { echo "expected files updated"; exit 0; }
echo "Passed: $pass  Failed: $fail"
[ $fail -eq 0 ]
