#ifndef PARSER_H
#define PARSER_H

#define MAX_ARGS 64

typedef struct {
    char *argv[MAX_ARGS];

    char *input_file;
    char *output_file;

    int append;
    int background;

} Command;

void init_command(Command *cmd);

int parse_command(char *input, Command *cmd);

void free_command(Command *cmd);

#endif
