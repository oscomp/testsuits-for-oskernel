import json
import sys


def parse_splice_test(output):
    ans = {}
    key = ""
    for line in output.split("\n"):
        if "START splice-test" in line:
            key = "SpliceTest " + line.split(" ")[6].strip('####')

        if "got -1" in line and "splice-test1-glibc" in key:
            ans[key] = 8
        if "got 0" in line and "splice-test2-glibc" in key:
            ans[key] = 8
        if "got 5" in line and "splice-test3-glibc" in key:
            ans[key] = 8
        if "got 3" in line and "splice-test4-glibc" in key:
            ans[key] = 8            
        if "got -1" in line and "splice-test5-glibc" in key:
            ans[key] = 8            
    return ans

serial_out = sys.stdin.read()

splice_output = parse_splice_test(serial_out)


results = [{
    "name": k,
    "pass": 1,
    "total": 1,
    "score": v,
} for k, v in splice_output.items()]
print(json.dumps(results))
