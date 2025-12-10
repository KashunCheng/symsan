#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Smartphone notification mode
 *
 * Input (binary, 4 x int):
 *   hour, isWeekend, isMeeting, priority
 *
 * Output:
 *   MUTED / SILENT_VIBRATE / SILENT / ALERT
 */

int main(void) {
    int hour = 0;
    int isWeekend = 0;
    int isMeeting = 0;
    int priority = 0;

    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&hour,      buf + 0, 4);
    memcpy(&isWeekend, buf + 4, 4);
    memcpy(&isMeeting, buf + 8, 4);
    memcpy(&priority,  buf + 12, 4);

    const char *mode = "MUTED";

    if (isMeeting && priority <= 7) {
        mode = "SILENT";
    } else if ((hour >= 22 || hour <= 6) && priority < 9) {
        mode = "SILENT_VIBRATE";
    } else if (isWeekend && hour >= 10 && hour <= 20 && priority >= 5) {
        mode = "ALERT";
    } else if (priority >= 8) {
        mode = "ALERT";
    } else {
        mode = "MUTED";
    }

    printf("%s\n", mode);
    return 0;
}
