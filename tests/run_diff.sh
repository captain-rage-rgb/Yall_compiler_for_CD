#!/usr/bin/env bash
# tests/run_diff.sh: Differential testing script for Yall
# Compares --run (optimized) against --run --no-opt and tests/expected/*.out

set -u
export PATH="/usr/bin:/bin:$PATH"

YALL="./yall"
if [ ! -f "$YALL" ] && [ -f "$YALL.exe" ]; then
    YALL="./yall.exe"
fi

FAILED=0
PASSED=0

echo "=== RUNNING DIFFERENTIAL TESTS (Optimized vs Unoptimized) ==="

NORMAL_TESTS=(
    "run_hello"
    "run_arith"
    "run_decimal"
    "run_loops"
    "run_funcs"
    "run_logic"
    "run_text"
    "run_scope"
    "run_wrap"
)

TMP_OPT="/tmp/yall_opt.out"
TMP_NOOPT="/tmp/yall_noopt.out"

for t in "${NORMAL_TESTS[@]}"; do
    SRC="tests/${t}.yall"
    EXPECTED="tests/expected/${t}.out"

    $YALL "$SRC" --run > "$TMP_OPT" 2>/dev/null
    OPT_EXIT=$?

    $YALL "$SRC" --run --no-opt > "$TMP_NOOPT" 2>/dev/null
    NOOPT_EXIT=$?

    if [ $OPT_EXIT -ne 0 ] || [ $NOOPT_EXIT -ne 0 ]; then
        echo "FAIL: $t (nonzero exit code: opt=$OPT_EXIT noopt=$NOOPT_EXIT)"
        FAILED=$((FAILED + 1))
        continue
    fi

    if cmp -s "$TMP_OPT" "$TMP_NOOPT"; then
        if [ -f "$EXPECTED" ]; then
            # Strip trailing CR for Windows compatibility
            tr -d '\r' < "$TMP_OPT" > "${TMP_OPT}.clean"
            tr -d '\r' < "$EXPECTED" > "${EXPECTED}.clean"
            if cmp -s "${TMP_OPT}.clean" "${EXPECTED}.clean"; then
                echo "PASS: $t (optimized == unoptimized == expected)"
                PASSED=$((PASSED + 1))
            else
                echo "FAIL: $t (output matches unoptimized, but differs from expected file)"
                diff -u "${EXPECTED}.clean" "${TMP_OPT}.clean"
                FAILED=$((FAILED + 1))
            fi
            rm -f "${TMP_OPT}.clean" "${EXPECTED}.clean"
        else
            echo "PASS: $t (optimized == unoptimized)"
            PASSED=$((PASSED + 1))
        fi
    else
        echo "FAIL: $t (optimized output differs from unoptimized output)"
        diff -u "$TMP_NOOPT" "$TMP_OPT"
        FAILED=$((FAILED + 1))
    fi
done

echo ""
echo "=== RUNNING RUNTIME ERROR TESTS ==="

# 1. run_err_div
ERR_OUT=$($YALL tests/run_err_div.yall --run 2>&1)
ERR_EXIT=$?
if [ $ERR_EXIT -eq 3 ] && echo "$ERR_OUT" | grep -q "division by zero"; then
    echo "PASS: run_err_div (exit 3 and reported division by zero)"
    PASSED=$((PASSED + 1))
else
    echo "FAIL: run_err_div (exit $ERR_EXIT, expected 3; output: $ERR_OUT)"
    FAILED=$((FAILED + 1))
fi

# 2. run_err_deep
ERR_OUT=$($YALL tests/run_err_deep.yall --run 2>&1)
ERR_EXIT=$?
if [ $ERR_EXIT -eq 3 ] && echo "$ERR_OUT" | grep -q "stack overflow"; then
    echo "PASS: run_err_deep (exit 3 and reported stack overflow)"
    PASSED=$((PASSED + 1))
else
    echo "FAIL: run_err_deep (exit $ERR_EXIT, expected 3; output: $ERR_OUT)"
    FAILED=$((FAILED + 1))
fi

# 3. run_err_loop
ERR_OUT=$($YALL tests/run_err_loop.yall --run --max-steps 1000 2>&1)
ERR_EXIT=$?
if [ $ERR_EXIT -eq 3 ] && echo "$ERR_OUT" | grep -q "step limit exceeded"; then
    echo "PASS: run_err_loop (exit 3 and reported step limit exceeded)"
    PASSED=$((PASSED + 1))
else
    echo "FAIL: run_err_loop (exit $ERR_EXIT, expected 3; output: $ERR_OUT)"
    FAILED=$((FAILED + 1))
fi

rm -f "$TMP_OPT" "$TMP_NOOPT"

echo ""
echo "Differential Testing Summary: $PASSED passed, $FAILED failed."
if [ $FAILED -ne 0 ]; then
    exit 1
fi
exit 0
