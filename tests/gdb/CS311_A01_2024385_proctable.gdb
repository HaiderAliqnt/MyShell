# proctable.gdb - check the shell's internal process table.
#
# Two background jobs are started, then execution stops when the `ps`
# built-in is entered and the table is dumped straight from memory.
#
#   gdb -q -batch -x tests/gdb/CS311_A01_2024385_proctable.gdb ./mysh
set pagination off
set confirm off
break builtin_ps
run < tests/gdb/CS311_A01_2024385_proctable_input.txt
printf "GDB2 count=%d\n", (int)sh->procs.count
printf "GDB2 entry0=%s\n", sh->procs.entries[0].name
printf "GDB2 entry1=%s\n", sh->procs.entries[1].name
printf "GDB2 pids_differ=%d\n", sh->procs.entries[0].pid != sh->procs.entries[1].pid
print sh->procs
continue
