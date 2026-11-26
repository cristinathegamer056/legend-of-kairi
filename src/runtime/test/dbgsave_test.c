// host tests for runtime/sora_dbgsave.h (dbgsave #60, s288 port): the sidecar's "dbgmenu" line, written and read back
// build: clang -O1 -g -Wall -Iruntime runtime/test/dbgsave_test.c -o dbgsave_test.exe
#include "sora_dbgsave.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

// a sidecar as sl_sidecar_write lays it out, with the line in its place (between "summon" and "dream")
static void side_(char *b, size_t cap, const char *line) {
    snprintf(b, cap, "{\n  \"version\": 1,\n  \"exp\": 1234,\n  \"drive\": 300,\n  \"summon\": 0,\n%s  \"dream\":\"sword\",\n  \"saved\": \"x\"\n}\n", line);
}

static void test_round_trip(void) {
    int v[DBS_NROWS], w[DBS_NROWS], bad = -1;
    bool have[DBS_NROWS];
    for (int r = 0; r < DBS_NROWS; r++) v[r] = r * 3 - 7;  // negatives too (Party None = -1)
    char line[512], b[2048];
    int n = dbs_write(line, sizeof line, v);
    CHECK(n > 0 && n == (int)strlen(line));
    CHECK(!strncmp(line, "  \"dbgmenu\": \"lv=-7,kb=-4,", 26) && !strcmp(line + n - 3, "\",\n"));
    side_(b, sizeof b, line);
    CHECK(dbs_parse(b, w, have, &bad) == DBS_NROWS && bad == 0);
    for (int r = 0; r < DBS_NROWS; r++) CHECK(have[r] && w[r] == v[r]);
    // the other flat keys of the sidecar still read their own value (the line has no quoted name inside)
    CHECK(strstr(b, "\"drive\": 300") != NULL && strstr(line, "\"drive\"") == NULL);
}

static void test_too_small(void) {
    int v[DBS_NROWS] = {0};
    char line[40];
    memset(line, 'x', sizeof line);
    CHECK(dbs_write(line, sizeof line, v) == 0 && line[0] == 0);  // never half a line
    int big[DBS_NROWS];
    for (int r = 0; r < DBS_NROWS; r++) big[r] = -2147483647 - 1;
    char l2[512];
    int n = dbs_write(l2, sizeof l2, big);
    CHECK(n > 0 && n <= 362);  // the worst case fits the sidecar's 512 buffer
    printf("worst-case line: %d chars\n", n);
}

static void test_missing_and_broken(void) {
    int w[DBS_NROWS], bad;
    bool have[DBS_NROWS];
    char b[2048];
    side_(b, sizeof b, "");  // an older save: no key
    CHECK(dbs_parse(b, w, have, &bad) == -1);
    CHECK(dbs_parse(NULL, w, have, &bad) == -1);
    side_(b, sizeof b, "  \"dbgmenu\": 5,\n");  // not a string
    CHECK(dbs_parse(b, w, have, &bad) == -2);
    side_(b, sizeof b, "  \"dbgmenu\": \"lv=3,kb=1\n");  // no closing quote on its line (a cut / hand-edited file)
    CHECK(dbs_parse(b, w, have, &bad) == -2);
    for (int r = 0; r < DBS_NROWS; r++) CHECK(!have[r]);
    strcpy(b, "{ \"dbgmenu\": \"lv=3");  // the file ends inside the value
    CHECK(dbs_parse(b, w, have, &bad) == -2);
    side_(b, sizeof b, "  \"dbgmenu\": \"\",\n");  // empty: nothing read, nothing bad
    CHECK(dbs_parse(b, w, have, &bad) == 0 && bad == 0);
}

static void test_old_format_and_junk(void) {
    int w[DBS_NROWS], bad;
    bool have[DBS_NROWS];
    char b[2048];
    side_(b, sizeof b, "  \"dbgmenu\": \"0,1,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,0,1,0\",\n");  // PR #60's positional format: nothing used
    CHECK(dbs_parse(b, w, have, &bad) == 0 && bad == 20);
    side_(b, sizeof b, "  \"dbgmenu\": \"lv=12,newrow=4,god=1,kb=,clip=1x,gyro=0,lv=13\",\n");  // a later DLL's row, junk, a repeat
    CHECK(dbs_parse(b, w, have, &bad) == 3 && bad == 3);
    CHECK(have[0] && w[0] == 13 && have[5] && w[5] == 1 && have[18] && w[18] == 0 && !have[1] && !have[6]);
    side_(b, sizeof b, "  \"dbgmenu\" :  \"mickey=2\",\n");  // spaces around ':' are fine (the glue clamps Mickey to 1)
    CHECK(dbs_parse(b, w, have, &bad) == 1 && have[14] && w[14] == 2);
}

int main(void) {
    test_round_trip();
    test_too_small();
    test_missing_and_broken();
    test_old_format_and_junk();
    printf("%s dbgsave_test (%d failed)\n", fails ? "FAIL" : "PASS", fails);
    return fails != 0;
}
