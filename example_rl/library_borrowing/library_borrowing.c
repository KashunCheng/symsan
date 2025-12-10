#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Library borrowing limit
 *
 * Input (binary, 4 x int):
 *   membershipType, currentBooks, overdueBooks, hasUnpaidFines
 *
 * Output:
 *   canBorrowMore=<0/1> maxAdditional=<int>
 */

int main(void) {
    int membershipType = 0;
    int currentBooks = 0;
    int overdueBooks = 0;
    int hasUnpaidFines = 0;

    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&membershipType,  buf + 0, 4);
    memcpy(&currentBooks,    buf + 4, 4);
    memcpy(&overdueBooks,    buf + 8, 4);
    memcpy(&hasUnpaidFines,  buf + 12, 4);

    int maxAdditional = 0;
    bool canBorrowMore = false;

    if (hasUnpaidFines) {
        maxAdditional = 0;
        canBorrowMore = false;
    } else {
        int baseLimit;

        if (membershipType == 1) {
            baseLimit = 10;
        } else {
            baseLimit = 3;
        }

        if (overdueBooks > 0) {
            baseLimit = baseLimit - overdueBooks;
            if (baseLimit < 0) {
                baseLimit = 0;
            }
        }

        maxAdditional = baseLimit - currentBooks;
        if (maxAdditional < 0) {
            maxAdditional = 0;
        }

        if (maxAdditional > 0) {
            canBorrowMore = true;
        }
    }

    printf("canBorrowMore=%d maxAdditional=%d\n",
           canBorrowMore ? 1 : 0, maxAdditional);
    return 0;
}
