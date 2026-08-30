#!/bin/bash
# loadtrial.sh <disc.cdi> <label> <runs> -- controlled level-load timing.
#
# Deletes the emulated VMU before every run. Card-stored preferences override
# default_sound_manager_parameters, and measuring the sound stage without
# clearing the card is what made three earlier runs disagree by 20 seconds
# (0a12774 documents the same trap).
#
# Also re-asserts Debug.SerialConsoleEnabled, which Flycast drops from emu.cfg
# when it quits -- a run with no dctrace lines looks like a hang and is not.

set -u
DISC="$1"; LABEL="$2"; RUNS="${3:-3}"
S=/private/tmp/claude-501/-Users-mgibbons-Desktop-Marathon-DC/5161da89-557e-45c1-a7f2-aebfaf8380ce/scratchpad
CFG="$HOME/Library/Application Support/Flycast/emu.cfg"
DATA="$HOME/Library/Application Support/Flycast/data"

for i in $(seq 1 "$RUNS"); do
	pkill -f run-flycast.sh 2>/dev/null
	pkill -x Flycast 2>/dev/null
	python3 -c "import time; time.sleep(3)"

	rm -f "$DATA"/*vmu_save*.bin
	grep -q 'Debug.SerialConsoleEnabled = yes' "$CFG" || \
		python3 - "$CFG" <<'PY'
import io,sys
p=sys.argv[1]; d=io.open(p,encoding='latin-1').read()
if 'Debug.SerialConsoleEnabled' in d:
    d=d.replace('Debug.SerialConsoleEnabled = no','Debug.SerialConsoleEnabled = yes')
else:
    d=d.replace('[config]','[config]\nDebug.SerialConsoleEnabled = yes',1)
io.open(p,'w',encoding='latin-1').write(d)
PY

	LOG="$S/${LABEL}_$i.log"
	tools/run-flycast.sh "$DISC" "$LOG" 20 >/dev/null 2>&1 &
	python3 -c "import time; time.sleep(1)"
	echo "--- $LABEL run $i ---"
	python3 "$S/timeload.py" "$LOG" 200 | tail -2
	python3 - "$LOG" <<'PY'
import io,re,sys
d=io.open(sys.argv[1],'rb').read().decode('latin-1')
for h in re.findall(r'load: (collections|monster sounds|game sounds) (\d+) ms', d):
    print('    %-16s %6s ms' % h)
if not re.search(r'load: collections', d):
    print('    STALLED: no collections trace')
PY
done
pkill -f run-flycast.sh 2>/dev/null; pkill -x Flycast 2>/dev/null
