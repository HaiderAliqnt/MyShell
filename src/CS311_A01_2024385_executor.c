#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/CS311_A01_2024385_builtins.h"
#include "../include/CS311_A01_2024385_executor.h"
#include "../include/CS311_A01_2024385_utils.h"




static int move_fd(int fd, int target)
{
    if (fd == target)
        return 0;
    if (dup2(fd, target) == -1) {
        perror("mysh: dup2");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}


static int apply_redirections(const command_t *cmd)
{
    if (cmd->in_file != NULL) {
        int fd = open(cmd->in_file, O_RDONLY);

        if (fd == -1) {
            fprintf(stderr, "mysh: %s: %s\n", cmd->in_file, strerror(errno));
            return -1;
        }
        if (move_fd(fd, STDIN_FILENO) == -1)
            return -1;
    }

    if (cmd->out_file != NULL) {
        int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
        int fd = open(cmd->out_file, flags, 0644);

        if (fd == -1) {
            fprintf(stderr, "mysh: %s: %s\n", cmd->out_file, strerror(errno));
            return -1;
        }
        if (move_fd(fd, STDOUT_FILENO) == -1)
            return -1;
    }
    return 0;
}


static char *join_argv(char *const argv[])
{
    size_t len = 1;
    char *s, *p;

    for (size_t i = 0; argv[i] != NULL; i++)
        len += strlen(argv[i]) + 1;

    s = p = xmalloc(len);
    for (size_t i = 0; argv[i] != NULL; i++) {
        size_t n = strlen(argv[i]);

        if (i > 0)
            *p++ = ' ';
        memcpy(p, argv[i], n);
        p += n;
    }
    *p = '\0';
    return s;
}


static int run_builtin_in_shell(shell_t *sh, const command_t *cmd)
{
    int saved_in = -1, saved_out = -1;
    int status;

    fflush(stdout);

    if (cmd->in_file != NULL || cmd->out_file != NULL) {
        saved_in = dup(STDIN_FILENO);
        saved_out = dup(STDOUT_FILENO);
        if (saved_in == -1 || saved_out == -1) {
            perror("mysh: dup");
            status = 1;
            goto restore;
        }
        if (apply_redirections(cmd) == -1) {
            status = 1;
            goto restore;
        }
    }

    status = builtin_run(sh, cmd->argv);
    fflush(stdout);

restore:
    if (saved_in != -1) {
        dup2(saved_in, STDIN_FILENO);
        close(saved_in);
    }
    if (saved_out != -1) {
        dup2(saved_out, STDOUT_FILENO);
        close(saved_out);
    }
    return status;
}


 */
static int child_run(shell_t *sh, const command_t *cmd, int in_fd,
                     const int out_pipe[2], int background, pid_t pgid)
{

    signal(SIGINT, SIG_DFL);


    if (background && setpgid(0, pgid) == -1)
        perror("mysh: setpgid");

    if (in_fd != -1 && move_fd(in_fd, STDIN_FILENO) == -1)
        return 1;
    if (out_pipe[1] != -1 && move_fd(out_pipe[1], STDOUT_FILENO) == -1)
        return 1;
    if (out_pipe[0] != -1)
        close(out_pipe[0]);


    if (apply_redirections(cmd) == -1)
        return 1;

    if (builtin_is(cmd->argv[0])) {
        int status = builtin_run(sh, cmd->argv);

        fflush(stdout);
        return status;
    }

    execvp(cmd->argv[0], cmd->argv);


    if (errno == ENOENT) {
        fprintf(stderr, "mysh: %s: command not found\n", cmd->argv[0]);
        return 127;
    }
    fprintf(stderr, "mysh: %s: %s\n", cmd->argv[0], strerror(errno));
    return 126;
}




static void wait_foreground(shell_t *sh, const pid_t *pids, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        int status = 0;
        pid_t r;

        do {
            r = waitpid(pids[i], &status, 0);
        } while (r == -1 && errno == EINTR);

        if (r == -1) {
            perror("mysh: waitpid");
            status = 1 << 8;
        }
        proc_table_remove(&sh->procs, pids[i]);

        if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);

            if (sig != SIGINT && sig != SIGPIPE)
                fprintf(stderr, "mysh: process %d terminated by signal %d (%s)\n",
                        (int)pids[i], sig, strsignal(sig));
        }

        if (i == n - 1) {
            if (WIFEXITED(status))
                sh->last_status = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                sh->last_status = 128 + WTERMSIG(status);
        }
    }
}

static void exec_pipeline(shell_t *sh, const pipeline_t *pl)
{
    pid_t *pids;
    size_t started = 0;
    int prev_read = -1;
    pid_t pgid = 0;


    if (pl->ncmds == 1 && !pl->background && builtin_is(pl->cmds[0].argv[0])) {
        sh->last_status = run_builtin_in_shell(sh, &pl->cmds[0]);
        return;
    }

    pids = xmalloc(pl->ncmds * sizeof(*pids));


    fflush(stdout);

    for (size_t i = 0; i < pl->ncmds; i++) {
        const command_t *cmd = &pl->cmds[i];
        int pfd[2] = { -1, -1 };
        pid_t pid;

        if (i + 1 < pl->ncmds && pipe(pfd) == -1) {
            perror("mysh: pipe");
            sh->last_status = 1;
            break;
        }

        pid = fork();
        if (pid == -1) {
            perror("mysh: fork");
            if (pfd[0] != -1) {
                close(pfd[0]);
                close(pfd[1]);
            }
            sh->last_status = 1;
            break;
        }
        if (pid == 0) {

            sh->last_status = child_run(sh, cmd, prev_read, pfd,
                                        pl->background, pgid);
            sh->in_child = 1;
            sh->running = 0;
            free(pids);
            return;
        }


        if (pl->background) {

            if (pgid == 0)
                pgid = pid;
            setpgid(pid, pgid);
        }

        {
            char *name = join_argv(cmd->argv);

            proc_table_add(&sh->procs, pid, name);
            free(name);
        }
        pids[started++] = pid;

        if (prev_read != -1)
            close(prev_read);
        if (pfd[1] != -1)
            close(pfd[1]);
        prev_read = pfd[0];
    }

    if (prev_read != -1)
        close(prev_read);

    if (started > 0) {
        if (pl->background)
            printf("[bg] pid %d\n", (int)pids[started - 1]);
        else
            wait_foreground(sh, pids, started);
    }
    free(pids);
}

void exec_cmdline(shell_t *sh, const cmdline_t *cl)
{
    for (size_t i = 0; i < cl->count && sh->running; i++)
        exec_pipeline(sh, &cl->pipelines[i]);
}
