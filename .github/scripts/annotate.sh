#!/usr/bin/env bash
# Puts the telling lines of a log on the workflow run page as an annotation, so that the outcome
# of a build or validation step can be read without opening the full log.
#
#   annotate.sh <title> <log file> [<grep -E pattern>] [<level>]
#
# Defaults: every line, level "error". The last 60 selected lines are shown.
title="$1"
log="$2"
pattern="${3:-.}"
level="${4:-error}"

body="$(tr -d '\r' < "$log" | grep -aE "$pattern" | tail -n 60 | cut -c1-220 | sed -e 's/%/%25/g' | awk '{ printf "%s%%0A", $0 }')"
echo "::${level} title=${title}::${body:-no matching lines in ${log}}"
