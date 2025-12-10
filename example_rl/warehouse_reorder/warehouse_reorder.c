#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Warehouse reorder decision
 *
 * Input (binary, 4 x int):
 *   currentStock, dailySales, daysUntilNextDelivery, isCriticalItem
 *
 * Output:
 *   reorderNow=<0/1> emergencySupplier=<0/1>
 */

int main(void) {
    int currentStock = 0;
    int dailySales = 0;
    int daysUntilNextDelivery = 0;
    int isCriticalItem = 0;

    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&currentStock,        buf + 0, 4);
    memcpy(&dailySales,          buf + 4, 4);
    memcpy(&daysUntilNextDelivery,buf + 8, 4);
    memcpy(&isCriticalItem,      buf + 12, 4);

    bool reorderNow = false;
    bool emergencySupplier = false;

    if (dailySales <= 0) {
        reorderNow = false;
        emergencySupplier = false;
    } else {
        int daysOfStock = currentStock / dailySales;
        int safetyDays = daysUntilNextDelivery + 3;

        if (isCriticalItem) {
            safetyDays = safetyDays + 2;
        }

        if (daysOfStock < safetyDays) {
            reorderNow = true;
        }

        if (daysOfStock < daysUntilNextDelivery) {
            emergencySupplier = true;
        }
    }

    printf("reorderNow=%d emergencySupplier=%d\n",
           reorderNow ? 1 : 0, emergencySupplier ? 1 : 0);
    return 0;
}
