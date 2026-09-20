#include <string.h>

#include "../include/CS311_A01_2024385_parser.h"
#include "CS311_A01_2024385_test_util.h"

/* Parse `line` expecting success. */
static int parse_ok(const char *line, cmdline_t *cl)
{
    return parse_cmdline(line, cl) == 0;
}

static void test_blank_lines(void)
{
    cmdline_t cl;

    CHECK(parse_ok("", &cl));
    CHECK(cl.count == 0);
    cmdline_free(&cl);

    CHECK(parse_ok("   \t  ", &cl));
    CHECK(cl.count == 0);
    cmdline_free(&cl);
}

static void test_simple_command(void)
{
    cmdline_t cl;

    CHECK(parse_ok("ls -l   /tmp", &cl));
    CHECK(cl.count == 1);
    CHECK(cl.pipelines[0].ncmds == 1);
    CHECK(cl.pipelines[0].background == 0);
    CHECK(cl.pipelines[0].cmds[0].argc == 3);
    CHECK(strcmp(cl.pipelines[0].cmds[0].argv[0], "ls") == 0);
    CHECK(strcmp(cl.pipelines[0].cmds[0].argv[1], "-l") == 0);
    CHECK(strcmp(cl.pipelines[0].cmds[0].argv[2], "/tmp") == 0);
    CHECK(cl.pipelines[0].cmds[0].argv[3] == NULL);
    cmdline_free(&cl);
}

static void test_redirections(void)
{
    cmdline_t cl;
    command_t *c;

    CHECK(parse_ok("sort < in.txt > out.txt", &cl));
    c = &cl.pipelines[0].cmds[0];
    CHECK(strcmp(c->in_file, "in.txt") == 0);
    CHECK(strcmp(c->out_file, "out.txt") == 0);
    CHECK(c->append == 0);
    CHECK(c->argc == 1);
    cmdline_free(&cl);

    CHECK(parse_ok("echo hi>>log", &cl));       /* no spaces around operator */
    c = &cl.pipelines[0].cmds[0];
    CHECK(strcmp(c->out_file, "log") == 0);
    CHECK(c->append == 1);
    CHECK(c->argc == 2);
    cmdline_free(&cl);

    CHECK(parse_ok("cat > a > b", &cl));         /* last redirection wins */
    CHECK(strcmp(cl.pipelines[0].cmds[0].out_file, "b") == 0);
    cmdline_free(&cl);
}

static void test_pipes(void)
{
    cmdline_t cl;

    CHECK(parse_ok("cat f | grep x | wc -l > n", &cl));
    CHECK(cl.count == 1);
    CHECK(cl.pipelines[0].ncmds == 3);
    CHECK(strcmp(cl.pipelines[0].cmds[1].argv[0], "grep") == 0);
    CHECK(cl.pipelines[0].cmds[2].out_file != NULL);
    CHECK(cl.pipelines[0].cmds[0].out_file == NULL);
    cmdline_free(&cl);
}

static void test_background(void)
{
    cmdline_t cl;

    CHECK(parse_ok("sleep 5 &", &cl));
    CHECK(cl.count == 1);
    CHECK(cl.pipelines[0].background == 1);
    cmdline_free(&cl);

    /* a & b  -> a in background, b in foreground */
    CHECK(parse_ok("sleep 5 & echo hi", &cl));
    CHECK(cl.count == 2);
    CHECK(cl.pipelines[0].background == 1);
    CHECK(cl.pipelines[1].background == 0);
    cmdline_free(&cl);

    CHECK(parse_ok("a & b & c &", &cl));
    CHECK(cl.count == 3);
    CHECK(cl.pipelines[2].background == 1);
    cmdline_free(&cl);

    CHECK(parse_ok("a | b &", &cl));
    CHECK(cl.count == 1);
    CHECK(cl.pipelines[0].ncmds == 2);
    CHECK(cl.pipelines[0].background == 1);
    cmdline_free(&cl);
}

static void test_quotes(void)
{
    cmdline_t cl;

    CHECK(parse_ok("echo \"a   b\" 'c | d'", &cl));
    CHECK(cl.pipelines[0].cmds[0].argc == 3);
    CHECK(strcmp(cl.pipelines[0].cmds[0].argv[1], "a   b") == 0);
    CHECK(strcmp(cl.pipelines[0].cmds[0].argv[2], "c | d") == 0);
    CHECK(cl.pipelines[0].ncmds == 1);           /* quoted | is not a pipe */
    cmdline_free(&cl);

    CHECK(parse_ok("echo \"\"", &cl));             /* empty string argument */
    CHECK(cl.pipelines[0].cmds[0].argc == 2);
    CHECK(strcmp(cl.pipelines[0].cmds[0].argv[1], "") == 0);
    cmdline_free(&cl);

    CHECK(parse_ok("echo a\"b c\"d", &cl));       /* quotes glue into one word */
    CHECK(strcmp(cl.pipelines[0].cmds[0].argv[1], "ab cd") == 0);
    cmdline_free(&cl);
}

static void test_syntax_errors(void)
{
    cmdline_t cl;

    CHECK(parse_cmdline("ls |", &cl) == -1);
    CHECK(parse_cmdline("| ls", &cl) == -1);
    CHECK(parse_cmdline("ls | | wc", &cl) == -1);
    CHECK(parse_cmdline("&", &cl) == -1);
    CHECK(parse_cmdline("ls & &", &cl) == -1);
    CHECK(parse_cmdline("ls &&", &cl) == -1);
    CHECK(parse_cmdline("ls ||", &cl) == -1);
    CHECK(parse_cmdline("ls >", &cl) == -1);
    CHECK(parse_cmdline("ls > > f", &cl) == -1);
    CHECK(parse_cmdline("ls <", &cl) == -1);
    CHECK(parse_cmdline("> file", &cl) == -1);
    CHECK(parse_cmdline("echo \"oops", &cl) == -1);
    CHECK(parse_cmdline("echo 'oops", &cl) == -1);
    CHECK(parse_cmdline("ls | &", &cl) == -1);
}

int main(void)
{
    test_blank_lines();
    test_simple_command();
    test_redirections();
    test_pipes();
    test_background();
    test_quotes();
    test_syntax_errors();
    return TEST_SUMMARY("parser");
}
