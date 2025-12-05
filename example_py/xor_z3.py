import json
import subprocess
from pathlib import Path
from z3 import Ints, Solver, And, Or, Not, sat

from reward_utils import branch_truth_from_coverage, trace_f1

MIN_REWARD = -1.0

def convert_c_lines_to_py_lines(trace):
    """
    trace: list[(line, dir)], dir in {'T', 'F'}
    return: list[(line, dir)], dir in {'T', 'F'}
    """
    C_TO_PY = {
        5: 5,
        7: 7,
        10: 9,
    }
    result = []
    for ln, d in trace:
        if ln not in C_TO_PY:
            raise RuntimeError(f'line {ln} is not an if')
        result.append((C_TO_PY[ln], d))
    return result


def verify_trace(trace, target_line=11):
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

    a, b = Ints('a b')
    s.add(a >= 0, a <= 1)
    s.add(b >= 0, b <= 1)

    cond5 = And(a == 1, b == 1)
    cond7 = And(a == 0, b == 0)
    cond9 = Or(cond5, cond7)

    cond_map = {
        5: cond5,
        7: cond7,
        9: cond9,
    }

    for ln, d in trace:
        cond = cond_map[ln]
        if d == 'T':
            s.add(cond)
        else:
            s.add(Not(cond))

    if target_line == 11:
        s.add(cond9)
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
    a_val = m[a].as_long()
    b_val = m[b].as_long()

    script_dir = Path(__file__).resolve().parent
    coverage_file = script_dir / '.coverage'
    coverage_json = script_dir / 'coverage.json'
    xor_py = script_dir / 'xor.py'

    coverage_file.unlink(missing_ok=True)
    coverage_json.unlink(missing_ok=True)

    try:
        subprocess.run(
            ['coverage', 'run', str(xor_py), str(a_val), str(b_val)],
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
        executed_lines = set(data['files']['xor.py']['executed_lines'])
    except (OSError, KeyError, json.JSONDecodeError) as exc:
        return {
            'sat': True,
            'reward': MIN_REWARD,
            'reason': f'failed to parse coverage: {exc}',
        }

    # If entered the if, i.e., the next line, set it to Taken
    true_line_map = {
        5: 6,
        7: 8,
        9: 10,
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
            (5, 'F'),
            (7, 'T'),
            (10, 'T')
        ]
    ))
