#ifndef PROCTABLE_H
#define PROCTABLE_H

#include <stddef.h>
#include <sys/types.h>

typedef struct {
    pid_t pid;
    char *name;
} proc_entry_t;

typedef struct {
    proc_entry_t *entries;
    size_t count;
    size_t capacity;
} proc_table_t;


void proc_table_init(proc_table_t *table);


void proc_table_destroy(proc_table_t *table);


void proc_table_add(proc_table_t *table, pid_t pid, const char *name);


int proc_table_remove(proc_table_t *table, pid_t pid);


int proc_table_contains(const proc_table_t *table, pid_t pid);

#endif
