#include <stdio.h>
#include <stdlib.h>

static int ensure_bytes(size_t want) {
  unsigned char buf[3];
  size_t n = fread(buf, 1, want, stdin);
  if (n != want) {
    return -1;
  }
  int a = buf[0] - '0';
  int b = buf[1] - '0';
  int c = buf[2] - '0';

  int score = 0;

  if (a == 4) {
    score += 1;
  }

  if (b == 2) {
    score += 2;
  }

  if (c == 7) {
    score += 4;
  }

  if ((a + b) == 10) {
    score += 8;
  }

  if (b > c) {
    score += 16;
  } else {
    score -= 3;
  }

  if (a == b) {
    if (c == 0) {
      score += 32;
    } else {
      score -= 5;
    }
  } else {
    if (a > c) {
      score += 64;
    }
  }

  if ((a ^ b) == 5) {
    score += 128;
  }

  if ((b + c) == (a + 2)) {
    score += 256;
  }

  printf("Complex scores: %d (%d %d %d)\n", score, a, b, c);

  if (score > 350) {
    return 0;
  } else if (score > 200) {
    return 1;
  } else if (score > 100) {
    return 2;
  }
  return 3;
}

int main(void) {
  int ret = ensure_bytes(3);
  if (ret < 0) {
    return 9;
  }
  return ret;
}
