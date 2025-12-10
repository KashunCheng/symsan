#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Student scholarship decision
 *
 * Input (binary, 4 x int):
 *   avgGrade, familyIncome, hasDisability, isRemoteArea
 *
 * Output:
 *   SCHOLARSHIP_NONE / SCHOLARSHIP_PARTIAL / SCHOLARSHIP_FULL
 */

int main(void) {
    int avgGrade = 0;
    int familyIncome = 0;
    int hasDisability = 0;
    int isRemoteArea = 0;

    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&avgGrade,     buf + 0, 4);
    memcpy(&familyIncome, buf + 4, 4);
    memcpy(&hasDisability,buf + 8, 4);
    memcpy(&isRemoteArea, buf + 12, 4);

    int type = 0; /* 0=none,1=partial,2=full */

    if (avgGrade < 60) {
        type = 0;
    } else {
        if (avgGrade >= 90) {
            type = 2;
        } else if (avgGrade >= 75) {
            type = 1;
        } else {
            type = 0;
        }

        if ((hasDisability || isRemoteArea) && familyIncome < 20000) {
            if (type < 2) {
                type = type + 1;
            }
        }

        if (familyIncome > 80000 && type == 2) {
            type = 1;
        }
    }

    if (type == 0) {
        printf("SCHOLARSHIP_NONE\n");
    } else if (type == 1) {
        printf("SCHOLARSHIP_PARTIAL\n");
    } else {
        printf("SCHOLARSHIP_FULL\n");
    }

    return 0;
}
