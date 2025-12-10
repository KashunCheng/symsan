#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Smart street light control
 *
 * Input (binary, 4 x int):
 *   ambient, hour, isRaining, isHoliday
 *
 * Output:
 *   level=<0=off,1=dim,2=bright> keepOnAllNight=<0/1>
 */

int main(void) {
    int ambient = 0;
    int hour = 0;
    int isRaining = 0;
    int isHoliday = 0;

    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&ambient,   buf + 0, 4);
    memcpy(&hour,      buf + 4, 4);
    memcpy(&isRaining, buf + 8, 4);
    memcpy(&isHoliday, buf + 12, 4);

    int level = 0;
    bool keepOnAllNight = false;

    if (hour >= 8 && hour <= 17 && ambient >= 70 && !isRaining) {
        level = 0;
    } else if (ambient <= 30 || isRaining) {
        level = 2;
    } else {
        level = 1;
    }

    if (isHoliday && hour >= 20) {
        keepOnAllNight = true;
    }

    printf("level=%d keepOnAllNight=%d\n", level, keepOnAllNight ? 1 : 0);
    return 0;
}
