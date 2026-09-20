# fork.gdb - follow the forked child instead of the shell.
#
# Stops in child_run() (the code that runs in each forked child just before
# execvp), shows the command and the pipe fd it will read from, then lets it
# exec.
#
#   gdb -q -batch -x tests/gdb/CS311_A01_2024385_fork.gdb ./mysh
set pagination off
set confirm off
set follow-fork-mode child
set detach-on-fork on
break child_run
run < tests/gdb/CS311_A01_2024385_fork_input.txt
printf "GDB3 child cmd=%s in_fd=%d background=%d\n", cmd->argv[0], in_fd, background
bt 2
continue
