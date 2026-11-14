#!/bin/bash
# Builds Tests/PluginHarness.cpp together with the plug-in's own sources and runs it: the real
# processor and editor, without a host and without a display.
#
#   Tests/run-harness.sh [snapshot.png]     the optional file receives a picture of the editor
#
# Needs Linux with git and g++; see Tests/local-juce.sh. The first run compiles JUCE.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
source "$HERE/local-juce.sh"
local_juce_setup

OUT="$LOCAL_ROOT/harness"
mkdir -p "$OUT"

# What juce_add_plugin() would define
PLUGIN_DEFS="-DJucePlugin_Name=\"Cloudveil\" -DJucePlugin_Desc=\"Cloudveil\" -DJucePlugin_Manufacturer=\"CloudveilAudio\" -DJucePlugin_VersionString=\"1.1.0\" -DJucePlugin_IsSynth=0 -DJucePlugin_WantsMidiInput=0 -DJucePlugin_ProducesMidiOutput=0 -DJucePlugin_IsMidiEffect=0"

pids=""
for f in "$HERE/../Source/PluginProcessor.cpp" "$HERE/../Source/PluginEditor.cpp" "$HERE/PluginHarness.cpp" $EXTRA_SOURCES; do
    g++ $JUCE_CXXFLAGS $PLUGIN_DEFS -I"$HERE/../Source" $EXTRA_FLAGS -c "$f" -o "$OUT/$(basename "$f").o" &
    pids="$pids $!"
done
for pid in $pids; do wait "$pid"; done

g++ "$OUT"/*.o "$JUCE_OBJ"/*.o $JUCE_LIBS -rdynamic -o "$OUT/harness"

# JUCE's leak detector complains at exit because the harness leaves without a full shutdown
cd "$HERE/.." && "$OUT/harness" "$@" 2> >(grep -v "Leaked objects\|juce_LeakedObjectDetector\|juce_Singleton" >&2)
