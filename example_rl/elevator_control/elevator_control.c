#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Office elevator control
 *
 * Input (binary, 4 x int):
 *   employeeLevel, maintenanceMode, fireAlarm, requestedFloor
 *
 * Output:
 *   dest=<floor or -1> locked=<0/1> vipExpress=<0/1>
 */

int main(void) {
    int employeeLevel = 0;
    int maintenanceMode = 0;
    int fireAlarm = 0;
    int requestedFloor = 0;

    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&employeeLevel,   buf + 0, 4);
    memcpy(&maintenanceMode, buf + 4, 4);
    memcpy(&fireAlarm,       buf + 8, 4);
    memcpy(&requestedFloor,  buf + 12, 4);

    int destinationFloor = -1;
    bool locked = false;
    bool vipExpress = false;

    if (fireAlarm) {
        if (employeeLevel >= 5) {
            destinationFloor = 0;
            locked = false;
        } else {
            destinationFloor = -1;
            locked = true;
        }
    } else if (maintenanceMode) {
        if (employeeLevel >= 7) {
            if (requestedFloor == 30) {
                destinationFloor = 30;
            } else {
                destinationFloor = 1;
            }
            locked = false;
        } else {
            destinationFloor = -1;
            locked = true;
        }
    } else {
        if (employeeLevel <= 2 && requestedFloor > 5) {
            destinationFloor = -1;
            locked = true;
        } else {
            destinationFloor = requestedFloor;
            locked = false;
            if (employeeLevel >= 8 && requestedFloor >= 20) {
                vipExpress = true;
            }
        }
    }

    printf("dest=%d locked=%d vipExpress=%d\n",
           destinationFloor, locked ? 1 : 0, vipExpress ? 1 : 0);
    return 0;
}
