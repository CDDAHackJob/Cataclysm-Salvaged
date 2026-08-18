#!/bin/bash

# Shell script intended for clang-tidy check

echo "Using bash version $BASH_VERSION"
set -exo pipefail

# Overridable, raised from upstream's 3 to 4. This check runs on one self-hosted
# machine, so the default is set for that machine rather than left at a value
# sized for GitHub's 2-4 core hosted runners; CLANG_TIDY_JOBS overrides it for
# anywhere else.
#
# Measured on this tree (647 translation units): 537 TUs in 75 min at -P4, so
# roughly 34 s of CPU per TU, giving ~90 min whole-tree at -P4 and ~120 min at
# -P3. That matters because the whole-tree path is reachable from an ordinary
# pull request -- this script escalates to analysing everything when the change
# touches clang-tidy, build-scripts or cmake -- and the CI host has one runner.
#
# DO NOT RAISE IT FURTHER WITHOUT MEASURING HEAT, not wall time. 4 is not a
# throughput compromise, it is a thermal ceiling found by testing: on the
# i7-12700K CI host, 6 jobs ran ~90 C with 96 throttle events, and 12 and 16 both
# reached 100 C (TjMax) with the CPU fan already pegged at 255/255. 4 peaked at
# 87 C with zero throttle events. This job is the worst thermal offender in the
# set because it is continuous clang frontends with no link or I/O phase to let
# the die cool. Full table in .github/workflows/clang-tidy.yml at the
# `run clang-tidy` step.
num_jobs=${CLANG_TIDY_JOBS:-4}

# We might need binaries installed via pip, so ensure that our personal bin dir is on the PATH
export PATH=$HOME/.local/bin:$PATH

if [ "$RELEASE" = "1" ]
then
    build_type=MinSizeRel
else
    build_type=Debug
fi

cmake_extra_opts=()

if [ "$CATA_CLANG_TIDY" = "plugin" ]
then
    cmake_extra_opts+=("-DCATA_CLANG_TIDY_PLUGIN=ON")
    # Need to specify the particular LLVM / Clang versions to use, lest it
    # use the older LLVM that comes by default on Ubuntu.
    cmake_extra_opts+=("-DLLVM_DIR=/usr/lib/llvm-17/lib/cmake/llvm")
    cmake_extra_opts+=("-DClang_DIR=/usr/lib/llvm-17/lib/cmake/clang")
fi

mkdir -p build
cd build
cmake \
    -DBACKTRACE=ON \
    ${COMPILER:+-DCMAKE_CXX_COMPILER=$COMPILER} \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DTILES=${TILES:-0} \
    -DSOUND=${SOUND:-0} \
    "${cmake_extra_opts[@]}" \
    ..

if [ "$CATA_CLANG_TIDY" = "plugin" ]
then
    make -j$num_jobs CataAnalyzerPlugin
    export PATH=$PWD/tools/clang-tidy-plugin/clang-tidy-plugin-support/bin:$PATH
    if ! which FileCheck
    then
        ls -l tools/clang-tidy-plugin/clang-tidy-plugin-support/bin
        ls -l /usr/bin
        echo "Missing FileCheck"
        exit 1
    fi
    if ! which python && which python3
    then
        ln -s `which python3` $PWD/tools/clang-tidy-plugin/clang-tidy-plugin-support/bin/python
    fi
    CATA_CLANG_TIDY=clang-tidy
    lit -v tools/clang-tidy-plugin/test
fi

"$CATA_CLANG_TIDY" --version

# Show compiler C++ header search path
${COMPILER:-clang++} -v -x c++ /dev/null -c
# And the same for clang-tidy
"$CATA_CLANG_TIDY" ../src/version.cpp -- -v

cd ..
ln -s build/compile_commands.json

# We want to first analyze all files that changed in this PR, then as
# many others as possible, in a random order.
set +x

# Check for changes to any files that would require us to run clang-tidy across everything
changed_global_files="$( ( cat ./files_changed || echo 'unknown' ) | \
    egrep -i "clang-tidy|build-scripts|cmake|unknown" || true )"
if [ -n "$changed_global_files" ]
then
    first_changed_file="$(echo "$changed_global_files" | head -n 1)"
    echo "Analyzing all files because $first_changed_file was changed"
    TIDY="all"
fi

all_cpp_files="$(jq -r '.[].file | select(contains("third-party") | not)' build/compile_commands.json)"
if [ "$TIDY" == "all" ]
then
    echo "Analyzing all files"
    tidyable_cpp_files=$all_cpp_files
else
    make \
        -j $num_jobs \
        ${COMPILER:+COMPILER=$COMPILER} \
        TILES=${TILES:-0} \
        SOUND=${SOUND:-0} \
        includes

    tidyable_cpp_files="$( \
        ( test -f ./files_changed && ( build-scripts/get_affected_files.py ./files_changed | grep -v third-party ) ) || \
        echo unknown )"

    if [ "$tidyable_cpp_files" == "unknown" ]
    then
        echo "Unable to determine affected files, tidying all files"
        tidyable_cpp_files=$all_cpp_files
    fi
fi

printf "Subset to analyze: '%s'\n" "$CATA_CLANG_TIDY_SUBSET"

# We might need to analyze only a subset of the files if they have been split
# into multiple jobs for efficiency. The paths from `compile_commands.json` can
# be absolute but the paths from `get_affected_files.py` are relative, so both
# formats are matched. Exit code 1 from grep (meaning no match) is ignored in
# case one subset contains no file to analyze.
case "$CATA_CLANG_TIDY_SUBSET" in
    ( src )
        tidyable_cpp_files=$(printf '%s\n' "$tidyable_cpp_files" | grep -E '(^|/)src/' || [[ $? == 1 ]])
        ;;
    ( other )
        tidyable_cpp_files=$(printf '%s\n' "$tidyable_cpp_files" | grep -Ev '(^|/)src/' || [[ $? == 1 ]])
        ;;
esac

# The exit status is INTERPRETED rather than merely propagated, because xargs
# uses distinct codes for "the tool reported problems" and "the tool broke, and I
# gave up dispatching" -- and those two mean opposite things about coverage.
# Measured directly, 20 files at -P4 with one misbehaving:
#
#     file exits 1 (a finding)   rc=123   all 20 analysed
#     file killed by a signal    rc=125   only 17 analysed, rest ABANDONED
#     file exits 255             rc=124   dispatch stopped
#
# Without this, all three surface identically as "the job failed", and the honest
# reading of a red run -- "clang-tidy found things" -- is wrong in exactly the
# cases where files went unchecked. A crashing clang-tidy is not rare on a large
# codebase, and it silently drops every file still queued behind it.
function analyze_files_in_random_order
{
    if [ -n "$1" ]
    then
        local rc=0
        echo "$1" | shuf | \
            xargs -P "$num_jobs" -n 1 ./build-scripts/clang-tidy-wrapper.sh -quiet || rc=$?
        case "$rc" in
            0 )
                ;;
            123 )
                echo "clang-tidy reported findings. All files were analysed." >&2
                ;;
            124 | 125 )
                echo "ERROR: analysis ABORTED EARLY (xargs rc=$rc)." >&2
                echo "  125 = a clang-tidy process was killed by a signal (crash)." >&2
                echo "  124 = one exited 255." >&2
                echo "  Either way xargs stopped dispatching, so an unknown number of" >&2
                echo "  files were NEVER ANALYSED. This is NOT a clean 'found problems'" >&2
                echo "  result -- treat coverage as incomplete and re-run." >&2
                ;;
            * )
                echo "ERROR: xargs exited $rc, which is not a status it documents." >&2
                ;;
        esac
        return $rc
    else
        echo "No files to analyze"
    fi
}

echo "Analyzing affected files"
analyze_files_in_random_order "$tidyable_cpp_files"
set -x
