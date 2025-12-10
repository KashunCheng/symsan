#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Simple visa eligibility
 *
 * Input (binary, 5 x int):
 *   age, hasJobOffer, hasCriminalRecord, bankBalance, stayMonths
 *
 * Output:
 *   APPROVED / NEED_MORE_DOCS / REJECTED
 */

int main(void) {
    int age = 0;
    int hasJobOffer = 0;
    int hasCriminalRecord = 0;
    int bankBalance = 0;
    int stayMonths = 0;

    char buf[20];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&age,              buf + 0, 4);
    memcpy(&hasJobOffer,      buf + 4, 4);
    memcpy(&hasCriminalRecord,buf + 8, 4);
    memcpy(&bankBalance,      buf + 12, 4);
    memcpy(&stayMonths,       buf + 16, 4);

    int decision = 0; /* 0=REJECTED,1=NEED_MORE_DOCS,2=APPROVED */

    if (age < 18 || age > 80 || hasCriminalRecord) {
        decision = 0;
    } else if (stayMonths > 60 && !hasJobOffer) {
        decision = 0;
    } else if (bankBalance >= 20000 && stayMonths <= 24) {
        decision = 2;
    } else if (hasJobOffer && bankBalance >= 10000) {
        decision = 2;
    } else if (bankBalance >= 5000) {
        decision = 1;
    } else {
        decision = 0;
    }

    if (decision == 2) {
        printf("APPROVED\n");
    } else if (decision == 1) {
        printf("NEED_MORE_DOCS\n");
    } else {
        printf("REJECTED\n");
    }

    return 0;
}
