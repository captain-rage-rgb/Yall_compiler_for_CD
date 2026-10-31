#!/usr/bin/env bash
# tests/run_all.sh: Master test suite for the Yall compiler across all 8 phases.

set -u
export PATH="/usr/bin:/bin:$PATH"

YALL="./yall"
if [ ! -f "$YALL" ] && [ -f "$YALL.exe" ]; then
    YALL="./yall.exe"
fi

TOTAL=0
PASSED=0
FAILED=0

run_test() {
    local desc="$1"
    local cmd="$2"
    local expect_exit="$3"

    TOTAL=$((TOTAL + 1))
    eval "$cmd" > /dev/null 2>&1
    local exit_code=$?

    if [ "$exit_code" -eq "$expect_exit" ]; then
        echo "  [PASS] $desc"
        PASSED=$((PASSED + 1))
    else
        echo "  [FAIL] $desc (expected exit $expect_exit, got $exit_code)"
        FAILED=$((FAILED + 1))
    fi
}

echo "=========================================================="
echo "          YALL COMPILER MASTER TEST SUITE                "
echo "=========================================================="

echo ""
echo "--- Phase 1: Lexical Analysis Tests ---"
run_test "Lexer valid_basic" "$YALL tests/valid_basic.yall --tokens" 0
run_test "Lexer valid_rep" "$YALL tests/valid_rep.yall --tokens" 0
run_test "Lexer valid_out" "$YALL tests/valid_out.yall --tokens" 0
run_test "Lexer error_chars (rejects invalid chars)" "$YALL tests/error_chars.yall --tokens" 1
run_test "Lexer error_strings (rejects bad escapes/unterminated)" "$YALL tests/error_strings.yall --tokens" 1
run_test "Lexer error_cwords (rejects C-isms)" "$YALL tests/error_cwords.yall --tokens" 1
run_test "Lexer error_comment (rejects unclosed block comment)" "$YALL tests/error_comment.yall --tokens" 1
run_test "Lexer error_numbers (rejects malformed numbers)" "$YALL tests/error_numbers.yall --tokens" 1

echo ""
echo "--- Phase 2: Syntax Analysis (Parser) Tests ---"
run_test "Parser parse_ok_basic" "$YALL tests/parse_ok_basic.yall --ast" 0
run_test "Parser parse_ok_control" "$YALL tests/parse_ok_control.yall --ast" 0
run_test "Parser parse_ok_funcs" "$YALL tests/parse_ok_funcs.yall --ast" 0
run_test "Parser parse_ok_in" "$YALL tests/parse_ok_in.yall --ast" 0
run_test "Parser parse_err_missing_semi" "$YALL tests/parse_err_missing_semi.yall" 1
run_test "Parser parse_err_parens" "$YALL tests/parse_err_parens.yall" 1
run_test "Parser parse_err_braces" "$YALL tests/parse_err_braces.yall" 1
run_test "Parser parse_err_multi" "$YALL tests/parse_err_multi.yall" 1
run_test "Parser parse_err_array" "$YALL tests/parse_err_array.yall" 1
run_test "Parser parse_err_let_no_init" "$YALL tests/parse_err_let_no_init.yall" 1

echo ""
echo "--- Phase 3: Semantic Analysis Tests ---"
run_test "Semantic sem_ok_basic" "$YALL tests/sem_ok_basic.yall --semantics" 0
run_test "Semantic sem_ok_let" "$YALL tests/sem_ok_let.yall --semantics" 0
run_test "Semantic sem_ok_funcs" "$YALL tests/sem_ok_funcs.yall --semantics" 0
run_test "Semantic sem_ok_scopes" "$YALL tests/sem_ok_scopes.yall --semantics" 0
run_test "Semantic sem_warn (unused var & unreachable code)" "$YALL tests/sem_warn.yall --semantics" 0
run_test "Semantic sem_err_undeclared" "$YALL tests/sem_err_undeclared.yall" 1
run_test "Semantic sem_err_redecl" "$YALL tests/sem_err_redecl.yall" 1
run_test "Semantic sem_err_types (8 type errors)" "$YALL tests/sem_err_types.yall" 1
run_test "Semantic sem_err_funcs" "$YALL tests/sem_err_funcs.yall" 1
run_test "Semantic sem_err_return" "$YALL tests/sem_err_return.yall" 1
run_test "Semantic sem_err_loop" "$YALL tests/sem_err_loop.yall" 1
run_test "Semantic sem_err_fixed" "$YALL tests/sem_err_fixed.yall" 1
run_test "Semantic sem_err_misc" "$YALL tests/sem_err_misc.yall" 1

echo ""
echo "--- Phase 4: Intermediate Code Generation (TAC) Tests ---"
run_test "TAC ir_basic" "$YALL tests/ir_basic.yall --tac" 0
run_test "TAC ir_widen" "$YALL tests/ir_widen.yall --tac" 0
run_test "TAC ir_if" "$YALL tests/ir_if.yall --tac" 0
run_test "TAC ir_loops" "$YALL tests/ir_loops.yall --tac" 0
run_test "TAC ir_funcs" "$YALL tests/ir_funcs.yall --tac" 0
run_test "TAC ir_out" "$YALL tests/ir_out.yall --tac" 0
run_test "TAC ir_logic" "$YALL tests/ir_logic.yall --tac" 0
run_test "TAC ir_in" "$YALL tests/ir_in.yall --tac" 0
run_test "TAC ir_compound" "$YALL tests/ir_compound.yall --tac" 0
run_test "TAC ir_prompt1" "$YALL tests/ir_prompt1.yall --tac" 0
run_test "TAC ir_prompt2" "$YALL tests/ir_prompt2.yall --tac" 0

echo ""
echo "--- Phase 5: TAC Optimizer Tests ---"
run_test "Opt opt_fold" "$YALL tests/opt_fold.yall --opt" 0
run_test "Opt opt_prop" "$YALL tests/opt_prop.yall --opt" 0
run_test "Opt opt_branch" "$YALL tests/opt_branch.yall --opt" 0
run_test "Opt opt_dead" "$YALL tests/opt_dead.yall --opt" 0
run_test "Opt opt_algebra" "$YALL tests/opt_algebra.yall --opt" 0
run_test "Opt opt_calls" "$YALL tests/opt_calls.yall --opt" 0
run_test "Opt opt_safety" "$YALL tests/opt_safety.yall --opt" 0

echo ""
echo "--- Phase 6: Bytecode Generation Tests ---"
run_test "Bytecode bc_basic" "$YALL tests/bc_basic.yall --bytecode" 0
run_test "Bytecode bc_decimal" "$YALL tests/bc_decimal.yall --bytecode" 0
run_test "Bytecode bc_text" "$YALL tests/bc_text.yall --bytecode" 0
run_test "Bytecode bc_loops" "$YALL tests/bc_loops.yall --bytecode" 0
run_test "Bytecode bc_funcs" "$YALL tests/bc_funcs.yall --bytecode" 0

echo ""
echo "--- Phase 7: Virtual Machine & Differential Tests ---"
sh tests/run_diff.sh
DIFF_EXIT=$?
TOTAL=$((TOTAL + 1))
if [ $DIFF_EXIT -eq 0 ]; then
    PASSED=$((PASSED + 1))
    echo "  [PASS] Phase 7 Differential & Execution Suite"
else
    FAILED=$((FAILED + 1))
    echo "  [FAIL] Phase 7 Differential & Execution Suite"
fi

echo ""
echo "--- Phase 8: Example Demos ---"
run_test "Demo hello" "$YALL examples/demo_hello.yall" 0
run_test "Demo primes" "$YALL examples/demo_primes.yall" 0
run_test "Demo fib" "$YALL examples/demo_fib.yall" 0
run_test "Demo calc" "$YALL examples/demo_calc.yall" 0
run_test "Demo err_lex (halts before parse)" "$YALL examples/demo_err_lex.yall" 1
run_test "Demo err_syntax (halts before semantic)" "$YALL examples/demo_err_syntax.yall" 1
run_test "Demo err_sem (halts before TAC)" "$YALL examples/demo_err_sem.yall" 1

echo ""
echo "=========================================================="
echo "TEST SUMMARY: $PASSED / $TOTAL passed ($FAILED failed)"
echo "=========================================================="

if [ $FAILED -ne 0 ]; then
    exit 1
fi
exit 0
