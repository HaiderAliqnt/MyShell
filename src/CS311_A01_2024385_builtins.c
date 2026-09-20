#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../include/CS311_A01_2024385_builtins.h"
#include "../include/CS311_A01_2024385_utils.h"


#define KILL_GRACE_POLLS    50
#define KILL_POLL_NSEC      10000000L

typedef int (*builtin_fn)(shell_t *sh, char *const argv[]);

static size_t count_args(char *const argv[])
{
    size_t n = 0;

    while (argv[n] != NULL)
        n++;
    return n;
}


static void update_pwd_vars(const char *old_dir)
{
    char *now;

    if (old_dir != NULL && setenv("OLDPWD", old_dir, 1) == -1)
        perror("mysh: cd: setenv OLDPWD");

    now = getcwd(NULL, 0);
    if (now == NULL) {
        perror("mysh: cd: getcwd");
        return;
    }
    if (setenv("PWD", now, 1) == -1)
        perror("mysh: cd: setenv PWD");
    free(now);
}

static int builtin_cd(shell_t *sh, char *const argv[])
{
    size_t argc = count_args(argv);
    const char *target;
    char *expanded = NULL;
    char *old_dir;
    int print_dir = 0;

    (void)sh;

    if (argc > 2) {
        fprintf(stderr, "mysh: cd: too many arguments\n");
        return 1;
    }

    if (argc == 1) {                                    /* cd            */
        target = getenv("HOME");
        if (target == NULL) {
            fprintf(stderr, "mysh: cd: HOME not set\n");
            return 1;
        }
    } else if (strcmp(argv[1], "-") == 0) {             /* cd -          */
        target = getenv("OLDPWD");
        if (target == NULL) {
            fprintf(stderr, "mysh: cd: OLDPWD not set\n");
            return 1;
        }
        print_dir = 1;
    } else if (argv[1][0] == '~' &&
               (argv[1][1] == '\0' || argv[1][1] == '/')) {  /* cd ~, ~/x */
        const char *home = getenv("HOME");

        if (home == NULL) {
            fprintf(stderr, "mysh: cd: HOME not set\n");
            return 1;
        }
        expanded = xmalloc(strlen(home) + strlen(argv[1]));
        strcpy(expanded, home);
        strcat(expanded, argv[1] + 1);
        target = expanded;
    } else if (argv[1][0] == '\0') {                    /* cd ""  (no-op) */
        return 0;
    } else {
        target = argv[1];
    }

    old_dir = getcwd(NULL, 0);      /* may be NULL if the cwd was deleted */

    if (chdir(target) == -1) {
        fprintf(stderr, "mysh: cd: %s: %s\n", target, strerror(errno));
        free(old_dir);
        free(expanded);
        return 1;
    }

    update_pwd_vars(old_dir);
    if (print_dir) {
        const char *pwd = getenv("PWD");

        if (pwd != NULL)
            printf("%s\n", pwd);
    }

    free(old_dir);
    free(expanded);
    return 0;
}

static int builtin_pwd(shell_t *sh, char *const argv[])
{
    char *cwd;

    (void)sh;
    if (argv[1] != NULL) {
        fprintf(stderr, "mysh: pwd: too many arguments\n");
        return 1;
    }

    cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        fprintf(stderr, "mysh: pwd: %s\n", strerror(errno));
        return 1;
    }
    printf("%s\n", cwd);
    free(cwd);
    return 0;
}



static int builtin_exit(shell_t *sh, char *const argv[])
{
    size_t argc = count_args(argv);
    long code = sh->last_status;

    if (argc > 2) {
        fprintf(stderr, "mysh: exit: too many arguments\n");
        return 1;                       /* like bash: do not exit */
    }
    if (argc == 2) {
        char *end;

        errno = 0;
        code = strtol(argv[1], &end, 10);
        if (errno != 0 || end == argv[1] || *end != '\0') {
            fprintf(stderr, "mysh: exit: %s: numeric argument required\n",
                    argv[1]);
            code = 2;
        }
    }

    sh->running = 0;
    return (int)(code & 0xFF);
}



static int builtin_ps(shell_t *sh, char *const argv[])
{
    if (argv[1] != NULL) {
        fprintf(stderr, "mysh: ps: this version of ps takes no arguments\n");
        return 1;
    }

    shell_reap_children(sh);        /* forget processes that already ended */

    printf("%7s  %s\n", "PID", "COMMAND");
    for (size_t i = 0; i < sh->procs.count; i++)
        printf("%7d  %s\n", (int)sh->procs.entries[i].pid,
               sh->procs.entries[i].name);
    return 0;
}


static int terminate_process(pid_t pid)
{
    struct timespec pause = { 0, KILL_POLL_NSEC };

    if (kill(pid, SIGTERM) == -1) {
        if (errno == ESRCH)
            return 0;               /* already gone */
        return -1;
    }
    kill(pid, SIGCONT);             /* let a stopped process see SIGTERM */

    for (int i = 0; i < KILL_GRACE_POLLS; i++) {
        pid_t r = waitpid(pid, NULL, WNOHANG);

        if (r == pid || (r == -1 && errno == ECHILD))
            return 0;
        nanosleep(&pause, NULL);
    }

    if (kill(pid, SIGKILL) == -1 && errno != ESRCH)
        return -1;
    while (waitpid(pid, NULL, 0) == -1 && errno == EINTR)
        ;
    return 0;
}

static int builtin_kill(shell_t *sh, char *const argv[])
{
    char *end;
    long value;
    pid_t pid;

    if (argv[1] == NULL || argv[2] != NULL) {
        fprintf(stderr, "usage: kill <pid>\n");
        return 1;
    }

    errno = 0;
    value = strtol(argv[1], &end, 10);
    if (errno != 0 || end == argv[1] || *end != '\0' ||
        value <= 0 || value > INT_MAX) {
        fprintf(stderr, "mysh: kill: %s: arguments must be a positive process id\n",
                argv[1]);
        return 1;
    }
    pid = (pid_t)value;

    shell_reap_children(sh);
    if (!proc_table_contains(&sh->procs, pid)) {
        fprintf(stderr, "mysh: kill: (%ld) - no such process started by mysh\n",
                value);
        return 1;
    }

    if (terminate_process(pid) == -1) {
        fprintf(stderr, "mysh: kill: (%ld) - %s\n", value, strerror(errno));
        return 1;
    }
    proc_table_remove(&sh->procs, pid);     /* expunge its record */
    return 0;
}


static const struct {
    const char *name;
    builtin_fn fn;
} builtin_table[] = {
    { "cd",   builtin_cd   },
    { "pwd",  builtin_pwd  },
    { "exit", builtin_exit },
    { "ps",   builtin_ps   },
    { "kill", builtin_kill },
    { NULL,   NULL         }
};

int builtin_is(const char *name)
{
    for (size_t i = 0; builtin_table[i].name != NULL; i++) {
        if (strcmp(name, builtin_table[i].name) == 0)
            return 1;
    }
    return 0;
}

int builtin_run(shell_t *sh, char *const argv[])
{
    for (size_t i = 0; builtin_table[i].name != NULL; i++) {
        if (strcmp(argv[0], builtin_table[i].name) == 0)
            return builtin_table[i].fn(sh, argv);
    }
    return 1;
}
