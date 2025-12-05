from typing import Dict, Iterable, Mapping, Sequence, Tuple, Union

# Trace entries are (line, 'T' or 'F')
Trace = Sequence[Tuple[int, str]]
TruthMap = Dict[int, bool]
TrueBranchMap = Mapping[int, Union[int, Iterable[int]]]


def _normalize_true_lines(true_lines: Union[int, Iterable[int]]) -> Tuple[int, ...]:
    if isinstance(true_lines, int):
        return (true_lines,)
    return tuple(true_lines)


def branch_truth_from_coverage(executed_lines: Iterable[int],
                               true_branch_map: TrueBranchMap) -> TruthMap:
    """
    executed_lines: iterable of executed source lines reported by coverage.py
    true_branch_map: maps the condition line to one or more lines executed ONLY
                     when the branch evaluates to True
    returns: mapping from condition line to the actual boolean result
    """
    executed = set(executed_lines)
    truth: TruthMap = {}
    for cond_line, true_lines in true_branch_map.items():
        if cond_line not in executed:
            continue
        normalized = _normalize_true_lines(true_lines)
        truth[cond_line] = any(line in executed for line in normalized)
    return truth


def trace_f1(trace: Trace, actual_truth: TruthMap) -> float:
    """
    trace: proposed sequence of (line, direction) pairs
    actual_truth: mapping from condition line to its actual boolean value
    returns: F1 score between provided trace and actual truth
    """
    provided = len(trace)
    actual_count = len(actual_truth)

    matches = 0
    for line, direction in trace:
        actual_dir = actual_truth.get(line)
        if actual_dir is None:
            continue
        if actual_dir == (direction == 'T'):
            matches += 1

    precision = matches / provided if provided else 0.0
    recall = matches / actual_count if actual_count else 0.0

    if precision + recall == 0.0:
        return 0.0
    return 2.0 * precision * recall / (precision + recall)
