#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Home insurance claim approval
 *
 * Input (binary, 5 x int):
 *   damageCost, policyLimit, hasPoliceReport,
 *   monthsSinceStart, isPremiumPolicy
 *
 * Output:
 *   approvedAmount=<int> fastTrack=<0/1>
 */

int main(void) {
    int damageCost = 0;
    int policyLimit = 0;
    int hasPoliceReport = 0;
    int monthsSinceStart = 0;
    int isPremiumPolicy = 0;

    char buf[20];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&damageCost,      buf + 0, 4);
    memcpy(&policyLimit,     buf + 4, 4);
    memcpy(&hasPoliceReport, buf + 8, 4);
    memcpy(&monthsSinceStart,buf + 12, 4);
    memcpy(&isPremiumPolicy, buf + 16, 4);

    int approvedAmount = 0;
    bool fastTrack = false;

    if (damageCost <= 0 || policyLimit <= 0 || monthsSinceStart < 1) {
        approvedAmount = 0;
    } else {
        if (!hasPoliceReport && damageCost > 500000) {
            approvedAmount = 0;
        } else {
            int coverage = 80;
            if (isPremiumPolicy) {
                coverage = 90;
            }

            approvedAmount = damageCost * coverage / 100;

            if (approvedAmount > policyLimit) {
                approvedAmount = policyLimit;
            }

            if (isPremiumPolicy &&
                hasPoliceReport &&
                monthsSinceStart >= 6 &&
                damageCost < policyLimit / 2) {
                fastTrack = true;
            }
        }
    }

    printf("approvedAmount=%d fastTrack=%d\n",
           approvedAmount, fastTrack ? 1 : 0);
    return 0;
}
