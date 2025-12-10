#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Online payment fraud check
 *
 * Input (binary, 5 x int):
 *   amountCents, countryRisk, isFirstPurchase, numChargebacks, isNightHour
 *
 * Output: APPROVED / MANUAL_REVIEW / DECLINED
 */

int main(void) {
    int amountCents = 0;
    int countryRisk = 0;
    int isFirstPurchase = 0;
    int numChargebacks = 0;
    int isNightHour = 0;

    char buf[20];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&amountCents,     buf + 0, 4);
    memcpy(&countryRisk,     buf + 4, 4);
    memcpy(&isFirstPurchase, buf + 8, 4);
    memcpy(&numChargebacks,  buf + 12, 4);
    memcpy(&isNightHour,     buf + 16, 4);

    int decision = 0; /* 0=APPROVED,1=MANUAL_REVIEW,2=DECLINED */

    if (numChargebacks >= 3) {
        decision = 2;
    } else if (amountCents > 200000 && countryRisk >= 1 && isFirstPurchase) {
        decision = 2;
    } else if (countryRisk == 2 ||
               (isNightHour && amountCents > 50000 && isFirstPurchase)) {
        decision = 1;
    } else {
        decision = 0;
    }

    if (decision == 0) {
        printf("APPROVED\n");
    } else if (decision == 1) {
        printf("MANUAL_REVIEW\n");
    } else {
        printf("DECLINED\n");
    }

    return 0;
}
