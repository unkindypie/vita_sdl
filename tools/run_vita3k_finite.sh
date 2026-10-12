#!/bin/sh
# Qt returns to its library after guest exit. Finish finite benchmarks only
# after observing this run's guest completion, then close our own emulator PID.
set -eu
stage_log=$1
shift
initial_bytes=0
if [ -f "$stage_log" ]; then
    initial_bytes=$(wc -c < "$stage_log")
fi
"$@" &
emulator_pid=$!
cleanup() {
    if kill -0 "$emulator_pid" 2>/dev/null; then
        kill -TERM "$emulator_pid" 2>/dev/null || true
        sleep 2
        # This Qt build can ignore TERM after returning to its library. The
        # guest is already shut down on success; never signal another process.
        if kill -0 "$emulator_pid" 2>/dev/null; then
            kill -KILL "$emulator_pid" 2>/dev/null || true
        fi
    fi
    wait "$emulator_pid" 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT TERM
check_completion() {
    if [ -f "$stage_log" ]; then
        current_run=$(tail -c "+$((initial_bytes + 1))" "$stage_log")
        if printf '%s\n' "$current_run" | grep -q '^SHUTDOWN_OK'; then
            printf '%s\n' "$current_run"
            if printf '%s\n' "$current_run" | grep -q '^SMOKE_OK'; then
                exit 0
            fi
            echo 'Vita3K finite run ended without SMOKE_OK' >&2
            exit 1
        fi
    fi
}
elapsed=0
while :; do
    check_completion
    # SDL may exit between polls after flushing successful guest markers.
    # Read those markers before treating process exit as a failed run.
    if ! kill -0 "$emulator_pid" 2>/dev/null; then
        # The child may have written its markers between our read and kill -0.
        check_completion
        echo 'Vita3K exited before clean guest completion' >&2
        exit 1
    fi
    if [ "$elapsed" -ge "${VITA3K_BENCH_TIMEOUT:-180}" ]; then
        echo 'Vita3K finite run timed out before clean guest completion' >&2
        exit 1
    fi
    sleep 1
    elapsed=$((elapsed + 1))
done
