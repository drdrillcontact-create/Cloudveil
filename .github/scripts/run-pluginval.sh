#!/usr/bin/env bash
# Runs pluginval on a plug-in and reports the outcome on the workflow run page.
# pluginval loads the plug-in the way a host does and exercises it: parameters, state, bus
# layouts, the editor, odd buffer sizes and sample rates, allocations on the audio thread.
#
#   run-pluginval.sh <pluginval executable> <plug-in> <label>
set -o pipefail
here="$(cd "$(dirname "$0")" && pwd)"
pluginval="$1"
plugin="$2"
label="$3"

if "$pluginval" --strictness-level 10 --verbose --validate "$plugin" 2>&1 | tee pluginval.log; then
  bash "$here/annotate.sh" "pluginval passed the $label" pluginval.log 'Strictness level|Time taken to run all tests|^SUCCESS' notice
  exit 0
fi

# Which tests failed, then the lines that led up to each failure
{
  tr -d '\r' < pluginval.log | awk '/Starting tests in/ { section = $0 } /!!!/ { print section; print "  " $0 }' | uniq
  echo "--"
  grep -a -B 14 -A 2 '!!!' pluginval.log | head -n 44
} > pluginval-failures.log

bash "$here/annotate.sh" "pluginval rejected the $label" pluginval-failures.log
exit 1
