#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Car rental eligibility
 *
 * Input (binary, 5 x int):
 *   age, licenseYears, hasMajorAccident, hasCreditCard, carClass
 *
 * Output:
 *   DENIED / ALLOWED_WITH_DEPOSIT / ALLOWED_NO_DEPOSIT
 */

int main(void) {
    int age = 0;
    int licenseYears = 0;
    int hasMajorAccident = 0;
    int hasCreditCard = 0;
    int carClass = 0;

    char buf[20];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&age,             buf + 0, 4);
    memcpy(&licenseYears,    buf + 4, 4);
    memcpy(&hasMajorAccident,buf + 8, 4);
    memcpy(&hasCreditCard,   buf + 12, 4);
    memcpy(&carClass,        buf + 16, 4);

    int decision = 0; /* 0=DENIED,1=ALLOWED_WITH_DEPOSIT,2=ALLOWED_NO_DEPOSIT */

    if (age < 18 || !hasCreditCard) {
        decision = 0;
    } else if (carClass == 4) {
        if (age >= 25 && licenseYears >= 3 && !hasMajorAccident) {
            decision = 2;
        } else if (age >= 23 && licenseYears >= 2 && !hasMajorAccident) {
            decision = 1;
        } else {
            decision = 0;
        }
    } else {
        if (age >= 21 && licenseYears >= 1) {
            if (hasMajorAccident) {
                decision = 1;
            } else {
                decision = 2;
            }
        } else {
            decision = 0;
        }
    }

    if (decision == 0) {
        printf("DENIED\n");
    } else if (decision == 1) {
        printf("ALLOWED_WITH_DEPOSIT\n");
    } else {
        printf("ALLOWED_NO_DEPOSIT\n");
    }

    return 0;
}
