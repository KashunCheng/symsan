#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* Program: Shipping cost and free shipping
 *
 * Input (binary, 4 x int):
 *   cartValueCents, customerTier, isInternational, hasCoupon
 *
 * Output:
 *   shippingFee=<int> freeShipping=<0/1>
 */

int main(void) {
    int cartValueCents = 0;
    int customerTier = 0;
    int isInternational = 0;
    int hasCoupon = 0;

    char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), stdin);
    (void)n;

    memcpy(&cartValueCents, buf + 0, 4);
    memcpy(&customerTier,   buf + 4, 4);
    memcpy(&isInternational,buf + 8, 4);
    memcpy(&hasCoupon,      buf + 12, 4);

    int shippingFee = 0;
    bool freeShipping = false;

    if (isInternational) {
        shippingFee = 2500;

        if (cartValueCents >= 20000 && customerTier >= 1) {
            shippingFee = 0;
            freeShipping = true;
        } else if (customerTier == 2) {
            shippingFee = shippingFee / 2;
        }
    } else {
        shippingFee = 700;

        if (cartValueCents >= 5000 || customerTier == 2) {
            shippingFee = 0;
            freeShipping = true;
        }
    }

    if (hasCoupon && shippingFee > 0) {
        int discount = 500;
        if (shippingFee > discount) {
            shippingFee = shippingFee - discount;
        } else {
            shippingFee = 0;
            freeShipping = true;
        }
    }

    printf("shippingFee=%d freeShipping=%d\n",
           shippingFee, freeShipping ? 1 : 0);
    return 0;
}
