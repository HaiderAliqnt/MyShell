#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "CS311_A01_2024385_executor.h"
#include "CS311_A01_2024385_parser.h"
#include "CS311_A01_2024385_shell.h"
#include "linenoise.h"

#define PROMPT_MAX 1024

void shell_init(shell_t *sh)
{
    proc_table_init(&sh->procs);
    sh->last_status = 0;
    sh->running = 1;
    sh->in_child = 0;
}

void shell_destroy(shell_t *sh)
{
    proc_table_destroy(&sh->procs);
}

void shell_reap_children(shell_t *sh)
{
    for (;;) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);

        if (pid > 0) {
            proc_table_remove(&sh->procs, pid);
        } else if (pid == -1 && errno == EINTR) {
            continue;
        } else {
            break;      /* 0: nothing finished, -1: no children left */
        }
    }
}

/*
 * Build a prompt such as "[mysh] ~/projects > ".
 * Plain ASCII on purpose: linenoise measures the prompt with strlen(), so
 * colour escape codes would confuse its cursor placement.
 */
static void build_prompt(char *buf, size_t size)
{
    char *cwd = getcwd(NULL, 0);
    const char *home = getenv("HOME");
    const char *shown = cwd ? cwd : "?";
    size_t hlen = home ? strlen(home) : 0;

    /* show the home directory as ~ */
    if (cwd != NULL && hlen > 1 && strncmp(cwd, home, hlen) == 0 &&
        (cwd[hlen] == '\0' || cwd[hlen] == '/')) {
        snprintf(buf, size, "[mysh] ~%s > ", cwd + hlen);
    } else {
        snprintf(buf, size, "[mysh] %s > ", shown);
    }
    free(cwd);
}

int shell_run(shell_t *sh)
{
    char prompt[PROMPT_MAX];

    while (sh->running) {
        char *line;
        cmdline_t cl;

        shell_reap_children(sh);
        build_prompt(prompt, sizeof(prompt));

        errno = 0;
        line = linenoise(prompt);
        if (line == NULL) {
            if (errno == EAGAIN)    /* Ctrl-C: abandon the line, re-prompt */
                continue;
            break;                  /* Ctrl-D or end of input */
        }

        if (parse_cmdline(line, &cl) == 0) {
            exec_cmdline(sh, &cl);
            cmdline_free(&cl);
        }
        linenoiseFree(line);
    }
    return sh->last_status;
}
