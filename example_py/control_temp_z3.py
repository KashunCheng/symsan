import json
import subprocess
from pathlib import Path
from z3 import Ints, Solver, And, Not, If, BoolVal, sat

from reward_utils import branch_truth_from_coverage, trace_f1

MIN_REWARD = -1.0

def convert_c_lines_to_py_lines(trace):
    """
    trace: list[(line, dir)], dir in {'T', 'F'}
    return: list[(line, dir)], dir in {'T', 'F'}
    """
    C_TO_PY = {
        13: 10,
        15: 12,
        20: 16,
        23: 19,
        31: 26,
        35: 30,
        42: 34,
        44: 36,
        51: 41,
        57: 46
    }
    result = []
    for ln, d in trace:
        if ln not in C_TO_PY:
            raise RuntimeError(f'line {ln} is not an if')
        result.append((C_TO_PY[ln], d))
    return result


def verify_trace(trace, target_line=47):
    """
    trace: list[(line, dir)], dir in {'T', 'F'}
    target_line: target line
    return: dict(sat: bool, reward: float, reason: str)
    """

    # 1. invalid if lines
    try:
        trace = convert_c_lines_to_py_lines(trace)
    except RuntimeError as e:
        return {
            'sat': False,
            'reward': MIN_REWARD,
            'reason': str(e)
        }

    s = Solver()

    mode, temp, user_level, emergency = Ints('mode temp user_level emergency')
    s.add(emergency >= 0, emergency <= 1)

    sensor_ok = BoolVal(True)
    mode_is_1 = mode == 1
    mode_is_2 = mode == 2
    temp_high = And(temp > 30, sensor_ok)
    user_ge_5 = user_level >= 5
    user_ge_10 = user_level >= 10
    temp_comfort = And(And(temp >= 18, temp <= 26), sensor_ok)
    emergency_active = emergency == 1

    open_after_mode = If(
        mode_is_1,
        temp_high,
        If(
            mode_is_2,
            user_ge_5,
            temp_comfort
        )
    )
    locked_after_mode = If(
        mode_is_2,
        Not(user_ge_5),
        BoolVal(False)
    )

    open_after_emergency = If(
        emergency_active,
        If(user_ge_10, BoolVal(True), BoolVal(False)),
        open_after_mode
    )
    locked_after_emergency = If(
        emergency_active,
        If(user_ge_10, BoolVal(False), locked_after_mode),
        locked_after_mode
    )

    final_open = If(locked_after_emergency, BoolVal(False), open_after_emergency)

    cond_map = {
        10: mode_is_1,
        12: temp_high,
        16: mode_is_2,
        19: user_ge_5,
        26: Not(sensor_ok),
        30: temp_comfort,
        34: emergency_active,
        36: user_ge_10,
        41: locked_after_emergency,
        46: final_open,
    }

    for ln, d in trace:
        cond = cond_map[ln]
        if d == 'T':
            s.add(cond)
        else:
            s.add(Not(cond))

    if target_line == 47:
        s.add(final_open)
    else:
        return {
            'sat': False,
            'reward': MIN_REWARD,
            'reason': f'unsupported target line {target_line}'
        }

    # 5. SAT / UNSAT
    if s.check() != sat:
        return {
            'sat': False,
            'reward': MIN_REWARD,
            'reason': 'unsatisfiable trace'
        }

    # 6. coverage based method scoring (f1)

    m = s.model()
    mode_val = m[mode].as_long()
    temp_val = m[temp].as_long() if m[temp] is not None else 999
    user_level_val = m[user_level].as_long() if m[user_level] is not None else 999
    emergency_val = m[emergency].as_long()

    script_dir = Path(__file__).resolve().parent
    coverage_file = script_dir / '.coverage'
    coverage_json = script_dir / 'coverage.json'
    control_temp_py = script_dir / 'control_temp.py'

    coverage_file.unlink(missing_ok=True)
    coverage_json.unlink(missing_ok=True)

    try:
        subprocess.run(
            [
                'coverage',
                'run',
                str(control_temp_py),
                str(mode_val),
                str(temp_val),
                str(user_level_val),
                str(emergency_val),
            ],
            check=True,
            cwd=script_dir,
        )
        subprocess.run(['coverage', 'json'], check=True, cwd=script_dir)
    except subprocess.CalledProcessError as exc:
        return {
            'sat': True,
            'reward': MIN_REWARD,
            'reason': f'coverage failed: {exc}',
        }

    if not coverage_json.exists():
        return {
            'sat': True,
            'reward': MIN_REWARD,
            'reason': 'coverage report missing',
        }

    try:
        with coverage_json.open('r', encoding='utf-8') as fp:
            data = json.load(fp)
        executed_lines = set(data['files']['control_temp.py']['executed_lines'])
    except (OSError, KeyError, json.JSONDecodeError) as exc:
        return {
            'sat': True,
            'reward': MIN_REWARD,
            'reason': f'failed to parse coverage: {exc}',
        }

    # If entered the if, i.e., the next line, set it to Taken
    true_line_map = {
        10: 11,
        12: 13,
        16: 17,
        19: 20,
        26: 27,
        30: 31,
        34: 35,
        36: 37,
        41: 42,
        46: 47
    }

    actual_truth = branch_truth_from_coverage(executed_lines, true_line_map)
    reward = trace_f1(trace, actual_truth)

    return {
        'sat': True,
        'reward': reward,
        'reason': 'ok'
    }

if __name__ == '__main__':
    print(verify_trace(
        [
            (13, 'T'),
            (15, 'T'),
            (42, 'F'),
            (51, 'F'),
            (57, 'T')
        ]
    ))
