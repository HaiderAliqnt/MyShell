#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "CS311_A01_2024385_shell.h"

int main(int argc, char *argv[])
{
    shell_t sh;
    int status;

    if (argc > 1) {
        fprintf(stderr, "usage: %s\n", argv[0]);
        return 2;
    }


    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    shell_init(&sh);
    status = shell_run(&sh);
    shell_destroy(&sh);


    if (sh.in_child)
        _exit(status);
    return status;
}
