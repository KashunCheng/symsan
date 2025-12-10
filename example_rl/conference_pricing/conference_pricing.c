#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Conference registration pricing
 *
 * Input (binary, 4 x int):
 *   jobType, daysBefore, isSpeaker, hasVoucher
 *
 * Output:
 *   priceCents=<int>
 */

int main(void) {
    int jobType = 0;
    int daysBefore = 0;
    int isSpeaker = 0;
    int hasVoucher = 0;

    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&jobType,    buf + 0, 4);
    memcpy(&daysBefore, buf + 4, 4);
    memcpy(&isSpeaker,  buf + 8, 4);
    memcpy(&hasVoucher, buf + 12, 4);

    int priceCents = 0;

    if (jobType == 1) {
        priceCents = 15000;
    } else if (jobType == 2) {
        priceCents = 30000;
    } else {
        priceCents = 50000;
    }

    if (daysBefore >= 60) {
        int discount = priceCents * 30 / 100;
        priceCents = priceCents - discount;
    } else if (daysBefore < 7) {
        int surcharge = priceCents * 20 / 100;
        priceCents = priceCents + surcharge;
    }

    if (isSpeaker) {
        int discountSpeaker = priceCents / 2;
        priceCents = priceCents - discountSpeaker;
    }

    if (hasVoucher) {
        priceCents = priceCents - 5000;
    }

    if (priceCents < 0) {
        priceCents = 0;
    }

    printf("priceCents=%d\n", priceCents);
    return 0;
}
