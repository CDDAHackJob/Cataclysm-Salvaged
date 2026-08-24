#!/bin/bash

# Script made specifically for compiling without running tests on GitHub Actions, rewritten by claude-opus.

echo "Using bash version $BASH_VERSION"
set -exo pipefail

# Upstream hardcodes 3, sized for GitHub's 2-4 core hosted runners. On the
# self-hosted i7-12700K the binding constraint is HEAT, not wall time, and 4 is
# the last value on the good side of a measured discontinuity: at 4 the scheduler
# migrates work between cores and the die cools between bursts (87 C, zero
# throttle events); at 6 it parks the load on the same P-cores and holds them hot
# (~90 C sustained, 96 throttle events). See CDDA_CI_PLAN.md 11.5h.
#
# RAISING IT WAS TRIED ON COMPILES AND REJECTED. The hope was that a compile's
# link and I/O phases let the die breathe where clang-tidy's continuous frontends
# do not, but measured on this cooling, 5 and 6 both drive cores past 94 C. The
# wall-time gain does not justify running that close to TjMax.
#
# The default lives in the script rather than the workflow so that running this
# by hand on the box also gets the safe value, matching what was done for
# build-scripts/clang-tidy.sh (CLANG_TIDY_JOBS). CDDA_BUILD_JOBS overrides it
# per job.
num_jobs=${CDDA_BUILD_JOBS:-4}

# We might need binaries installed via pip, so ensure that our personal bin dir is on the PATH
export PATH=$HOME/.local/bin:$PATH

if [ -n "$CROSS_COMPILATION" ]
then
    "$CROSS_COMPILATION$COMPILER" --version
else
    $COMPILER --version
fi

if [ -n "$TEST_STAGE" ]
then
    # validate_json.py is KEPT, not a duplicate of json.yml: it globs
    # '**/**.json' across the whole repository (5424 files) where json.yml's
    # `style-all-json-parallel` only reaches `find data` (5379). The 45-file
    # difference is CMakePresets.json, the msvc-full-features and vcpkg_ports
    # manifests, the per-tileset tile_config/layering files and the
    # problem-matcher definitions -- places where a syntax error breaks a BUILD
    # rather than a data load, and which json.yml never sees.
    #
    # `make style-all-json-parallel RELEASE=1` does NOT belong here: json.yml
    # runs that exact target.
    build-scripts/validate_json.py

    tools/dialogue_validator.py data/json/npcs/* data/json/npcs/*/* data/json/npcs/*/*/*

    tools/json_tools/generic_guns_validator.py

    tools/json_tools/gun_variant_validator.py -v -cin data/json

    # Also build chkjson (even though we're not using it), to catch any
    # compile errors there
    make -j "$num_jobs" chkjson
# Skip the rest of the run if this change is pure json and this job doesn't test any extra mods
elif [ -n "$JUST_JSON" -a -z "$MODS" ]
then
    echo "Early exit on just-json change"
    exit 0
fi

ccache --zero-stats
# Increase cache size because debug builds generate large object files
# 5G TRUNCATED THE LTO BUILD MID-FLIGHT. Measured 2026-08-19, peak cache per
# variant on a cold build: CMake 0.83G, clang-21 Tiles 1.9G, clang-21 ASan 2.24G,
# MinGW 4.02G, g++-11 Curses 4.23G, g++-14 LTO **4.9 of 5.0** -- sitting on the
# ceiling, evicting objects it had just produced. Upstream's 5G was sized for
# GitHub's 10 GB repo-wide cache budget, which does not apply on a box with
# 426 GB free. 8G gives the largest variant real headroom; overridable so a
# constrained host can put it back.
ccache -M "${CCACHE_MAXSIZE:-8G}"
ccache --show-stats --verbose

if [ "$CMAKE" = "1" ]
then
    if [ "$RELEASE" = "1" ]
    then
        build_type=MinSizeRel
    else
        build_type=Debug
    fi

    # -DLOCALIZE IS PASSED EXPLICITLY, and was missing for the same reason the
    # Makefile branch below was wrong: the value happened to be right. Upstream
    # passed no -DLOCALIZE, so this branch took CMakeLists.txt:27's
    # `option(LOCALIZE ... "ON")` default and printed `LOCALIZE : ON` regardless
    # of the environment. Every current CMake variant wants ON, so it looked
    # correct -- exactly how the Makefile branch's defect survived.
    #
    # ${LOCALIZE:-1}, not a bare $LOCALIZE: an empty value would configure
    # LOCALIZE=OFF on every job that does not set it. Same trap, same guard as
    # below.
    mkdir build
    cd build
    cmake \
        -DBACKTRACE=ON \
        ${COMPILER:+-DCMAKE_CXX_COMPILER=$COMPILER} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DTILES=${TILES:-0} \
        -DSOUND=${SOUND:-0} \
        -DLOCALIZE=${LOCALIZE:-1} \
        ..
    make -j$num_jobs
else
    # LOCALIZE MUST BE PASSED ON THE COMMAND LINE, NOT INHERITED FROM THE
    # ENVIRONMENT. The Makefile assigns `LOCALIZE = 1` unconditionally at top
    # level (Makefile:172), and a Makefile assignment BEATS an environment
    # variable -- only a command-line assignment overrides it. So `LOCALIZE=0` in
    # a workflow's env: block is silently ignored and the build gets localization
    # anyway. Measured 2026-08-18: the g++-11 job, whose whole purpose is the
    # LOCALIZE=0 configuration, emitted -DLOCALIZE. Upstream sets it the same
    # way, so their localize:0 variant carries the same defect. RELEASE and TILES
    # are NOT affected -- RELEASE is `ifndef`-guarded and TILES is only assigned
    # inside `ifeq ($(SDL),1)`.
    #
    # THE DEFAULT MATTERS: ${LOCALIZE:-1} preserves the Makefile's own default
    # when the variable is unset. A bare LOCALIZE="$LOCALIZE" would send an EMPTY
    # value on every job that does not set it, and `ifeq ($(LOCALIZE),1)` would
    # then disable localization everywhere -- turning a one-job bug into a
    # six-job one.
    make -j "$num_jobs" CCACHE=1 CROSS="$CROSS_COMPILATION" LINTJSON=0 FRAMEWORK=1 UNIVERSAL_BINARY=1 DEBUG_SYMBOLS=1 LOCALIZE="${LOCALIZE:-1}"

    # For CI on macOS, patch the test binary so it can find SDL2 libraries.
    if [[ ! -z "$OS" && "$OS" = "macos-12" ]]
    then
        file tests/cata_test
        install_name_tool -add_rpath $HOME/Library/Frameworks tests/cata_test
    fi
fi

# vim:tw=0
