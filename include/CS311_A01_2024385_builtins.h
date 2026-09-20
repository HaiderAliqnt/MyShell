#ifndef BUILTINS_H
#define BUILTINS_H

#include "CS311_A01_2024385_shell.h"


int builtin_is(const char *name);


int builtin_run(shell_t *sh, char *const argv[]);



#endif
