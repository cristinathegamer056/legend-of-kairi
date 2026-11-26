// host test for runtime/sora_updraft.h (s288 port of PR #21 updraft): the pure part (updraft_parse, updraft_find), on
// made-up tables (no game data).
//   clang -O1 -g -Wall -Wextra -DSORA_UPDRAFT_TEST -Iruntime runtime/test/updraft_test.c -o build_h/updraft_test.exe
// Optional: updraft_test.exe <updrafts.txt> parses a real file and checks that every column finds itself at its centre.
#include <stdio.h>
#include "sora_updraft.h"

static int fails, runs;
#define CHECK(c) do { runs++; if (!(c)) fails++, printf("FAIL line %d: %s\n", __LINE__, #c); } while (0)

static Updraft T[UPDRAFT_MAX + 1];

static int parse(const char *s) { return updraft_parse(s, T, UPDRAFT_MAX, NULL); }

int main(int argc, char **argv) {
    const char *why = NULL;
    // one upright column at (10, 0, 20), 4 x 10 x 4 half sizes, 20 u/s; one turned 90 degrees about z (pushes along -x)
    const char *ok2 =
        "updrafts v1 2\n"
        "10.0000 0.0000 20.0000 1 0 0 0 1 0 0 0 1 4.0000 10.0000 4.0000 20.0000\n"
        "100 50 100 0 -1 0 1 0 0 0 0 1 2 5 2 30\n";
    CHECK(updraft_parse(ok2, T, UPDRAFT_MAX, &why) == 2 && !*why);
    CHECK(T[0].pos[0] == 10 && T[0].pos[2] == 20 && T[0].half[1] == 10 && T[0].push == 20 && T[1].r[1] == -1 && T[1].push == 30);
    float push[3];
    float in[3] = {11, 5, 19}, above[3] = {10, 20.5f, 20}, below[3] = {10, -0.5f, 20}, side[3] = {14.5f, 5, 20};
    CHECK(updraft_find(T, 2, in, push) == 0 && push[0] == 0 && push[1] == 20 && push[2] == 0);
    CHECK(updraft_find(T, 2, above, push) == -1);
    CHECK(updraft_find(T, 2, below, push) == -1);
    CHECK(updraft_find(T, 2, side, push) == -1);
    // the turned one: local y = world -x... R rows (0 -1 0 / 1 0 0 / 0 0 1): up (R's y column) = (-1, 0, 0)
    float t1[3] = {95, 50, 100};
    CHECK(updraft_find(T, 2, t1, push) == 1 && push[0] == -30 && push[1] == 0 && push[2] == 0);
    float t1b[3] = {105, 50, 100};
    CHECK(updraft_find(T, 2, t1b, push) == -1);
    // CRLF, blank lines, an empty table
    CHECK(parse("updrafts v1 1\r\n\r\n1 2 3 1 0 0 0 1 0 0 0 1 1 1 1 5\r\n\r\n") == 1 && T[0].pos[2] == 3 && T[0].push == 5);
    CHECK(parse("updrafts v1 0\n") == 0);
    CHECK(parse("updrafts v1 0") == 0);
    // refused
    CHECK(updraft_parse(NULL, T, UPDRAFT_MAX, &why) == -1);
    CHECK(parse("") == -1);
    CHECK(parse("updrafts v2 1\n1 2 3 1 0 0 0 1 0 0 0 1 1 1 1 5\n") == -1);
    CHECK(parse("updrafts v1 2\n1 2 3 1 0 0 0 1 0 0 0 1 1 1 1 5\n") == -1);                                     // (too few)
    CHECK(parse("updrafts v1 1\n1 2 3 1 0 0 0 1 0 0 0 1 1 1 1 5\n1 2 3 1 0 0 0 1 0 0 0 1 1 1 1 5\n") == -1);     // (too many)
    CHECK(parse("updrafts v1 1\n1 2 3 1 0 0 0 1 0 0 0 1 1 1 1\n") == -1);                                       // (15 numbers)
    CHECK(parse("updrafts v1 1\n1 2 3 1 0 0 0 1 0 0 0 1 1 1 1 5 6\n") == -1);                                   // (17)
    CHECK(parse("updrafts v1 1\n1 2 3 1 0 0 0 1 0 0 0 1 1 1 1 nan\n") == -1);
    CHECK(parse("updrafts v1 1\n1 2 3 1 0 0 0 1 0 0 0 1 1 1 1 inf\n") == -1);
    CHECK(parse("updrafts v1 1\n1 2 3 1 0 0 0 1 0 0 0 1 1 1 1 1e39\n") == -1);                                  // (float overflow)
    CHECK(parse("updrafts v1 1\n1 2 3 1 0 0 0 1 0 0 0 1 1 1 1 5x\n") == -1);
    CHECK(parse("updrafts v1 1\n1 2 3 1 0 0 0 1 0 0 0 1 1 -1 1 5\n") == -1);                                    // (negative half)
    CHECK(parse("updrafts v1 1025\n") == -1);                                                                    // (over UPDRAFT_MAX)
    CHECK(parse("updrafts v1 -1\n") == -1);
    CHECK(parse("updrafts v1 1 junk\n1 2 3 1 0 0 0 1 0 0 0 1 1 1 1 5\n") == -1);
    CHECK(updraft_parse("updrafts v1 3\n", T, 2, &why) == -1);                                                   // (over cap)
    // the cap: exactly UPDRAFT_MAX rows parse
    {
        static char big[UPDRAFT_MAX * 64 + 64];
        int n = snprintf(big, sizeof big, "updrafts v1 %d\n", UPDRAFT_MAX);
        for (int i = 0; i < UPDRAFT_MAX; i++) n += snprintf(big + n, sizeof big - n, "%d 0 0 1 0 0 0 1 0 0 0 1 1 1 1 20\n", i * 10);
        CHECK(parse(big) == UPDRAFT_MAX && T[UPDRAFT_MAX - 1].pos[0] == (UPDRAFT_MAX - 1) * 10);
    }
    // a real file (optional): every column finds itself 1 u above its feet, in its own box
    if (argc > 1) {
        FILE *f = fopen(argv[1], "rb");
        static char buf[UPDRAFT_FILE_MAX + 1];
        size_t n = f ? fread(buf, 1, UPDRAFT_FILE_MAX, f) : 0;
        if (f) fclose(f);
        buf[n] = 0;
        int rows = updraft_parse(buf, T, UPDRAFT_MAX, &why), self = 0;
        printf("%s: %d rows%s%s\n", argv[1], rows, rows < 0 ? ", refused: " : "", rows < 0 ? why : "");
        CHECK(rows > 0);
        for (int i = 0; i < rows; i++) {
            float c[3];
            for (int k = 0; k < 3; k++) c[k] = T[i].pos[k] + T[i].r[k * 3 + 1] * (T[i].half[1] < 1 ? T[i].half[1] : 1.0f);
            float pp[3];
            int j = updraft_find(T, rows, c, pp);
            self += j >= 0;
        }
        printf("  %d of %d columns found at their feet + 1 u (or + half height)\n", self, rows);
        CHECK(self == rows);
    }
    printf("updraft_test: %d checks, %d failed\n", runs, fails);
    return fails != 0;
}
