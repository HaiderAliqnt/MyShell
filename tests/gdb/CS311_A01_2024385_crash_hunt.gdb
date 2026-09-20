# crash_hunt.gdb - run a nasty session and report if mysh ever gets a fatal
# signal (SIGSEGV, SIGABRT, ...). On a crash gdb stops and prints a backtrace.
#
#   gdb -q -batch -x tests/gdb/CS311_A01_2024385_crash_hunt.gdb ./mysh
set pagination off
set confirm off
handle SIGINT nostop noprint pass
run < tests/gdb/CS311_A01_2024385_stress_input.txt > /dev/null 2>&1
if $_isvoid($_exitcode)
    echo GDB4 CRASHED\n
    bt full
else
    printf "GDB4 exit code %d\n", $_exitcode
end
quit
