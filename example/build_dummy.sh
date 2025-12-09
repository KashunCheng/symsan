# KO_DONT_OPTIMIZE=1 ../build/bin/ko-clang ./dummy.c -S -emit-llvm -o dummy.ll -g -O0
KO_DONT_OPTIMIZE=1 ../build/bin/ko-clang ./dummy.c -o dummy -g -O0
