def control(mode, temp, userLevel, emergency):
    open_ = False
    locked = False
    sensorOk = True

    print("\n=== Control Function Debug ===")
    print(f"Input: mode={mode}, temp={temp}, userLevel={userLevel}, emergency={emergency}")
    print(f"Sensor status: {'OK' if sensorOk else 'FAIL'}")

    if mode == 1:
        print("Mode 1: Temperature-based control")
        if temp > 30 and sensorOk:
            open_ = True
        else:
            open_ = False
    elif mode == 2:
        print("Mode 2: User level control")
        locked = True
        if userLevel >= 5:
            open_ = True
            locked = False
        else:
            pass  # Do nothing
    else:
        print("Mode 3 (default): Normal operation")
        if not sensorOk:
            print("[ERROR] Bad sensor", file=sys.stderr)
            open_ = False
        else:
            if 18 <= temp <= 26:
                open_ = True
            else:
                open_ = False
    if emergency:
        print("Emergency mode activated!")
        if userLevel >= 10:
            open_ = True
            locked = False
        else:
            open_ = False
    if locked:
        print("System is LOCKED")
        open_ = False
    print(f"Final state: open={int(open_)}, locked={int(locked)}")
    print("==============================\n")
    if open_:
        print("[OK] Door is OPEN")
    else:
        print("[INFO] Door remains CLOSED")
    return open_


if __name__ == "__main__":
    import sys
    if len(sys.argv) != 5:
        print("Usage: python control.py <mode> <temp> <userLevel> <emergency>")
        sys.exit(1)

    mode = int(sys.argv[1])
    temp = int(sys.argv[2])
    userLevel = int(sys.argv[3])
    emergency = bool(int(sys.argv[4]))  # Accepts 0/1

    control(mode, temp, userLevel, emergency)
