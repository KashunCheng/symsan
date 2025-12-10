#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>


int main(int argc, char *argv[]) {
    // Read input from stdin: "mode temp userLevel emergency"
    const size_t want = 4;
    unsigned char buf[4];
    size_t n = fread(buf, 1, want, stdin);
    if (n != want) {
        return -1;
    }
    int mode = buf[0] - '0';
    int temp = buf[1] - '0';
    int userLevel = buf[2] - '0';
    bool emergency = (buf[3] - '0' != 0);
    // Control function logic inlined
    bool open = false;
    bool locked = false;
    bool sensorOk = true; // Inlined check_sensor - returns fixed value

    printf("\n=== Control Function Debug ===\n");
    printf("Input: mode=%d, temp=%d, userLevel=%d, emergency=%d\n",
           mode, temp, userLevel, emergency);
    printf("Sensor status: %s\n", sensorOk ? "OK" : "FAIL");
        
    if (mode == 1) {
        printf("Mode 1: Temperature-based control\n");
        if (temp > 12 && sensorOk) {
            open = true; printf("Temperature within range, open set true.\n");
        } else {
            open = false;
        }

    } else if (mode == 2) {
        printf("Mode 2: User level control\n");
        locked = true;
        if (userLevel >= 5) {
            open = true;
            locked = false;
        } else {
            // Do nothing
        }

    } else {
        printf("Mode 3 (default): Normal operation\n");
        if (!sensorOk) {
            // Inlined log_error
            fprintf(stderr, "[ERROR] Bad sensor\n");
            open = false;
        } else {
            if (temp >= 18 && temp <= 26) {
                open = true;
            } else {
                open = false;
            }
        }
    }

    if (emergency) {
        printf("Emergency mode activated!\n");
        if (userLevel >= 10) {
            open = true;
            locked = false;
        } else {
            open = false;
        }
    }

    if (locked) {
        printf("System is LOCKED\n");
        open = false;
    }

    printf("Final state: open=%d, locked=%d\n", open, locked);
    printf("==============================\n\n");

    if (open) {
        // Inlined ok function
        printf("[OK] Door is OPEN\n");
    } else {
        printf("[INFO] Door remains CLOSED\n");
    }

    return 0;
}
