#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Parking garage fee category
 *
 * Input (binary, 4 x int):
 *   minutesParked, isWeekend, hasSubscription, hasDisabledPermit
 *
 * Output:
 *   feeCents=<int> violation=<0/1>
 */

int main(void) {
    int minutesParked = 0;
    int isWeekend = 0;
    int hasSubscription = 0;
    int hasDisabledPermit = 0;

    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&minutesParked,    buf + 0, 4);
    memcpy(&isWeekend,        buf + 4, 4);
    memcpy(&hasSubscription,  buf + 8, 4);
    memcpy(&hasDisabledPermit,buf + 12, 4);

    int feeCents = 0;
    bool violation = false;

    if (minutesParked <= 15) {
        feeCents = 0;
    } else if (minutesParked <= 120) {
        feeCents = 500;
    } else {
        feeCents = 1500;
    }

    if (isWeekend && !hasSubscription && minutesParked > 120) {
        feeCents = 1000;
    }

    if (hasDisabledPermit && minutesParked <= 120) {
        feeCents = 0;
    }

    if (hasSubscription && feeCents > 0) {
        feeCents = feeCents / 2;
    }

    if (minutesParked > 720) {
        violation = true;
    }

    printf("feeCents=%d violation=%d\n", feeCents, violation ? 1 : 0);
    return 0;
}
