#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Simple hospital triage
 *
 * Input (binary, 4 x int):
 *   heartRate, systolicBP, isPregnant, hasTrauma
 *
 * Output:
 *   RED / YELLOW / GREEN
 */

int main(void) {
    int heartRate = 0;
    int systolicBP = 0;
    int isPregnant = 0;
    int hasTrauma = 0;

    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&heartRate,  buf + 0, 4);
    memcpy(&systolicBP, buf + 4, 4);
    memcpy(&isPregnant, buf + 8, 4);
    memcpy(&hasTrauma,  buf + 12, 4);

    int level = 3; /* 1=RED,2=YELLOW,3=GREEN */

    if ((hasTrauma && systolicBP < 90) ||
        heartRate > 130 ||
        (isPregnant && systolicBP < 100)) {
        level = 1;
    } else if (hasTrauma ||
               (heartRate >= 100 && heartRate <= 130) ||
               (systolicBP >= 90 && systolicBP <= 100)) {
        level = 2;
    } else {
        level = 3;
    }

    if (level == 1) {
        printf("RED\n");
    } else if (level == 2) {
        printf("YELLOW\n");
    } else {
        printf("GREEN\n");
    }

    return 0;
}
