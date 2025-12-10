#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Ride-sharing surge level
 *
 * Input (binary, 4 x int):
 *   activeDrivers, waitingPassengers, weatherAlert, bigEvent
 *
 * Output:
 *   surgeLevel=<0=normal,1=moderate,2=high>
 */

int main(void) {
    int activeDrivers = 0;
    int waitingPassengers = 0;
    int weatherAlert = 0;
    int bigEvent = 0;

    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&activeDrivers,     buf + 0, 4);
    memcpy(&waitingPassengers, buf + 4, 4);
    memcpy(&weatherAlert,      buf + 8, 4);
    memcpy(&bigEvent,          buf + 12, 4);

    int surgeLevel = 0;

    if (activeDrivers <= 0) {
        surgeLevel = 2;
    } else {
        int demandIndex = waitingPassengers * 10 / activeDrivers;

        if (weatherAlert) {
            demandIndex = demandIndex + 5;
        }
        if (bigEvent) {
            demandIndex = demandIndex + 5;
        }

        if (demandIndex >= 15) {
            surgeLevel = 2;
        } else if (demandIndex >= 8) {
            surgeLevel = 1;
        } else {
            surgeLevel = 0;
        }
    }

    printf("surgeLevel=%d\n", surgeLevel);
    return 0;
}
