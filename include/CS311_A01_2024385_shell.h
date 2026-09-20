#ifndef SHELL_H
#define SHELL_H

#include "CS311_A01_2024385_processTable.h"

typedef struct {
    proc_table_t procs;
    int last_status;
    int running;
    int in_child;
} shell_t;


void shell_init(shell_t *sh);
void shell_destroy(shell_t *sh);


void shell_reap_children(shell_t *sh);


int shell_run(shell_t *sh);

#endif
