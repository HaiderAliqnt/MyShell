#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/CS311_A01_2024385_parser.h"
#include "../include/CS311_A01_2024385_utils.h"


typedef enum {
    TOK_WORD,
    TOK_PIPE,       /* |  */
    TOK_AMP,        /* &  */
    TOK_IN,         /* <  */
    TOK_OUT,        /* >  */
    TOK_APPEND      /* >> */
} tok_type_t;

typedef struct {
    tok_type_t type;
    char *text;
} token_t;

typedef struct {
    token_t *items;
    size_t count;
    size_t capacity;
} toklist_t;

static void toklist_push(toklist_t *tl, tok_type_t type, char *text)
{
    if (tl->count == tl->capacity) {
        tl->capacity = tl->capacity ? tl->capacity * 2 : 16;
        tl->items = xrealloc(tl->items, tl->capacity * sizeof(*tl->items));
    }
    tl->items[tl->count].type = type;
    tl->items[tl->count].text = text;
    tl->count++;
}

static void toklist_free(toklist_t *tl)
{
    for (size_t i = 0; i < tl->count; i++)
        free(tl->items[i].text);
    free(tl->items);
    tl->items = NULL;
    tl->count = tl->capacity = 0;
}


typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    int active;
} wordbuf_t;

static void word_append(wordbuf_t *w, char c)
{
    if (w->len + 1 >= w->cap) {
        w->cap = w->cap ? w->cap * 2 : 32;
        w->buf = xrealloc(w->buf, w->cap);
    }
    w->buf[w->len++] = c;
    w->active = 1;
}


static void word_flush(wordbuf_t *w, toklist_t *tl)
{
    if (!w->active)
        return;
    if (w->buf == NULL)
        w->buf = xmalloc(1);
    w->buf[w->len] = '\0';
    toklist_push(tl, TOK_WORD, w->buf);
    w->buf = NULL;
    w->len = w->cap = 0;
    w->active = 0;
}


static int tokenize(const char *line, toklist_t *tl)
{
    wordbuf_t w = { NULL, 0, 0, 0 };

    for (const char *p = line; *p != '\0'; p++) {
        char c = *p;

        if (c == '\'' || c == '"') {
            char quote = c;

            w.active = 1;
            for (p++; *p != '\0' && *p != quote; p++)
                word_append(&w, *p);
            if (*p == '\0') {
                fprintf(stderr, "mysh: syntax error: unterminated %s quote\n",
                        quote == '"' ? "double" : "single");
                free(w.buf);
                toklist_free(tl);
                return -1;
            }
        } else if (isspace((unsigned char)c)) {
            word_flush(&w, tl);
        } else if (c == '|') {
            word_flush(&w, tl);
            toklist_push(tl, TOK_PIPE, NULL);
        } else if (c == '&') {
            word_flush(&w, tl);
            toklist_push(tl, TOK_AMP, NULL);
        } else if (c == '<') {
            word_flush(&w, tl);
            toklist_push(tl, TOK_IN, NULL);
        } else if (c == '>') {
            word_flush(&w, tl);
            if (p[1] == '>') {
                toklist_push(tl, TOK_APPEND, NULL);
                p++;
            } else {
                toklist_push(tl, TOK_OUT, NULL);
            }
        } else {
            word_append(&w, c);
        }
    }
    word_flush(&w, tl);
    return 0;
}



static void command_init(command_t *cmd)
{
    cmd->argv = NULL;
    cmd->argc = 0;
    cmd->in_file = NULL;
    cmd->out_file = NULL;
    cmd->append = 0;
}

static void command_free(command_t *cmd)
{
    for (size_t i = 0; i < cmd->argc; i++)
        free(cmd->argv[i]);
    free(cmd->argv);
    free(cmd->in_file);
    free(cmd->out_file);
    command_init(cmd);
}

static void command_add_arg(command_t *cmd, const char *word)
{

    cmd->argv = xrealloc(cmd->argv, (cmd->argc + 2) * sizeof(*cmd->argv));
    cmd->argv[cmd->argc++] = xstrdup(word);
    cmd->argv[cmd->argc] = NULL;
}

static void pipeline_init(pipeline_t *pl)
{
    pl->cmds = NULL;
    pl->ncmds = 0;
    pl->background = 0;
}

static void pipeline_free(pipeline_t *pl)
{
    for (size_t i = 0; i < pl->ncmds; i++)
        command_free(&pl->cmds[i]);
    free(pl->cmds);
    pipeline_init(pl);
}


static void pipeline_add_command(pipeline_t *pl, command_t *cmd)
{
    pl->cmds = xrealloc(pl->cmds, (pl->ncmds + 1) * sizeof(*pl->cmds));
    pl->cmds[pl->ncmds++] = *cmd;
}


static void cmdline_add_pipeline(cmdline_t *cl, pipeline_t *pl)
{
    cl->pipelines = xrealloc(cl->pipelines,
                             (cl->count + 1) * sizeof(*cl->pipelines));
    cl->pipelines[cl->count++] = *pl;
}

void cmdline_free(cmdline_t *cl)
{
    for (size_t i = 0; i < cl->count; i++)
        pipeline_free(&cl->pipelines[i]);
    free(cl->pipelines);
    cl->pipelines = NULL;
    cl->count = 0;
}

static const char *operator_name(tok_type_t type)
{
    switch (type) {
    case TOK_IN:     return "<";
    case TOK_OUT:    return ">";
    case TOK_APPEND: return ">>";
    case TOK_PIPE:   return "|";
    case TOK_AMP:    return "&";
    default:         return "word";
    }
}


static void command_set_redirect(command_t *cmd, tok_type_t op, const char *file)
{
    if (op == TOK_IN) {
        free(cmd->in_file);
        cmd->in_file = xstrdup(file);
    } else {
        free(cmd->out_file);
        cmd->out_file = xstrdup(file);
        cmd->append = (op == TOK_APPEND);
    }
}

int parse_cmdline(const char *line, cmdline_t *out)
{
    toklist_t tl = { NULL, 0, 0 };
    cmdline_t cl = { NULL, 0 };
    pipeline_t pl;
    command_t cur;

    out->pipelines = NULL;
    out->count = 0;

    if (tokenize(line, &tl) == -1)
        return -1;

    pipeline_init(&pl);
    command_init(&cur);

    for (size_t i = 0; i < tl.count; i++) {
        token_t *t = &tl.items[i];

        switch (t->type) {
        case TOK_WORD:
            command_add_arg(&cur, t->text);
            break;

        case TOK_IN:
        case TOK_OUT:
        case TOK_APPEND:
            if (i + 1 >= tl.count || tl.items[i + 1].type != TOK_WORD) {
                fprintf(stderr,
                        "mysh: syntax error: expected a file name after '%s'\n",
                        operator_name(t->type));
                goto fail;
            }
            i++;
            command_set_redirect(&cur, t->type, tl.items[i].text);
            break;

        case TOK_PIPE:
            if (cur.argc == 0) {
                fprintf(stderr, "mysh: syntax error: missing command before '|'\n");
                goto fail;
            }
            pipeline_add_command(&pl, &cur);
            command_init(&cur);
            break;

        case TOK_AMP:
            if (cur.argc == 0) {
                fprintf(stderr, "mysh: syntax error: missing command before '&'\n");
                goto fail;
            }
            pipeline_add_command(&pl, &cur);
            command_init(&cur);
            pl.background = 1;
            cmdline_add_pipeline(&cl, &pl);
            pipeline_init(&pl);
            break;
        }
    }

    if (cur.argc > 0) {
        pipeline_add_command(&pl, &cur);
        command_init(&cur);
        cmdline_add_pipeline(&cl, &pl);
        pipeline_init(&pl);
    } else if (pl.ncmds > 0) {
        fprintf(stderr, "mysh: syntax error: missing command after '|'\n");
        goto fail;
    } else if (cur.in_file != NULL || cur.out_file != NULL) {
        fprintf(stderr, "mysh: syntax error: redirection without a command\n");
        goto fail;
    }

    toklist_free(&tl);
    *out = cl;
    return 0;

fail:
    command_free(&cur);
    pipeline_free(&pl);
    cmdline_free(&cl);
    toklist_free(&tl);
    return -1;
}
