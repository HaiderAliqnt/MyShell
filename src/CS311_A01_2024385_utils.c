#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/CS311_A01_2024385_utils.h"


static void die_out_of_memory(void)
{
    fprintf(stderr, "mysh: fatal: out of memory\n");
    exit(EXIT_FAILURE);
}

void *xmalloc(size_t size)
{
    void *p = malloc(size);

    if (p == NULL)
        die_out_of_memory();
    return p;
}

void *xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);

    if (p == NULL)
        die_out_of_memory();
    return p;
}

char *xstrdup(const char *s)
{
    size_t len = strlen(s) + 1;
    char *copy = xmalloc(len);

    memcpy(copy, s, len);
    return copy;
}
