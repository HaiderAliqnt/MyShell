#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>

static int tests_run;
static int tests_failed;

#define CHECK(cond)                                                       \
    do {                                                                  \
        tests_run++;                                                      \
        if (!(cond)) {                                                    \
            tests_failed++;                                               \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                 \
    } while (0)

/* Print a summary and return the process exit code. */
#define TEST_SUMMARY(suite)                                               \
    (printf("%-22s %d checks, %d failed\n", (suite), tests_run,           \
            tests_failed), tests_failed ? 1 : 0)

#endif
