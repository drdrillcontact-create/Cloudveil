#!/usr/bin/env bash
# Starts the installer the way a user starts it and lets it go through itself: with
# --self-test it takes its window by a corner and by its edges and drags it to other sizes,
# moves it, clicks Next, Next and Finish, and so installs the plug-in into the standard folders
# (see SelfTest in Installer/Source/InstallerMain.cpp). Meanwhile this script photographs the
# screen, page by page. Afterwards, what was installed is compared with what the build made.
#
# Usage: run-installer.sh <installer program> <label> <folder for the pictures> <built>=<installed> [...]
# (No commas in the label: a comma ends the title of a note on the run page.)
set -o pipefail
here="$(cd "$(dirname "$0")" && pwd)"
installer="$1"
label="$2"
pictures="$3"
shift 3

log="installer-self-test.log"
rm -rf "$pictures" "$log"
mkdir -p "$pictures"
: > "$log"

if [ ! -f "$installer" ]; then
  echo "::error title=The installer was not built ($label)::There is no file $installer"
  exit 1
fi

"$installer" --self-test "$pictures" --log "$log" --page-seconds "${INSTALLER_PAGE_SECONDS:-12}" &
pid=$!

# Waits for a line in the installer's log; gives up when the installer has ended without it
wait_for_line() {
  local deadline=$((SECONDS + $2))
  while [ "$SECONDS" -lt "$deadline" ]; do
    tr -d '\r' < "$log" | grep -aq -- "$1" && return 0
    kill -0 "$pid" 2> /dev/null || return 1
    sleep 0.2
  done
  return 1
}

# The installer says when a page is up (and then leaves it up for some seconds), and where on
# the screen its window is. The last page is "finished", or "failed" if it could not install.
for page in welcome location 'f[a-z]*ed'; do
  if wait_for_line "^self-test: showing $page\$" 90; then
    sleep 0.5
    shown="$(tr -d '\r' < "$log" | sed -n 's/^self-test: showing //p' | tail -n 1)"
    where="$(tr -d '\r' < "$log" | sed -n 's/^self-test: window at //p' | tail -n 1)"
    # shellcheck disable=SC2086
    bash "$here/capture-screen.sh" "$pictures/screen-$shown.png" $where \
      || echo "(the screen could not be photographed for the page \"$shown\")"

    # The first page once more, in two halves: each is small enough to be put on the run page
    # as text at full size (see annotate-picture.sh)
    if [ "$shown" = "welcome" ] && [ -n "$where" ]; then
      for half in left right; do
        # shellcheck disable=SC2086
        bash "$here/capture-screen.sh" "$pictures/screen-$shown-$half.png" $where "$half" || true
      done
    fi
  fi
done

# It ends by itself: Finish closes it, and it gives itself two minutes for everything. Should it
# hang all the same, it is stopped here rather than holding up the run.
deadline=$((SECONDS + 200))
while kill -0 "$pid" 2> /dev/null && [ "$SECONDS" -lt "$deadline" ]; do
  sleep 0.5
done

if kill -0 "$pid" 2> /dev/null; then
  kill -9 "$pid" 2> /dev/null
  echo "the installer did not end by itself and was stopped" >> "$log"
fi

wait "$pid"
status=$?

ok=1
[ "$status" -eq 0 ] || { ok=0; echo "the installer ended with exit code $status" >> "$log"; }
tr -d '\r' < "$log" | grep -aq '^self-test: ALL PASSED' || { ok=0; echo "the installer's self-test did not end with ALL PASSED" >> "$log"; }

for pair in "$@"; do
  built="${pair%%=*}"
  installed="${pair#*=}"

  if diff -r "$built" "$installed" > installer-diff.log 2>&1; then
    echo "compare: [PASS] what was installed is what was built, file for file: $installed" >> "$log"
  else
    ok=0
    echo "compare: [FAIL] what was installed is not what was built: $installed" >> "$log"
    head -n 12 installer-diff.log >> "$log"
  fi

  # (which files those are, and how large)
  if [ -d "$installed" ]; then
    (cd "$installed" && find . -type f | LC_ALL=C sort | while read -r file; do
      echo "installed file: ${file#./}  ($(wc -c < "$file" | tr -d ' ') bytes)"
    done) | head -n 12 >> "$log"
  fi
done

echo "pictures: $(ls "$pictures" 2> /dev/null | tr '\n' ' ')" >> "$log"
cat "$log"

if [ "$ok" -eq 1 ]; then
  bash "$here/annotate.sh" "The installer installed the plug-in ($label)" "$log" 'PASS|FAIL|SKIP|-> |screen|window at|installed file|pictures' notice
  exit 0
fi

bash "$here/annotate.sh" "The installer failed ($label)" "$log"
exit 1
