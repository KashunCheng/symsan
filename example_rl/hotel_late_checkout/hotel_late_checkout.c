#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Hotel late checkout
 *
 * Input (binary, 4 x int):
 *   loyaltyLevel, occupancyRate, isWeekend, requestedHours
 *
 * Output:
 *   allow=<0/1> feeCents=<int>
 */

int main(void) {
    int loyaltyLevel = 0;
    int occupancyRate = 0;
    int isWeekend = 0;
    int requestedHours = 0;

    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&loyaltyLevel,  buf + 0, 4);
    memcpy(&occupancyRate, buf + 4, 4);
    memcpy(&isWeekend,     buf + 8, 4);
    memcpy(&requestedHours,buf + 12, 4);

    bool allow = false;
    int feeCents = 0;

    if (requestedHours <= 0) {
        allow = false;
        feeCents = 0;
    } else if (occupancyRate >= 90 && loyaltyLevel < 2) {
        allow = false;
        feeCents = 0;
    } else {
        allow = true;
        int chargeableHours = requestedHours;

        if (!isWeekend) {
            if (loyaltyLevel >= 2) {
                if (requestedHours > 2) {
                    chargeableHours = requestedHours - 2;
                } else {
                    chargeableHours = 0;
                }
            } else if (loyaltyLevel == 1) {
                if (requestedHours > 1) {
                    chargeableHours = requestedHours - 1;
                } else {
                    chargeableHours = 0;
                }
            }
        } else {
            if (loyaltyLevel == 3) {
                if (requestedHours > 2) {
                    chargeableHours = requestedHours - 2;
                } else {
                    chargeableHours = 0;
                }
            }
        }

        feeCents = chargeableHours * 3000;

        if (occupancyRate < 50 && feeCents > 0) {
            feeCents = feeCents / 2;
        }
    }

    printf("allow=%d feeCents=%d\n", allow ? 1 : 0, feeCents);
    return 0;
}
