#!/usr/bin/env bash
set -uo pipefail

PASS=0
FAIL=0

source ./test_helpers.sh
# test_run.sh

test_run_default_starts_and_exits_on_sigint() {
  fresh_dir
  flowhook init >/dev/null 2>&1
  flowhook add --command "echo hi" >/dev/null 2>&1

  local outfile
  outfile=$(mktemp)

  flowhook run > "$outfile" 2>&1 &
  local pid=$!

  sleep 1
  kill -INT "$pid" 2>/dev/null

  # Give it up to 3 seconds to exit gracefully; force-kill if it doesn't.
  local waited=0
  while kill -0 "$pid" 2>/dev/null && [[ $waited -lt 3 ]]; do
    sleep 1
    waited=$((waited+1))
  done
  if kill -0 "$pid" 2>/dev/null; then
    kill -9 "$pid" 2>/dev/null
    FAIL=$((FAIL+1))
    echo "FAIL: run did not exit on SIGINT within 3s, force-killed"
  else
    PASS=$((PASS+1))
  fi

  local out
  out=$(cat "$outfile")
  assert_contains "$out" "Exiting safely" "run prints clean exit message on SIGINT"
  rm -f "$outfile"
}

test_run_all_starts_and_exits_on_sigint() {
  fresh_dir
  flowhook init >/dev/null 2>&1
  flowhook add --command "echo hi" >/dev/null 2>&1

  local outfile
  outfile=$(mktemp)

  flowhook run --all > "$outfile" 2>&1 &
  local pid=$!

  sleep 1
  kill -INT "$pid"
  wait "$pid" 2>/dev/null

  local out
  out=$(cat "$outfile")
  assert_contains "$out" "Exiting safely" "run --all exits cleanly on SIGINT"
  rm -f "$outfile"
}

test_run_active_with_no_active_tasks_errors() {
  fresh_dir
  flowhook init >/dev/null 2>&1
  flowhook add --command "echo hi" >/dev/null 2>&1
  # never set --active

  local out code
  out=$(flowhook run --active 2>&1)
  code=$?
  assert_contains "$out" "Error: no active tasks to start" "run --active with no active tasks errors"
}

test_run_active_with_active_task_starts_and_exits() {
  fresh_dir
  flowhook init >/dev/null 2>&1
  flowhook add --command "echo hi" >/dev/null 2>&1
  flowhook set --active >/dev/null 2>&1

  local outfile
  outfile=$(mktemp)

  flowhook run --active > "$outfile" 2>&1 &
  local pid=$!

  sleep 1
  kill -INT "$pid"
  wait "$pid" 2>/dev/null

  local out
  out=$(cat "$outfile")
  assert_contains "$out" "Exiting safely" "run --active with an active task exits cleanly on SIGINT"
  rm -f "$outfile"
}

test_run_quiet_suppresses_command_output() {
  fresh_dir
  flowhook init >/dev/null 2>&1
  flowhook add --command "echo SHOULD_NOT_APPEAR" >/dev/null 2>&1

  local outfile
  outfile=$(mktemp)
  flowhook run --quiet > "$outfile" 2>&1 &
  local pid=$!
  sleep 1
  kill -INT "$pid"
  wait "$pid" 2>/dev/null

  local out
  out=$(cat "$outfile")
  assert_not_contains "$out" "SHOULD_NOT_APPEAR" "run --quiet suppresses build output"
  rm -f "$outfile"
}

test_run_all_skips_degraded_task() {
  fresh_dir
  flowhook init >/dev/null 2>&1
  flowhook add --command "echo hi" >/dev/null 2>&1

  local dir_a
  dir_a=$(pwd)
  local neighbor="${dir_a}_neighbor"
  local neighbor_vanished="${dir_a}_neighbor_vanished"
  rm -rf "$neighbor" "$neighbor_vanished"
  mkdir -p "$neighbor"
  cd "$neighbor" || exit 1
  flowhook init >/dev/null 2>&1
  flowhook add --command "echo hello" >/dev/null 2>&1

  # Move neighbor dir away so it becomes stale/degraded
  mv "$neighbor" "$neighbor_vanished"

  cd "$dir_a" || exit 1

  local outfile
  outfile=$(mktemp)

  flowhook run --all > "$outfile" 2>&1 &
  local pid=$!

  sleep 1
  kill -INT "$pid"
  wait "$pid" 2>/dev/null

  local out
  out=$(cat "$outfile")
  assert_contains "$out" "Exiting safely" "run --all with degraded task starts and exits cleanly on SIGINT"
  rm -f "$outfile"
  rm -rf "$neighbor_vanished"
}

