#!/bin/bash
#
# run-flycast.sh -- launch a disc image in Flycast, retrying past its startup bug.
#
#   tools/run-flycast.sh <disc.cdi> <logfile> [settle-seconds]
#
# Flycast on macOS fails to initialise roughly half the time with
#
#   Verify Failed : &mem_b[0] == ((u8*)getContext()->sq_buffer + ...)
#    in Init -> core/hw/sh4/dyna/driver.cpp : 349
#
# and exits 6. It depends on where ASLR places things, not on the disc image,
# and Dynarec.Enabled makes no difference -- the check runs before dynarec
# matters. So: try again.
#
# Liveness is checked by PID, not by `pgrep -f Flycast`. That pattern also
# matches the launching shell and any command line mentioning Flycast, so it
# reports success for a process that already died and the retry never happens.
#
# stdout is captured, which is where KOS's printf lands once Flycast's serial
# console is enabled.

set -u

DISC="${1:?usage: run-flycast.sh <disc.cdi> <logfile> [settle-seconds]}"
LOG="${2:?usage: run-flycast.sh <disc.cdi> <logfile> [settle-seconds]}"
SETTLE="${3:-8}"
BIN=/Applications/Flycast.app/Contents/MacOS/Flycast
ATTEMPTS=15

pkill -x Flycast 2>/dev/null
python3 -c "import time; time.sleep(3)"

for attempt in $(seq 1 $ATTEMPTS); do
	: > "$LOG"
	"$BIN" "$DISC" > "$LOG" 2>&1 &
	pid=$!

	python3 -c "import time; time.sleep($SETTLE)"

	if kill -0 "$pid" 2>/dev/null; then
		echo "flycast running (pid $pid, attempt $attempt)"
		exit 0
	fi

	if grep -q "Verify Failed" "$LOG" 2>/dev/null; then
		echo "attempt $attempt: VMEM assertion, retrying"
	else
		echo "attempt $attempt: exited early, retrying"
	fi
done

echo "failed to start Flycast after $ATTEMPTS attempts" >&2
exit 1
