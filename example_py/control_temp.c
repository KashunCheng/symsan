#include <stdbool.h>
#include <stdio.h>

bool control(int mode, int temp, int userLevel, bool emergency) {
  bool open = false;
  bool locked = false;
  bool sensorOk = true;

  printf("\n=== Control Function Debug ===\n");
  printf("Input: mode=%d, temp=%d, userLevel=%d, emergency=%d\n", mode, temp, userLevel, emergency);
  printf("Sensor status: %s\n", sensorOk ? "OK" : "FAIL");

  if (mode == 1) {
    printf("Mode 1: Temperature-based control\n");
    if (temp > 30 && sensorOk) {
      open = true;
    } else {
      open = false;
    }
  } else if (mode == 2) {
    printf("Mode 2: User level control\n");
    locked = true;
    if (userLevel >= 5) {
      open = true;
      locked = false;
    } else {
      // Do nothing
    }
  } else {
    printf("Mode 3 (default): Normal operation\n");
    if (!sensorOk) {
      fprintf(stderr, "[ERROR] Bad sensor\n");
      open = false;
    } else {
      if (temp >= 18 && temp <= 26) {
        open = true;
      } else {
        open = false;
      }
    }
  }
  if (emergency) {
    printf("Emergency mode activated!\n");
    if (userLevel >= 10) {
      open = true;
      locked = false;
    } else {
      open = false;
    }
  }
  if (locked) {
    printf("System is LOCKED\n");
    open = false;
  }
  printf("Final state: open=%d, locked=%d\n", open, locked);
  printf("==============================\n\n");
  if (open) {
    printf("[OK] Door is OPEN\n");
  } else {
    printf("[INFO] Door remains CLOSED\n");
  }
  return open;
}