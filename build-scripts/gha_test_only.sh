#!/bin/bash

# Script made specifically for running tests on GitHub Actions, rebuilt by Claude-Opus

echo "Using bash version $BASH_VERSION"
set -exo pipefail

# No `num_jobs` here: upstream declared one and never read it. The only
# parallelism in this script is `num_test_jobs` below -- do not re-add a knob
# that changes nothing.
parallel_opts="--verbose --linebuffer"
cata_test_opts="--min-duration 20 --use-colour yes --rng-seed time ${EXTRA_TEST_OPTS}"
# Upstream's default of 3 is kept: it is already below the measured safe ceiling
# of 4 on this box (CDDA_CI_PLAN.md 11.5h), and the test binary runs hotter than
# a compile. NUM_TEST_JOBS overrides per job.
[ -z $NUM_TEST_JOBS ] && num_test_jobs=3 || num_test_jobs=$NUM_TEST_JOBS

# We might need binaries installed via pip, so ensure that our personal bin dir is on the PATH
export PATH=$HOME/.local/bin:$PATH
# export so run_test can read it when executed by parallel
export cata_test_opts 

function run_test
{
    set -eo pipefail
    test_exit_code=0 sed_exit_code=0 exit_code=0
    test_bin=$1
    prefix=$2
    shift 2

    $WINE "$test_bin" ${cata_test_opts} "$@" 2>&1 | sed -E 's/^(::(warning|error|debug)[^:]*::)?/\1'"$prefix"'/' || test_exit_code="${PIPESTATUS[0]}" sed_exit_code="${PIPESTATUS[1]}"
    if [ "$test_exit_code" -ne "0" ]
    then
        echo "$3test exited with code $test_exit_code"
        exit_code=1
    fi
    if [ "$sed_exit_code" -ne "0" ]
    then
        echo "$3sed exited with code $sed_exit_code"
        exit_code=1
    fi
    return $exit_code
}
export -f run_test

if [ "$CMAKE" = "1" ]
then
    bin_path="./"
    if [ "$RELEASE" = "1" ]
    then
        build_type=MinSizeRel
        bin_path="build/tests/"
    else
        build_type=Debug
    fi

    # Which Catch2 specs to run. Defaults to the full split; CMAKE_TEST_SPECS
    # lets a job run a narrow subset, which is what makes this branch cheap
    # enough to execute at all (CDDA_CI_PLAN.md gap 2). Scoped to the CMake
    # branch deliberately -- the Makefile branch below is already green.
    #
    # SEMICOLON-separated, not whitespace: a Catch2 spec legitimately contains
    # spaces. Splitting on whitespace would turn one spec into several, and
    # Catch2 ORs multiple specs, so the run would quietly WIDEN instead of
    # failing.
    if [ -n "$CMAKE_TEST_SPECS" ]
    then
        IFS=';' read -r -a test_specs <<< "$CMAKE_TEST_SPECS"
    else
        test_specs=( "[slow] ~starting_items" "~[slow] ~[.],starting_items" )
    fi

    # Run regular tests
    #
    # `-j "$num_test_jobs"` is not optional: without -j, GNU parallel runs one
    # job per core -- 20 on this box, against a measured safe ceiling of 4
    # (CDDA_CI_PLAN.md 11.5h). Upstream omitted it here but not in the branch
    # below; harmless on GitHub's 2-4 core runners, which is why it survived.
    #
    # `if`, NOT `[ -f x ] && cmd`. Measured 2026-08-20: a failing `[ -f ]` on the
    # LAST line of this branch does not trip `set -e` (non-final command in an &&
    # list) but DOES become the script's exit status -- so a CURSES-only CMake
    # build ran its suite, passed, and the job still failed on the missing tiles
    # binary. Tiles-only builds escaped it purely because the missing probe was
    # not last, which is why no run ever showed this.
    ran_any=0
    if [ -f "${bin_path}cata_test" ]
    then
        parallel -j "$num_test_jobs" ${parallel_opts} "run_test $(printf %q "${bin_path}")'/cata_test' '('{}')=> ' --user-dir=test_user_dir_{#} {}" ::: "${test_specs[@]}"
        ran_any=1
    fi
    if [ -f "${bin_path}cata_test-tiles" ]
    then
        parallel -j "$num_test_jobs" ${parallel_opts} "run_test $(printf %q "${bin_path}")'/cata_test-tiles' '('{}')=> ' --user-dir=test_user_dir_{#} {}" ::: "${test_specs[@]}"
        ran_any=1
    fi
    if [ "$ran_any" -eq 0 ]
    then
        echo "FATAL: no CMake test binary found under '${bin_path}' -- nothing was tested"
        exit 1
    fi
else
    export ASAN_OPTIONS=detect_odr_violation=1
    export UBSAN_OPTIONS=print_stacktrace=1

    # A WINDOWS CROSS BUILD PRODUCES cata_test.exe, NOT cata_test. The Makefile
    # knows this (`TEST_TARGET = $(BUILD_PREFIX)cata_test.exe` when
    # TARGETSYSTEM=WINDOWS); upstream's script hardcoded the native name.
    # Measured 2026-08-18, the MinGW job built both binaries and then died with
    #
    #   wine: failed to open "./tests/cata_test": c0000135
    #   test exited with code 53
    #
    # -- the same exit code a MISSING DLL produces, misleading here because
    # nothing was missing except the four characters ".exe".
    #
    # PROBED, not derived from $CROSS_COMPILATION, so it is correct for any route
    # yielding a .exe (MXE, Debian mingw-w64, a native Windows runner) and needs
    # no new variable set correctly.
    test_bin=./tests/cata_test
    [ -f "${test_bin}.exe" ] && test_bin="${test_bin}.exe"

    # SPEC SYNTAX, since it is easy to get wrong: space = AND, comma = OR,
    # `~` = NOT. So `~[slow] ~[.],starting_items` is
    # "(not slow AND not hidden) OR starting_items".
    #
    # THE BUCKETS MUST PARTITION THE SUITE EXACTLY -- a spec typo that drops
    # cases makes the job faster AND greener while testing less, the worst
    # possible failure here. The assertion below is what makes that non-silent;
    # do not remove it as redundant. TEST_SPECS overrides the pair for
    # experiments; the assertion applies either way.
    if [ -n "${TEST_SPECS:-}" ]
    then
        # semicolon-delimited: a Catch2 spec legitimately contains spaces
        IFS=';' read -r -a test_specs <<< "$TEST_SPECS"
    else
        # Upstream's two-bucket split. Splitting further was measured and
        # rejected: the gain is ~10% of matrix wall time and the thermal cost is
        # not worth it on this cooling.
        #
        # ONE `~name` PER SPEC IS A HARD LIMIT. Catch2 2.13.7 silently excludes
        # NOTHING when a spec carries two name exclusions, and it fails OPEN --
        # the bucket widens rather than erroring. Tag exclusions are unlimited.
        # This pair is fine (one name exclusion); anything more elaborate needs
        # the partition assertion below to prove it.
        test_specs=( "[slow] ~starting_items" "~[slow] ~[.],starting_items" )
    fi

    # COVERAGE ASSERTION: every case the binary knows must appear in exactly one
    # bucket. Listing is cheap (no data load) and catches both halves of the
    # failure -- cases lost to a typo, and cases double-counted by overlapping
    # specs. A CHECK MUST FAIL WHEN IT CANNOT MEASURE, not only when it measures
    # something wrong, which is why an empty listing is fatal below.
    #
    # `--list-test-names-only`, ONE NAME PER LINE, not `--list-tests`: Catch2
    # prints "N matching test cases" when a FILTER is supplied, so parsing that
    # trailer returns nothing for every filtered count. Total and summed then come
    # from the same broken measurement, `0 == 0` prints "split verified", and an
    # unverified split runs for 15 minutes.
    #
    # DO NOT TEST THE EXIT STATUS OF THE LISTING. Catch2 v2 returns the NUMBER OF
    # TESTS LISTED as its exit code, so a successful listing of 1175 cases exits
    # 1175 -- which an `|| { ... }` guard reads as failure. Measured 2026-08-22:
    # the listing worked, `out` held every name, and the error branch fired
    # anyway. Capture the output, ignore the status, judge the RESULT; the
    # `-lt 1` guards below are what turn emptiness into a hard failure.
    #
    # `tr -d '\r'` IS REQUIRED FOR THE MinGW JOB. A Windows binary under wine
    # emits CRLF and `grep -c .` counts a line holding only `\r`, because `\r` is
    # a character. Measured 2026-08-22: every count came back exactly +1 under
    # wine (total 1119 vs 1118 native, buckets 17 and 1103 vs 16 and 1102), so
    # the total gained 1 while the two buckets gained 2 between them and the
    # partition check failed on a suite that was in fact identical. Native jobs
    # never see this.
    count_spec() {
        local out
        out=$( $WINE "$test_bin" --list-test-names-only "$1" 2>/dev/null || true )
        printf '%s\n' "$out" | tr -d '\r' | grep -c . || true
    }

    total=$( count_spec "~[.]" )
    if [ "${total:-0}" -lt 1 ]; then
        echo "FATAL: the suite lists ${total:-<nothing>} cases." >&2
        echo "  The listing returned nothing. Its EXIT STATUS is not a failure" >&2
        echo "  signal -- Catch2 returns the listed count as the code -- so this" >&2
        echo "  means genuinely empty output, not a failed command. A count of 0" >&2
        echo "  would otherwise make the partition check below pass trivially" >&2
        echo "  (0 == 0), which is exactly how an unverified split ran for 15" >&2
        echo "  minutes on 2026-08-22. Verify the binary and the flag." >&2
        exit 1
    fi

    summed=0
    for spec in "${test_specs[@]}"; do
        n=$( count_spec "$spec" )
        echo "  bucket [$spec] -> $n cases"
        if [ "${n:-0}" -lt 1 ]; then
            echo "FATAL: bucket [$spec] matches no cases -- a bucket that runs" >&2
            echo "  nothing is a spec error, not a valid partition." >&2
            exit 1
        fi
        summed=$(( summed + n ))
    done

    if [ "$summed" -ne "$total" ]; then
        echo "FATAL: buckets cover $summed cases but the suite has $total." >&2
        echo "  A mismatch means the split lost cases (typo) or double-counts them" >&2
        echo "  (overlapping specs). Either way the split is not a partition." >&2
        # NAME THE CULPRITS. A count alone sent two rounds of diagnosis down the
        # wrong path, and the name lists cannot be reconstructed from the job LOG
        # -- the runner prefixes every line and the prefixes end up inside the
        # extracted names. Compute the difference HERE, where the output is clean.
        tmpd=$( mktemp -d )
        $WINE "$test_bin" --list-test-names-only "~[.]" 2>/dev/null | sort > "$tmpd/all" || true
        : > "$tmpd/union"
        for spec in "${test_specs[@]}"; do
            $WINE "$test_bin" --list-test-names-only "$spec" 2>/dev/null >> "$tmpd/union" || true
        done
        sort "$tmpd/union" > "$tmpd/union.s"
        echo "  --- cases claimed by MORE THAN ONE bucket ---" >&2
        uniq -d "$tmpd/union.s" | sed 's/^/    /' >&2
        echo "  --- cases in the suite but in NO bucket ---" >&2
        comm -23 "$tmpd/all" <( sort -u "$tmpd/union.s" ) | sed 's/^/    /' >&2
        rm -rf "$tmpd"
        exit 1
    fi
    echo "  split verified: $total cases across ${#test_specs[@]} buckets"

    parallel -j "$num_test_jobs" ${parallel_opts} "run_test $(printf %q "${test_bin}") '('{}')=> ' --user-dir=test_user_dir_{#} {}" ::: "${test_specs[@]}"
    if [ -n "$MODS" ]
    then
        parallel -j "$num_test_jobs" ${parallel_opts} "run_test $(printf %q "${test_bin}") 'Mods-('{}')=> ' $(printf %q "${MODS}") --user-dir=modded_{#} {}" ::: "${test_specs[@]}"
    fi

    if [ -n "$TEST_STAGE" ]
    then
        # Run the tests with all the mods, without actually running any tests,
        # just to verify that all the mod data can be successfully loaded.
        # Because some mods might be mutually incompatible we might need to run a few times.

        ./build-scripts/get_all_mods.py | \
            while read mods
            do
                run_test "$test_bin" '(all_mods)=> ' '[force_load_game]' --user-dir=all_modded --mods="${mods}"
            done
    fi
fi

# vim:tw=0
