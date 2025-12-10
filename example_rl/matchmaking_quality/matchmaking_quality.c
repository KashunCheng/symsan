#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Online game matchmaking quality
 *
 * Input (binary, 4 x int):
 *   skillDiff, latencyMs, isRanked, partySize
 *
 * Output:
 *   MATCH_GREAT / MATCH_OK / MATCH_BAD
 */

int main(void) {
    int skillDiff = 0;
    int latencyMs = 0;
    int isRanked = 0;
    int partySize = 0;

    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&skillDiff, buf + 0, 4);
    memcpy(&latencyMs, buf + 4, 4);
    memcpy(&isRanked,  buf + 8, 4);
    memcpy(&partySize, buf + 12, 4);

    int quality = 2; /* 0=bad,1=ok,2=great */

    if (latencyMs > 250 || skillDiff > 1000) {
        quality = 0;
    } else {
        if (isRanked && skillDiff > 300) {
            quality = 1;
            if (skillDiff > 700) {
                quality = 0;
            }
        }

        if (partySize > 4 && latencyMs > 150) {
            if (quality > 0) {
                quality = quality - 1;
            }
        }
    }

    if (quality == 2) {
        printf("MATCH_GREAT\n");
    } else if (quality == 1) {
        printf("MATCH_OK\n");
    } else {
        printf("MATCH_BAD\n");
    }

    return 0;
}
