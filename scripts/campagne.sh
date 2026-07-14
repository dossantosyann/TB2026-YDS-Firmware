#!/bin/zsh
# Flash the board and capture the diagnostics campaign to a timestamped log.
#
# Usage:  zsh scripts/campagne.sh [port]
#
# zsh, not sh: Espressif's activate script decides whether it was sourced by checking
# whether $0 is named sh/bash/dash, which is false inside a POSIX-sh script even when it
# *is* sourced -- it then refuses with "This script should be sourced, not executed".
# Under zsh it reads ZSH_EVAL_CONTEXT instead and gets it right.
#
# The log only exists in the terminal: ESP_LOGI writes to the UART and nothing is
# stored on the device, so the capture has to be set up before the run, not after.
# Requires DIAG_ENABLED=1 in components/services/diag/diag.h.
set -e

PROJECT="$(cd "$(dirname "$0")/.." && pwd)"
IDF_PY="$HOME/.espressif/v6.0.1/esp-idf/tools/idf.py"
PYTHON="$HOME/.espressif/python_env/idf6.0_py3.14_env/bin/python"
LOG="$PROJECT/campagne_$(date +%m%d_%H%M).log"

# export.sh fails on this machine (tools installed by the VSCode EIM), and the build dir
# was configured with a different venv than the activate script's: override it or idf.py
# aborts on a python-env mismatch.
. "$HOME/.espressif/tools/activate_idf_v6.0.1.sh"
export IDF_PYTHON_ENV_PATH="$HOME/.espressif/python_env/idf6.0_py3.14_env"

cd "$PROJECT"

if ! grep -q '^#define DIAG_ENABLED 1' components/services/diag/diag.h; then
    echo "DIAG_ENABLED is 0: the diagnostics module is compiled out, there would be"
    echo "nothing to measure. Set it to 1 in components/services/diag/diag.h first."
    exit 1
fi

PORT_ARG=""
[ -n "$1" ] && PORT_ARG="-p $1"

# Flash resets every high water mark and the min-free-heap counters, so it happens once,
# before the capture — never mid-campaign.
"$PYTHON" "$IDF_PY" $PORT_ARG flash

echo "Capturing to $LOG  (Ctrl-] to stop)"
# caffeinate: a Mac that falls asleep drops the USB port and truncates the capture.
# tee: the log is written while staying readable live.
caffeinate -i "$PYTHON" "$IDF_PY" $PORT_ARG monitor | tee "$LOG"
