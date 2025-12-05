def xor(a, b):
    a %= 2
    b %= 2
    ret = 0
    if a and b:
        ret = 1
    elif not a and not b:
        ret = 1
    if ret:
        return 2
    return 0

if __name__ == '__main__':
    import sys
    xor(int(sys.argv[1]), int(sys.argv[2]))