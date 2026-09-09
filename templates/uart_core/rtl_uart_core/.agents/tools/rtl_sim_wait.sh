#!/usr/bin/env bash
# Quiet deterministic wrapper for long-running RTL simulation/regression commands.
#
# The wrapped command's console output is redirected to a temporary log so a long
# run does not continuously stream simulator text into the model context. A tiny
# background monitor checks child-process liveness at a fixed interval (30 s by
# default) and intentionally emits nothing. There is NO wall-clock timeout.
#
# Usage:
#   .agents/tools/rtl_sim_wait.sh [--poll-seconds N] [--log PATH] -- command [args...]
#
# Environment override:
#   RTL_SIM_POLL_SECONDS=N

set -u

poll_seconds="${RTL_SIM_POLL_SECONDS:-30}"
log_path=""

usage() {
    cat <<'USAGE'
Usage: rtl_sim_wait.sh [--poll-seconds N] [--log PATH] -- command [args...]

Runs command quietly, checks only whether the child process is still alive at the
configured interval, and returns when the command exits. No wall-clock timeout is
applied. The wrapped command's stdout/stderr is saved to a log file.
USAGE
}

while (($# > 0)); do
    case "$1" in
        --poll-seconds)
            (($# >= 2)) || { echo "rtl_sim_wait: missing value for --poll-seconds" >&2; exit 2; }
            poll_seconds="$2"
            shift 2
            ;;
        --log)
            (($# >= 2)) || { echo "rtl_sim_wait: missing value for --log" >&2; exit 2; }
            log_path="$2"
            shift 2
            ;;
        --)
            shift
            break
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "rtl_sim_wait: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

(($# > 0)) || { echo "rtl_sim_wait: no command supplied" >&2; usage >&2; exit 2; }

if ! [[ "$poll_seconds" =~ ^[1-9][0-9]*$ ]]; then
    echo "rtl_sim_wait: poll interval must be a positive integer number of seconds" >&2
    exit 2
fi

if [[ -z "$log_path" ]]; then
    log_path="$(mktemp "${TMPDIR:-/tmp}/rtl_flow_sim_wait.XXXXXX.log")" || exit 2
else
    mkdir -p "$(dirname "$log_path")" || exit 2
    : > "$log_path" || exit 2
fi

start_epoch="$(date +%s)"

"$@" >"$log_path" 2>&1 &
child_pid=$!
monitor_pid=""

cleanup_monitor() {
    if [[ -n "${monitor_pid:-}" ]]; then
        kill "$monitor_pid" 2>/dev/null || true
        wait "$monitor_pid" 2>/dev/null || true
    fi
}

forward_int() {
    kill -INT "$child_pid" 2>/dev/null || true
}

forward_term() {
    kill -TERM "$child_pid" 2>/dev/null || true
}

trap forward_int INT
trap forward_term TERM HUP
trap cleanup_monitor EXIT

# Deterministic, model-free liveness check. It deliberately produces no output,
# reads no transcript, and makes no progress/hang inference.
(
    while kill -0 "$child_pid" 2>/dev/null; do
        sleep "$poll_seconds"
    done
) &
monitor_pid=$!

wait "$child_pid"
exit_status=$?
end_epoch="$(date +%s)"
elapsed_seconds=$((end_epoch - start_epoch))

cleanup_monitor
monitor_pid=""
trap - EXIT

printf 'RTL_SIM_WAIT_DONE exit_status=%d elapsed_seconds=%d console_log=%s\n' \
    "$exit_status" "$elapsed_seconds" "$log_path"

exit "$exit_status"
