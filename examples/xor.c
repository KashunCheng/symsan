#include <stdio.h>

int main(){
    unsigned char argc[2];
    size_t n = fread(&argc, 1, sizeof(argc), stdin);
    if (n < 2) {
        return 4;
    }
    printf("Read from stdin: %d, %d\n", (int)(argc[0]), (int)(argc[1]));
    argc[0] %= 2;
    argc[1] %= 2;
    int ret = 0;
    if(argc[0] && argc[1]){
        ret = 1;
    }else if (!argc[0] && !argc[1]) {
        ret = 1;
    }
    if (ret) {
        return 2;
    }
    return 0;
}