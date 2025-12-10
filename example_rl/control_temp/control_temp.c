#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>


int main(int argc, char *argv[]) {
    // Read input from stdin: "mode temp userLevel emergency"
    const size_t want = 16;
    unsigned char buf[16];
    size_t n = fread(buf, 1, want, stdin);
    if (n != want) {
        return -1;
    }
    int mode = 0;
    int temp = 0;
    int userLevel = 0;
    int emergency = 0;
    memcpy(&mode, buf, 4);
    memcpy(&temp, buf + 4, 4);
    memcpy(&userLevel, buf + 8, 4);
    memcpy(&emergency, buf + 12, 4);
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
        if (temp > 30 && sensorOk) {
            open = true;
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
