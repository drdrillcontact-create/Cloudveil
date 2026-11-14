#!/usr/bin/env bash
# Puts the telling lines of a log on the workflow run page as an error annotation, so that the
# reason for a failed build or validation can be read without opening the full log.
#
#   annotate-failure.sh <title> <log file> [<grep -E pattern>]     (default: the last 40 lines)
title="$1"
log="$2"
pattern="${3:-.}"

body="$(tr -d '\r' < "$log" | grep -aE "$pattern" | tail -n 40 | cut -c1-240 | sed -e 's/%/%25/g' | awk '{ printf "%s%%0A", $0 }')"
echo "::error title=${title}::${body:-no matching lines in ${log}}"
