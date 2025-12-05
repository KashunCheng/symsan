def dummy(value):
    ret = 0
    if value == 1:
        ret = 1
    elif value == 2:
        ret = 2
    else:
        ret = 3
    return ret


if __name__ == '__main__':
    import sys
    dummy(int(sys.argv[1]))
