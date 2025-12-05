import json
import subprocess
from pathlib import Path
from z3 import Ints, Solver, Not, sat

from reward_utils import branch_truth_from_coverage, trace_f1

MIN_REWARD = -1.0

def convert_c_lines_to_py_lines(trace):
    """
    trace: list[(line, dir)], dir in {'T', 'F'}
    return: list[(line, dir)], dir in {'T', 'F'}
    """
    C_TO_PY = {
        3: 3,
        5: 5
    }
    result = []
    for ln, d in trace:
        if ln not in C_TO_PY:
            raise RuntimeError(f'line {ln} is not an if')
        result.append((C_TO_PY[ln], d))
    return result


def verify_trace(trace, target_line=8):
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

    value, = Ints('value')

    cond3 = value == 1
    cond5 = value == 2

    cond_map = {
        3: cond3,
        5: cond5,
    }

    for ln, d in trace:
        cond = cond_map[ln]
        if d == 'T':
            s.add(cond)
        else:
            s.add(Not(cond))

    if target_line == 8:
        s.add(Not(cond3), Not(cond5))
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
    value_val = m[value].as_long()

    script_dir = Path(__file__).resolve().parent
    coverage_file = script_dir / '.coverage'
    coverage_json = script_dir / 'coverage.json'
    dummy_py = script_dir / 'dummy.py'

    coverage_file.unlink(missing_ok=True)
    coverage_json.unlink(missing_ok=True)

    try:
        subprocess.run(
            ['coverage', 'run', str(dummy_py), str(value_val)],
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
        executed_lines = set(data['files']['dummy.py']['executed_lines'])
    except (OSError, KeyError, json.JSONDecodeError) as exc:
        return {
            'sat': True,
            'reward': MIN_REWARD,
            'reason': f'failed to parse coverage: {exc}',
        }

    # If entered the if, i.e., the next line, set it to Taken
    true_line_map = {
        3: 4,
        5: 6,
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
            (3, 'F'),
            (5, 'F'),
        ]
    ))
