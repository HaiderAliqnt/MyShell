#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>


typedef struct {
    char **argv;
    size_t argc;
    char *in_file;
    char *out_file;
    int append;
} command_t;


typedef struct {
    command_t *cmds;
    size_t ncmds;
    int background;
} pipeline_t;


typedef struct {
    pipeline_t *pipelines;
    size_t count;
} cmdline_t;


int parse_cmdline(const char *line, cmdline_t *out);


void cmdline_free(cmdline_t *cl);

#endif
