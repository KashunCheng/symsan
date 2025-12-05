int xor(int a, int b) {
  a %= 2;
  b %= 2;
  int ret = 0;
  if (a && b) {
    ret = 1;
  } else if (!a && !b) {
    ret = 1;
  }
  if (ret) {
    return 2;
  }
  return 0;
}