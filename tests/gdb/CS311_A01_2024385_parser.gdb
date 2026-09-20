# parser.gdb - inspect what the parser built, without running anything.
#
# Stops at the entry of exec_cmdline() for the first input line, prints the
# parsed structure, then kills the shell before any command is executed.
#
#   gdb -q -batch -x tests/gdb/CS311_A01_2024385_parser.gdb ./mysh
set pagination off
set confirm off
break exec_cmdline
run < tests/gdb/CS311_A01_2024385_parser_input.txt
printf "GDB1 pipelines=%d\n", (int)cl->count
printf "GDB1 p0.ncmds=%d background=%d\n", (int)cl->pipelines[0].ncmds, cl->pipelines[0].background
printf "GDB1 p0.cmd0=%s argc=%d in=%s\n", cl->pipelines[0].cmds[0].argv[0], (int)cl->pipelines[0].cmds[0].argc, cl->pipelines[0].cmds[0].in_file
printf "GDB1 p0.cmd1=%s arg1=%s out=%s append=%d\n", cl->pipelines[0].cmds[1].argv[0], cl->pipelines[0].cmds[1].argv[1], cl->pipelines[0].cmds[1].out_file, cl->pipelines[0].cmds[1].append
printf "GDB1 p1.cmd0=%s background=%d\n", cl->pipelines[1].cmds[0].argv[0], cl->pipelines[1].background
print *cl
print cl->pipelines[0].cmds[1]
kill
quit
