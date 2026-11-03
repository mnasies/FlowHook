#!/usr/bin/env bash
set -uo pipefail

PASS=0
FAIL=0

source ./test_helpers.sh

# test_check.sh

test_check_reports_initialized() {
  fresh_dir
  flowhook init >/dev/null 2>&1
  local out code
  out=$(flowhook check 2>&1)
  code=$?
  assert_exit 0 "$code" "check in initialized dir exits 0"
  assert_contains "$out" "is initialized" "check reports initialized after init"
}

test_check_reports_not_initialized() {
  fresh_dir
  local out code
  out=$(flowhook check 2>&1)
  code=$?
  assert_exit 0 "$code" "check in uninitialized dir exits 0"
  assert_contains "$out" "has not been initialized" "check reports not initialized without init"
}

# regression: an initialized task in dir A must still check fine when ANOTHER
# task's working dir gets renamed (lazy mode must not hard-fail on the
# renamed neighbor)
test_check_survives_renamed_neighbor_dir() {
  fresh_dir
  flowhook init >/dev/null 2>&1
  local dir_a
  dir_a=$(pwd)
  mkdir ../neighbor
  cd ../neighbor || exit 1
  flowhook init >/dev/null 2>&1
  # now move the neighbor's directory out from under it, dir A untouched
  mv "$dir_a/../neighbor" "$dir_a/../neighbor_renamed"
  cd "$dir_a" || exit 1
  local out code
  out=$(flowhook check 2>&1)
  code=$?
  assert_exit 0 "$code" "check exits 0 with renamed neighbor dir"
  assert_contains "$out" "is initialized" "check still reports initialized with renamed neighbor"
}

# read-only values must be served from config in lazy mode
test_check_depth_and_timeout_from_config() {
  fresh_dir
  flowhook init >/dev/null 2>&1
  flowhook set --depth 3 --timeout 42 >/dev/null 2>&1
  local out code
  out=$(flowhook check --depth 2>&1)
  code=$?
  assert_exit 0 "$code" "check --depth exits 0"
  assert_contains "$out" "3" "check --depth reports persisted depth"
  out=$(flowhook check --timeout 2>&1)
  code=$?
  assert_exit 0 "$code" "check --timeout exits 0"
  assert_contains "$out" "42" "check --timeout reports persisted timeout"
}

test_check_active_after_set_active() {
  fresh_dir
  flowhook init >/dev/null 2>&1
  flowhook set --active >/dev/null 2>&1
  local out code
  out=$(flowhook check --active 2>&1)
  code=$?
  assert_exit 0 "$code" "check --active exits 0"
  assert_contains "$out" "is active" "check --active reports persisted active flag"
}
