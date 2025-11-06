#!/bin/bash
set -euo pipefail

# Script to compare outputs between old and new config generators
# Tests all major build configurations used in production

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

OLD_SCRIPT="./generate_config.py"
NEW_SCRIPT="./generate_config_new.py"
BASE_CONFIG="./base_config.yml"
DEFAULT_CONTAINER="public.ecr.aws/b0b8h2r4/test-ubuntu:24.04-4c8e1373"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

format_args() {
    for a in "$@"; do
        if [ -z "$a" ]; then
            printf '"" '
        else
            printf '%q ' "$a"
        fi
    done
}

# Function to run comparison test
test_config() {
    local test_name="$1"
    shift
    local args=( "$@" )

    echo ""
    echo "=========================================="
    echo "Testing: $test_name"
    echo "Args: $(format_args "${args[@]}")"
    echo "=========================================="

    local old_output="/tmp/old_${test_name}.yml"
    local new_output="/tmp/new_${test_name}.yml"
    local old_normalized="/tmp/old_${test_name}_normalized.yml"
    local new_normalized="/tmp/new_${test_name}_normalized.yml"

    # Run old script
    echo "Running old generator..."
    echo "python3 \"$OLD_SCRIPT\" -o \"$old_output\" $(format_args "${args[@]}")"
    if ! python3 "$OLD_SCRIPT" -o "$old_output" "${args[@]}" 2>&1 | head -5; then
        echo -e "${RED}ERROR: Old generator failed${NC}"
        ((TESTS_FAILED++))
        return 0  # Continue with other tests
    fi

    # Run new script
    echo "Running new generator..."
    echo "python3 \"$NEW_SCRIPT\" -o \"$new_output\" $(format_args "${args[@]}")"
    if ! python3 "$NEW_SCRIPT" -o "$new_output" "${args[@]}" 2>&1 | head -5; then
        echo -e "${RED}ERROR: New generator failed${NC}"
        ((TESTS_FAILED++))
        return 0  # Continue with other tests
    fi

    # Normalize both outputs for comparison
    echo "Normalizing outputs..."
    python3 "$SCRIPT_DIR/normalize_config.py" "$old_output" "$old_normalized"
    python3 "$SCRIPT_DIR/normalize_config.py" "$new_output" "$new_normalized"

    # Compare normalized outputs
    echo "Comparing normalized outputs..."
    if diff -u "$old_normalized" "$new_normalized" > "/tmp/diff_${test_name}.txt"; then
        echo -e "${GREEN}✓ PASS: Outputs are identical${NC}"
        ((TESTS_PASSED++))
        rm -f "$old_output" "$new_output" "$old_normalized" "$new_normalized" "/tmp/diff_${test_name}.txt"
    else
        # Count the number of actual differences (ignoring line number changes)
        local diff_count=$(grep -c "^[-+]" "/tmp/diff_${test_name}.txt" | head -1)
        echo -e "${RED}✗ FAIL: Outputs differ (${diff_count} changed lines)${NC}"
        echo "Diff saved to: /tmp/diff_${test_name}.txt"
        echo "First 20 lines of diff:"
        head -20 "/tmp/diff_${test_name}.txt"
        ((TESTS_FAILED++))
        # Don't return early - continue with remaining tests
    fi
}

# Make sure we have pyyaml
echo "Installing dependencies..."
pip install -q pyyaml

echo ""
echo "=========================================="
echo "Starting config generator comparison tests"
echo "=========================================="

# Test 1: Default PR build (single test file, all tests)
# Matches the actual CircleCI command line from config.yml
test_config "pr_default" \
    --sanitizer "" \
    --arangosh-args "A" \
    --extra-args "A" \
    --default-container "$DEFAULT_CONTAINER" \
    --ui "on" \
    --ui-deployment "" \
    --ui-testsuites "" \
    --arangod-without-v8 "false" \
    --test-branches "" \
    ${BASE_CONFIG} \
    ../tests/test-definitions.yml || true

# Test 2: Nightly build (multiple test files)
test_config "nightly" \
    --sanitizer "" \
    --arangosh-args "A" \
    --extra-args "A" \
    --default-container "$DEFAULT_CONTAINER" \
    --ui "on" \
    --ui-deployment "" \
    --ui-testsuites "" \
    --arangod-without-v8 "false" \
    --nightly \
    --test-branches "" \
    ${BASE_CONFIG} \
    ../tests/test-definitions.yml \
    ../tests/arangojs-test-definitions.yml \
    ../tests/java-test-definitions.yml || true

# Test 3: Nightly TSAN build
test_config "nightly_tsan" \
    --sanitizer "tsan" \
    --arangosh-args "A" \
    --extra-args "A" \
    --default-container "$DEFAULT_CONTAINER" \
    --ui "off" \
    --ui-deployment "" \
    --ui-testsuites "" \
    --arangod-without-v8 "false" \
    --nightly \
    --test-branches "" \
    ${BASE_CONFIG} \
    ../tests/test-definitions.yml \
    ../tests/arangojs-test-definitions.yml \
    ../tests/java-test-definitions.yml || true

# Test 4: Nightly ASAN/UBSAN build
test_config "nightly_alubsan" \
    --sanitizer "alubsan" \
    --arangosh-args "A" \
    --extra-args "A" \
    --default-container "$DEFAULT_CONTAINER" \
    --ui "off" \
    --ui-deployment "" \
    --ui-testsuites "" \
    --arangod-without-v8 "false" \
    --nightly \
    --test-branches "" \
    ${BASE_CONFIG} \
    ../tests/test-definitions.yml \
    ../tests/arangojs-test-definitions.yml \
    ../tests/java-test-definitions.yml || true

# Test 5: Weekly no-v8 build
test_config "weekly_no_v8" \
    --sanitizer "" \
    --arangosh-args "A" \
    --extra-args "A" \
    --default-container "$DEFAULT_CONTAINER" \
    --ui "off" \
    --ui-deployment "" \
    --ui-testsuites "" \
    --arangod-without-v8 "true" \
    --nightly \
    --test-branches "" \
    ${BASE_CONFIG} \
    ../tests/test-definitions.yml \
    ../tests/arangojs-test-definitions.yml \
    ../tests/java-test-definitions.yml || true

# Print summary
echo ""
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo -e "Tests passed: ${GREEN}${TESTS_PASSED}${NC}"
echo -e "Tests failed: ${RED}${TESTS_FAILED}${NC}"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed. Check diff files in /tmp/${NC}"
    exit 1
fi
