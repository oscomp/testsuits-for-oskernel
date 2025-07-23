import json
import sys


def parse_copyfile_test(output):
    ans = {}
    key = ""
    for line in output.split("\n"):
        if "START copy-file-range-test" in line:
            key = "CopyFileRangeTest " + line.split(" ")[6].strip('####')

        if "passed" in line and "copy-file-range-test1-musl" in key:
            ans[key] = 10
        if "passed" in line and "copy-file-range-test2-musl" in key:
            ans[key] = 8
        if "passed" in line and "copy-file-range-test3-musl" in key:
            ans[key] = 6
        if "passed" in line and "copy-file-range-test4-musl" in key:
            ans[key] = 6

    return ans

serial_out = sys.stdin.read()

copyfile_output = parse_copyfile_test(serial_out)


results = [{
    "name": k,
    "pass": 1,
    "total": 1,
    "score": v,
} for k, v in copyfile_output.items()]
print(json.dumps(results))
