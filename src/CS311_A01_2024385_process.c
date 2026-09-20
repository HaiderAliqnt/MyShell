
#include <stdlib.h>
#include <string.h>

#include "../include/CS311_A01_2024385_processTable.h"
#include "../include/CS311_A01_2024385_utils.h"

#define PROC_TABLE_INITIAL_CAPACITY 8

void proc_table_init(proc_table_t *table)
{
    table->entries = NULL;
    table->count = 0;
    table->capacity = 0;
}

void proc_table_destroy(proc_table_t *table)
{
    for (size_t i = 0; i < table->count; i++)
        free(table->entries[i].name);
    free(table->entries);
    proc_table_init(table);
}

void proc_table_add(proc_table_t *table, pid_t pid, const char *name)
{
    if (table->count == table->capacity) {
        size_t new_cap = table->capacity ? table->capacity * 2
                                         : PROC_TABLE_INITIAL_CAPACITY;
        table->entries = xrealloc(table->entries,
                                  new_cap * sizeof(*table->entries));
        table->capacity = new_cap;
    }
    table->entries[table->count].pid = pid;
    table->entries[table->count].name = xstrdup(name);
    table->count++;
}

int proc_table_remove(proc_table_t *table, pid_t pid)
{
    for (size_t i = 0; i < table->count; i++) {
        if (table->entries[i].pid != pid)
            continue;

        free(table->entries[i].name);
        /* close the gap so the remaining records stay in start order */
        memmove(&table->entries[i], &table->entries[i + 1],
                (table->count - i - 1) * sizeof(*table->entries));
        table->count--;
        return 0;
    }
    return -1;
}

int proc_table_contains(const proc_table_t *table, pid_t pid)
{
    for (size_t i = 0; i < table->count; i++) {
        if (table->entries[i].pid == pid)
            return 1;
    }
    return 0;
}
