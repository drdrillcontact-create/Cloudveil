#!/usr/bin/env bash
# Starts the installer the way a user starts it and lets it go through itself: with
# --self-test it takes its window by a corner and by its edges and drags it to other sizes,
# moves it, clicks Next, Next and Finish, and so installs the plug-in into the standard folders
# (see SelfTest in Installer/Source/InstallerMain.cpp). Meanwhile this script photographs the
# screen, page by page. Afterwards, what was installed is compared with what the build made.
#
# Usage: run-installer.sh <installer program> <label> <folder for the pictures> <built>=<installed> [...]
# (No commas in the label: a comma ends the title of a note on the run page.)
#
# The installer is started from the folder it lies in, as a double click starts it. With
# OUT_OF_REACH=<folder> that folder (the build) is put out of its reach meanwhile: what it
# installs can then only have come out of the installer itself.
set -o pipefail
here="$(cd "$(dirname "$0")" && pwd)"
source "$here/alone.sh"
installer="$(cd "$(dirname "$1")" 2> /dev/null && pwd)/$(basename "$1")"
label="$2"
pictures="$(pwd)/$3"
shift 3

log="$(pwd)/installer-self-test.log"
rm -rf "$pictures" "$log"
mkdir -p "$pictures"
: > "$log"

if [ ! -f "$installer" ]; then
  echo "::error title=There is no installer to run ($label)::There is no file $installer"
  exit 1
fi

home="$(home_of "$installer")"
echo "alone: the installer is started in \"$home\"; with it there: $(ls -A "$home" | tr '\n' ';')" >> "$log"

if [ -n "$OUT_OF_REACH" ]; then
  if put_out_of_reach "$OUT_OF_REACH" >> "$log"; then
    trap 'bring_back "$OUT_OF_REACH"' EXIT
  else
    # (Something on the machine holds the folder open. The installer is still tried by itself
    # in its own folder; only this one precaution is missing, and the run page says so.)
    echo "::warning title=The build could not be moved out of the installer's reach ($label)::The folder \"$OUT_OF_REACH\" is in use. The installer was still run on its own, from $home."
    OUT_OF_REACH=""
  fi
fi

(cd "$home" && exec "$installer" --self-test "$pictures" --log "$log" --page-seconds "${INSTALLER_PAGE_SECONDS:-12}") &
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

if [ -n "$OUT_OF_REACH" ]; then
  bring_back "$OUT_OF_REACH" >> "$log" || status=1
  trap - EXIT
fi

ok=1
[ "$status" -eq 0 ] || { ok=0; echo "the installer ended with exit code $status" >> "$log"; }
tr -d '\r' < "$log" | grep -aq '^self-test: ALL PASSED' || { ok=0; echo "the installer's self-test did not end with ALL PASSED" >> "$log"; }

# The pixels of the first page, as the installer had them on this machine, have to be the
# design's: the two sums over them have to agree (see Installer/Design/make-assets.py).
sum_file="$here/../../Installer/Assets/picture.sum"
if [ -f "$sum_file" ]; then
  design_sum="$(tr -d ' \r\n' < "$sum_file")"
  shown_sum="$(tr -d '\r' < "$log" | sed -n "s/^self-test: the design's pixels sum to //p" | tail -n 1)"

  if [ -n "$design_sum" ] && [ "$shown_sum" = "$design_sum" ]; then
    echo "compare: [PASS] the first page is made of the design's pixels: both sum to $design_sum" >> "$log"
  else
    ok=0
    echo "compare: [FAIL] the first page is not made of the design's pixels: they sum to \"$shown_sum\", the design's to \"$design_sum\"" >> "$log"
  fi
fi

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
  bash "$here/annotate.sh" "The installer installed the plug-in ($label)" "$log" 'PASS|FAIL|SKIP|-> |alone|screen|window at|installed file|pictures' notice
  exit 0
fi

bash "$here/annotate.sh" "The installer failed ($label)" "$log"
exit 1
