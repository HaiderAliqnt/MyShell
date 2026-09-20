#!/usr/bin/env bash
# run_gdb_tests.sh - automated gdb checks for mysh.   Run with: make gdb-test
#
# Each gdb script prints lines tagged GDBn; this runner greps for the values
# the program is *supposed* to hold at that point.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 1

if ! command -v gdb >/dev/null 2>&1; then
    echo "gdb is not installed (sudo apt install gdb)" >&2
    exit 1
fi

pass=0
fail=0
GDB="timeout 60 gdb -q -batch -nx"

check() {   # check "name" "output" "expected text"
    if grep -qF -- "$3" <<< "$2"; then
        pass=$((pass + 1)); printf '  \033[32mPASS\033[0m  %s\n' "$1"
    else
        fail=$((fail + 1)); printf '  \033[31mFAIL\033[0m  %s\n        expected: %s\n' "$1" "$3"
        FAILED_OUTPUT+="$2"$'\n'
    fi
}
FAILED_OUTPUT=""

echo "== parser structure (parser.gdb) =="
out=$($GDB -x tests/gdb/CS311_A01_2024385_parser.gdb ./mysh 2>&1)
check "line splits into 2 pipelines"      "$out" "GDB1 pipelines=2"
check "first pipeline: 2 cmds, background" "$out" "GDB1 p0.ncmds=2 background=1"
check "cat has 1 arg, stdin from in.txt"  "$out" "GDB1 p0.cmd0=cat argc=1 in=in.txt"
check "sort has no input redirect (NULL)" "$out" "in_file = 0x0"
check "sort -r >> out.txt parsed"         "$out" "GDB1 p0.cmd1=sort arg1=-r out=out.txt"
check "append flag set for >>"            "$out" "append=1"
check "second pipeline is foreground"     "$out" "GDB1 p1.cmd0=echo background=0"

echo "== process table (proctable.gdb) =="
out=$($GDB -x tests/gdb/CS311_A01_2024385_proctable.gdb ./mysh 2>&1)
check "two jobs recorded"                 "$out" "GDB2 count=2"
check "entry 0 is 'sleep 2'"              "$out" 'GDB2 entry0=sleep 2'
check "entry 1 is 'sleep 3'"              "$out" 'GDB2 entry1=sleep 3'
check "pids are distinct"                 "$out" "GDB2 pids_differ=1"
check "ps prints the table"               "$out" "COMMAND"

echo "== forked child (fork.gdb) =="
out=$($GDB -x tests/gdb/CS311_A01_2024385_fork.gdb ./mysh 2>&1)
check "child runs 'echo'"                 "$out" "GDB3 child cmd=echo"
check "first pipe stage reads no pipe"    "$out" "in_fd=-1"
check "foreground job"                    "$out" "background=0"

echo "== crash hunt (crash_hunt.gdb) =="
out=$($GDB -x tests/gdb/CS311_A01_2024385_crash_hunt.gdb ./mysh 2>&1)
check "stress session ends without a signal" "$out" "GDB4 exit code"
if grep -q "GDB4 CRASHED" <<< "$out"; then echo "$out" | tail -25; fi

echo "== unit tests under gdb =="
for t in test_parser test_proctable; do
    out=$($GDB -ex run -ex 'printf "GDB5 exitcode=%d\n", $_exitcode' build/$t 2>&1 </dev/null)
    check "$t exits normally under gdb" "$out" "GDB5 exitcode=0"
done

echo
echo "gdb tests: $pass passed, $fail failed"
if (( fail > 0 )); then echo "--- gdb output of failed steps ---"; echo "$FAILED_OUTPUT" | head -60; fi
(( fail == 0 ))
