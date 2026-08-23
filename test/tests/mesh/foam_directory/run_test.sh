#!/usr/bin/env bash
# Wrapper for test/tests/mesh/foam_directory — mirrors tests file behavior.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODE="${1:-all}"

cd "${SCRIPT_DIR}"

step() { echo; echo "=== $* ==="; }

run_setup() {
  step "setup: creating malformed foam directories"
  mkdir -p foam_no_0/system foam_no_0/constant
  mkdir -p foam_no_constant/system foam_no_constant/0
  mkdir -p foam_no_system/0 foam_no_system/constant
}

expect_failure() {
  local label="$1"
  local expected="$2"
  shift 2
  step "${label}: expecting failure"
  set +e
  local output
  output="$("$@" 2>&1)"
  local status=$?
  set -e
  echo "$output"

  if [[ $status -eq 0 ]]; then
    echo "Expected failure for ${label}, but command succeeded."
    exit 1
  fi

  if [[ "$output" != *"$expected"* ]]; then
    echo "Expected error message not found for ${label}."
    echo "Wanted substring: $expected"
    exit 1
  fi
}

run_foam_invalid() {
  expect_failure "foam_invalid" "'foam1' is not a directory." \
    hippo-opt -i main.i --allow-test-objects Mesh/case=foam1
}

run_no_0() {
  expect_failure "no_0" "'foam_no_0' must have directories '0', 'constant', and 'system'." \
    hippo-opt -i main.i --allow-test-objects Mesh/case=foam_no_0
}

run_no_system() {
  expect_failure "no_system" "'foam_no_system' must have directories '0', 'constant', and 'system'." \
    hippo-opt -i main.i --allow-test-objects Mesh/case=foam_no_system
}

run_no_constant() {
  expect_failure "no_constant" "'foam_no_constant' must have directories '0', 'constant', and 'system'." \
    hippo-opt -i main.i --allow-test-objects Mesh/case=foam_no_constant
}

run_all() {
  run_setup
  run_foam_invalid
  run_no_0
  run_no_system
  run_no_constant
}

case "$MODE" in
  all)
    run_all
    ;;
  setup)
    run_setup
    ;;
  foam_invalid)
    run_foam_invalid
    ;;
  no_0)
    run_no_0
    ;;
  no_system)
    run_no_system
    ;;
  no_constant)
    run_no_constant
    ;;
  *)
    echo "Usage: $0 [all|setup|foam_invalid|no_0|no_system|no_constant]"
    exit 1
    ;;
esac

echo
echo "=== ALL STEPS PASSED ==="
