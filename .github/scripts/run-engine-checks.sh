#!/usr/bin/env bash
# Runs the engine checks (Tests/EngineChecks.cpp, built by CMake) and reports on the run page.
#
# Usage: run-engine-checks.sh <command that runs the checks> <label>
# (No commas in the label: a comma ends the title of a note on the run page.)
set -o pipefail
here="$(cd "$(dirname "$0")" && pwd)"
command="$1"
label="$2"
log="engine-checks-$(echo "$label" | tr -c 'A-Za-z0-9\n' '-').log"

# (the command may be two words, as in "arch -x86_64 <program>")
if $command 2>&1 | tee "$log"; then
  bash "$here/annotate.sh" "Engine checks passed ($label)" "$log" 'ALL PASSED|the octave comes out|nothing else to speak of|never makes the tail shorter' notice
  exit 0
fi

bash "$here/annotate.sh" "Engine checks failed ($label)" "$log" 'FAIL|rror'
exit 1
