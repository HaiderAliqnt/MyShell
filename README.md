# mysh - CS311 Assignment 01 (Mini Shell)

A small Linux shell written in C using raw system calls (`fork`, `execvp`,
`waitpid`, `pipe`, `dup2`, `kill`, ...). Line input uses the
[linenoise](https://github.com/antirez/linenoise) library.


## Build and run

```
make            # builds ./mysh  (gcc -Wall -Wextra -g, no warnings)
make run        # build and start the shell
make clean
```

## Features (exactly what the assignment lists)

| Feature | Notes |
|---|---|
| prompt | `[mysh] ~/dir > `, edited with linenoise |
| run commands | `fork()` + `execvp()`, parent `waitpid()`s; no `system()` |
| built-ins | `cd` (`cd`, `cd ~`, `cd ~/x`, `cd -`, `cd ..`), `pwd`, `exit [n]`; `PWD`/`OLDPWD` are kept up to date with `setenv()` |
| `ps` | lists the PID and command of processes *mysh started* that are still alive, from its own table |
| `kill <pid>` | terminates that process (SIGTERM, then SIGKILL after 0.5 s) and deletes its record; refuses PIDs mysh did not start |
| `&` | `cmd &` runs in the background; `a & b` runs a in the background and b in the foreground; `a & b &` starts two jobs |
| `>` `>>` | overwrite / append redirection of stdout |
| `<` | redirect stdin from a file |
| `\|` | pipelines of any length (`a \| b \| c ...`), combinable with `<`, `>`, `&` |

Also handled so that ordinary commands work: single/double quotes group words,
Ctrl-C at the prompt (shell survives), Ctrl-C during a command (kills the
command only), Ctrl-D exits. Every `syntax error` and every failed system call
prints a message and returns to the prompt.

Not implemented on purpose (not in the assignment): history, `;`, `&&`, `||`,
variables/`$`, globbing, job control (`fg`/`bg`), `export`.

## Testing

```
make test        # unit tests + functional tests (80+ checks, incl. a pseudo-terminal session)
make gdb-test    # automated gdb checks (18 checks)
make valgrind    # memory + file-descriptor leak check
```

### What the gdb tests do (`tests/gdb/`)

| Script | What it proves |
|---|---|
| `parser.gdb` | stops at `exec_cmdline()` and reads the parsed `cmdline_t` straight out of memory (pipeline count, argv, `in_file`, `out_file`, `append`, `background`), then kills the shell before anything runs |
| `proctable.gdb` | starts two background jobs, stops in `builtin_ps()` and inspects `sh->procs` |
| `fork.gdb` | `set follow-fork-mode child`, stops in `child_run()` inside the forked child |
| `crash_hunt.gdb` | runs a 50-line hostile session under gdb; any SIGSEGV/SIGABRT would stop it and print a backtrace |

### Debugging by hand

```
make gdb                         # = gdb -q ./mysh
(gdb) break exec_pipeline        # stop before each pipeline runs
(gdb) run
[mysh] ~ > ls | wc -l
(gdb) print *pl                  # the pipeline_t
(gdb) print pl->cmds[1].argv[0]  # "wc"
(gdb) watch sh->procs.count      # stop whenever a job is added/removed
(gdb) set follow-fork-mode child # follow children instead of the shell
(gdb) bt                         # backtrace      (next / step / finish / continue)
```
