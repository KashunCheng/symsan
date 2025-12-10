#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Museum entry and ticket price
 *
 * Input (binary, 4 x int in order):
 *   age, hasTicket, isMember, isWeekend
 *
 * Output:
 *   allowEntry=<0/1> priceCents=<int>
 */

int main(void) {
    int age = 0;
    int hasTicket = 0;
    int isMember = 0;
    int isWeekend = 0;

    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&age,       buf + 0, 4);
    memcpy(&hasTicket, buf + 4, 4);
    memcpy(&isMember,  buf + 8, 4);
    memcpy(&isWeekend, buf + 12, 4);

    bool allowEntry = false;
    int priceCents = 0;

    if (age < 3 || age > 110) {
        allowEntry = false;
        priceCents = 0;
    } else {
        allowEntry = true;

        if (age < 12) {
            priceCents = 0;
        } else {
            priceCents = 1200; /* base price */
        }

        if (age >= 65 && priceCents > 0) {
            priceCents = priceCents / 2; /* senior discount */
        }

        if (isMember && priceCents > 0) {
            int discount = priceCents * 20 / 100;
            priceCents = priceCents - discount;
        }

        if (isWeekend && !isMember && priceCents > 0) {
            int surcharge = priceCents * 30 / 100;
            priceCents = priceCents + surcharge;
        }

        if (priceCents < 0) {
            priceCents = 0;
        }
    }

    printf("allowEntry=%d priceCents=%d\n", allowEntry ? 1 : 0, priceCents);
    return 0;
}
