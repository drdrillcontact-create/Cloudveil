#!/bin/bash
# Builds Tests/InstallerHarness.cpp together with the installer's own sources and runs it: the
# real install routine (into temporary folders) and the real pages, without a display.
#
#   Tests/run-installer-harness.sh [folder]     the optional folder receives a picture of every page
#   Tests/run-installer-harness.sh --program <payload.zip> [options]
#                                               builds the installer program itself and starts it with
#                                               <payload.zip> as the plug-in it carries, e.g. with
#                                               --self-test <folder> or --silent --vst3-dir <folder>
#                                               (on a machine without a screen there is no window)
#
# Needs Linux with git and g++; see Tests/local-juce.sh. The first run compiles JUCE.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$HERE/.."
source "$HERE/local-juce.sh"
local_juce_setup

OUT="$LOCAL_ROOT/installer-harness"
mkdir -p "$OUT"

# The names and the version, as CMakeLists.txt states them (what configure_file() does in the build)
sed -e "s/@DALUNE_PRODUCT_NAME@/$PRODUCT_NAME/" -e "s/@DALUNE_COMPANY_NAME@/$COMPANY_NAME/" -e "s/@PROJECT_VERSION@/$PRODUCT_VERSION/" \
    "$ROOT/Installer/InstallerBuildConfig.h.in" > "$OUT/InstallerBuildConfig.h"

# What juce_add_binary_data() makes from Installer/Assets in the real build: the files as data
# in the program, under the same names.
{
    echo "#pragma once"
    echo "namespace InstallerData {"
    for f in "$ROOT"/Installer/Assets/portrait.jpg "$ROOT"/Installer/Assets/*.ttf; do
        id="$(basename "$f" | tr '.' '_')"
        echo "    extern const char* $id; extern const int ${id}Size;"
    done
    echo "}"
} > "$OUT/InstallerData.h"

{
    echo '#include "InstallerData.h"'
    n=0
    for f in "$ROOT"/Installer/Assets/portrait.jpg "$ROOT"/Installer/Assets/*.ttf; do
        id="$(basename "$f" | tr '.' '_')"
        echo "__asm__ (\".section .rodata\\n.global dalune_asset_${n}\\ndalune_asset_${n}:\\n.incbin \\\"$f\\\"\\n.global dalune_asset_${n}_end\\ndalune_asset_${n}_end:\\n.byte 0\\n.previous\\n\");"
        echo "extern \"C\" const char dalune_asset_${n}[], dalune_asset_${n}_end[];"
        echo "namespace InstallerData { const char* $id = dalune_asset_${n}; const int ${id}Size = (int) (dalune_asset_${n}_end - dalune_asset_${n}); }"
        n=$((n + 1))
    done
} > "$OUT/InstallerData.cpp"

# The harness, or the installer program itself
if [ "$1" = "--program" ]; then
    MAIN="$ROOT/Installer/Source/InstallerMain.cpp $ROOT/Installer/Source/SystemMouse.cpp"
    BINARY="$OUT/installer-program"
    OTHER="InstallerHarness.cpp.o"
else
    MAIN="$HERE/InstallerHarness.cpp"
    BINARY="$OUT/installer-harness"
    OTHER="InstallerMain.cpp.o SystemMouse.cpp.o"
fi

pids=""
for f in "$ROOT/Installer/Source/InstallerEngine.cpp" "$ROOT/Installer/Source/InstallerAssets.cpp" \
         "$ROOT/Installer/Source/InstallerView.cpp" "$OUT/InstallerData.cpp" $MAIN; do
    g++ $JUCE_CXXFLAGS -I"$ROOT/Installer/Source" -I"$OUT" -c "$f" -o "$OUT/$(basename "$f").o" &
    pids="$pids $!"
done
for pid in $pids; do wait "$pid"; done

(cd "$OUT" && rm -f $OTHER)
g++ "$OUT"/*.o "$JUCE_OBJ"/*.o $JUCE_LIBS -rdynamic -o "$BINARY"

# JUCE's leak detector complains at exit because the harness leaves without a full shutdown
if [ "$1" = "--program" ]; then
    payload="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
    shift 2
    DALUNE_INSTALLER_PAYLOAD="$payload" "$BINARY" "$@" 2> >(grep -v "Leaked objects\|juce_LeakedObjectDetector\|juce_Singleton" >&2)
else
    cd "$ROOT" && "$BINARY" "$@" 2> >(grep -v "Leaked objects\|juce_LeakedObjectDetector\|juce_Singleton" >&2)
fi
