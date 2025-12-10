#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: VPN connection policy
 *
 * Input (binary, 5 x int):
 *   countryRisk, isEmployee, isAdmin, deviceIsManaged, requestedService
 *
 * Output:
 *   DENY / ALLOW / ALLOW_WITH_MFA
 */

int main(void) {
    int countryRisk = 0;
    int isEmployee = 0;
    int isAdmin = 0;
    int deviceIsManaged = 0;
    int requestedService = 0;

    char buf[20];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&countryRisk,     buf + 0, 4);
    memcpy(&isEmployee,      buf + 4, 4);
    memcpy(&isAdmin,         buf + 8, 4);
    memcpy(&deviceIsManaged, buf + 12, 4);
    memcpy(&requestedService,buf + 16, 4);

    int decision = 0; /* 0=DENY,1=ALLOW,2=ALLOW_WITH_MFA */

    if (!isEmployee && requestedService != 3) {
        decision = 0;
    } else if (countryRisk == 2 && !deviceIsManaged) {
        decision = 0;
    } else if (requestedService == 2) {
        if (!isAdmin || !deviceIsManaged) {
            decision = 0;
        } else if (countryRisk >= 1) {
            decision = 2;
        } else {
            decision = 1;
        }
    } else if (requestedService == 1) {
        if (deviceIsManaged) {
            decision = 1;
        } else if (countryRisk == 0) {
            decision = 2;
        } else {
            decision = 0;
        }
    } else {
        decision = 1;
    }

    if (decision == 0) {
        printf("DENY\n");
    } else if (decision == 1) {
        printf("ALLOW\n");
    } else {
        printf("ALLOW_WITH_MFA\n");
    }

    return 0;
}
