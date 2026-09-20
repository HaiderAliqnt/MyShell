#include <string.h>

#include "../include/CS311_A01_2024385_processTable.h"
#include "CS311_A01_2024385_test_util.h"

static void test_add_and_contains(void)
{
    proc_table_t t;

    proc_table_init(&t);
    CHECK(t.count == 0);
    CHECK(!proc_table_contains(&t, 100));

    proc_table_add(&t, 100, "sleep 10");
    proc_table_add(&t, 101, "cat file");
    CHECK(t.count == 2);
    CHECK(proc_table_contains(&t, 100));
    CHECK(proc_table_contains(&t, 101));
    CHECK(!proc_table_contains(&t, 102));
    CHECK(strcmp(t.entries[0].name, "sleep 10") == 0);
    proc_table_destroy(&t);
}

static void test_remove_keeps_order(void)
{
    proc_table_t t;

    proc_table_init(&t);
    proc_table_add(&t, 1, "a");
    proc_table_add(&t, 2, "b");
    proc_table_add(&t, 3, "c");

    CHECK(proc_table_remove(&t, 2) == 0);
    CHECK(t.count == 2);
    CHECK(t.entries[0].pid == 1);
    CHECK(t.entries[1].pid == 3);
    CHECK(strcmp(t.entries[1].name, "c") == 0);

    CHECK(proc_table_remove(&t, 2) == -1);       /* already gone */
    CHECK(proc_table_remove(&t, 99) == -1);      /* never existed */
    CHECK(proc_table_remove(&t, 3) == 0);        /* remove last  */
    CHECK(proc_table_remove(&t, 1) == 0);        /* remove first */
    CHECK(t.count == 0);
    proc_table_destroy(&t);
}

static void test_growth(void)
{
    proc_table_t t;

    proc_table_init(&t);
    for (int i = 1; i <= 100; i++)
        proc_table_add(&t, i, "job");            /* forces several reallocs */
    CHECK(t.count == 100);
    CHECK(proc_table_contains(&t, 1));
    CHECK(proc_table_contains(&t, 100));
    CHECK(t.entries[57].pid == 58);
    proc_table_destroy(&t);
    CHECK(t.count == 0);
    proc_table_destroy(&t);                      /* second call is harmless */
}

int main(void)
{
    test_add_and_contains();
    test_remove_keeps_order();
    test_growth();
    return TEST_SUMMARY("proctable");
}
