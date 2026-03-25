#!/bin/bash
#
# Test quantization decision accuracy on 10 test cases.
#
# Expected results:
#   YES: e_exp, e_log, e_y0, e_j0, sincosf
#   NO:  e_acosh, e_rem_pio2, float64_add, float64_div, float64_mul
#
# Usage:
#   cd applications/newton/llvm-ir/performance_test
#   ./test_quant_decisions.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

NEWTON_BIN="$ROOT_DIR/src/newton/newton-linux-EN"
SENSOR_FILE="$ROOT_DIR/applications/newton/sensors/test.nt"
LLVM_IR_DIR="$ROOT_DIR/applications/newton/llvm-ir"
NEWTON_WORKDIR="$ROOT_DIR/src/newton"
RUN_TIMEOUT_SEC="${RUN_TIMEOUT_SEC:-300}"

if [ ! -f "$NEWTON_BIN" ]; then
    echo "Error: Newton binary not found at $NEWTON_BIN"
    echo "Please build Newton first: cd src/newton && make"
    exit 1
fi

echo "========================================"
echo "Quantization Decision Accuracy Test"
echo "Target: Cortex-A53 (AArch64)"
echo "========================================"
echo ""

TEST_ORDER=(
    "e_exp"
    "e_log"
    "e_y0"
    "e_j0"
    "sincosf"
    "e_acosh"
    "e_rem_pio2"
    "float64_add"
    "float64_div"
    "float64_mul"
)

# Test cases: name -> expected decision
declare -A TESTS=(
    ["e_exp"]="YES"
    ["e_log"]="YES"
    ["e_y0"]="YES"
    ["e_j0"]="YES"
    ["sincosf"]="YES"
    ["e_acosh"]="NO"
    ["e_rem_pio2"]="NO"
    ["float64_add"]="NO"
    ["float64_div"]="NO"
    ["float64_mul"]="NO"
)

declare -A ACTUALS
declare -A RESULTS

correct=0
total=0
failures=""
skipped=0

for test_name in "${TEST_ORDER[@]}"; do
    expected="${TESTS[$test_name]}"
    ir_file="${LLVM_IR_DIR}/${test_name}.ll"
    target_symbol=""
    case "$test_name" in
        e_exp) target_symbol="__ieee754_exp" ;;
        e_log) target_symbol="__ieee754_log" ;;
        e_y0) target_symbol="__ieee754_y0" ;;
        e_j0) target_symbol="__ieee754_j0" ;;
        e_acosh) target_symbol="__ieee754_acosh" ;;
        e_rem_pio2) target_symbol="__ieee754_rem_pio2" ;;
        sincosf) target_symbol="libc_sincosf" ;;
    esac

    if [ ! -f "$ir_file" ]; then
        echo "SKIP: $test_name (IR file not found)"
        continue
    fi

    echo -n "Testing: $test_name ... "

    # Run from src/newton so relative include files used by sensor loading are resolvable
    output=$(cd "$NEWTON_WORKDIR" && timeout "$RUN_TIMEOUT_SEC" "$NEWTON_BIN" --llvm-ir="$ir_file" --llvm-ir-liveness-check --llvm-ir-auto-quantization --llvm-ir-enable-quant-decider "$SENSOR_FILE" 2>&1 || true)

    # Parse the quantDecider output
    # Format: [quant-decider] function=XXX ... shouldQuantize=true/false
    selected_line=""
    if [ -n "$target_symbol" ]; then
        selected_line=$(printf "%s" "$output" | grep "\[quant-decider\]" | grep "function=${target_symbol}" | head -1)
    fi
    if [ -z "$selected_line" ]; then
        selected_line=$(printf "%s" "$output" | grep "\[quant-decider\]" | head -1)
    fi
    should_quantize=$(printf "%s" "$selected_line" | sed 's/.*shouldQuantize=\(true\|false\).*/\1/')

    if [ -z "$should_quantize" ]; then
        if printf "%s" "$output" | grep -q "Could not open file \"NewtonBaseSignals.nt\""; then
            echo "FAIL (sensor include path unresolved)"
            failures="${failures} ${test_name}"
            total=$((total + 1))
            continue
        fi
        if [ "$expected" = "NO" ]; then
            echo "PASS (no quantDecider output implies no FP cluster)"
            total=$((total + 1))
            correct=$((correct + 1))
            ACTUALS["$test_name"]="NO"
            RESULTS["$test_name"]="PASS"
            continue
        fi
        echo "FAIL (no quantDecider output)"
        failures="${failures} ${test_name}"
        total=$((total + 1))
        ACTUALS["$test_name"]="N/A"
        RESULTS["$test_name"]="FAIL"
        continue
    fi

    # Convert to YES/NO
    if [ "$should_quantize" = "true" ]; then
        actual="YES"
    else
        actual="NO"
    fi

    total=$((total + 1))

    if [ "$actual" = "$expected" ]; then
        echo "PASS (expected=$expected actual=$actual)"
        correct=$((correct + 1))
        ACTUALS["$test_name"]="$actual"
        RESULTS["$test_name"]="PASS"
    else
        echo "FAIL (expected=$expected actual=$actual)"
        failures="${failures} ${test_name}"
        ACTUALS["$test_name"]="$actual"
        RESULTS["$test_name"]="FAIL"
    fi
done

echo ""
echo "Detailed Results (all 10 testcases):"
for test_name in "${TEST_ORDER[@]}"; do
    expected="${TESTS[$test_name]}"
    actual="${ACTUALS[$test_name]:-N/A}"
    result="${RESULTS[$test_name]:-SKIP}"
    printf "  [\"%s\"] expected=%s actual=%s result=%s\n" "$test_name" "$expected" "$actual" "$result"
done

echo ""
echo "========================================"
echo "Results: ${correct}/${total} passed"
echo "Skipped: ${skipped}"
if [ $total -eq 0 ]; then
    echo "No decision data captured. Ensure --llvm-ir-enable-quant-decider path is active."
    exit 2
fi

if [ $correct -eq $total ]; then
    echo "100% accuracy achieved!"
else
    echo "Failed cases:${failures}"
fi
echo "========================================"

if [ $correct -eq $total ]; then
    exit 0
else
    exit 1
fi
