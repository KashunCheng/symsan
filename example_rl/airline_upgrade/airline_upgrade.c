#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Airline seat upgrade
 *
 * Input (binary, 4 x int):
 *   miles, memberLevel, isOverbooked, isLastMinute
 *
 * Output:
 *   upgradeType=<0=none,1=econPlus,2=business> voucherCents=<int>
 */

int main(void) {
    int miles = 0;
    int memberLevel = 0;
    int isOverbooked = 0;
    int isLastMinute = 0;

    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&miles,        buf + 0, 4);
    memcpy(&memberLevel,  buf + 4, 4);
    memcpy(&isOverbooked, buf + 8, 4);
    memcpy(&isLastMinute, buf + 12, 4);

    int upgradeType = 0;
    int voucherCents = 0;

    if (isOverbooked) {
        if (memberLevel >= 2) {
            upgradeType = 2;
            voucherCents = 0;
        } else {
            upgradeType = 1;
            voucherCents = 5000;
        }
    } else {
        if (miles >= 50000 && memberLevel >= 2) {
            upgradeType = 2;
        } else if (miles >= 20000 || memberLevel >= 1) {
            upgradeType = 1;
        } else {
            upgradeType = 0;
        }
    }

    if (isLastMinute && upgradeType > 0) {
        upgradeType = upgradeType - 1;
    }

    printf("upgradeType=%d voucherCents=%d\n",
           upgradeType, voucherCents);
    return 0;
}
