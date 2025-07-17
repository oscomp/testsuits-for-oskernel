import json
import sys


def parse_copyfile_test(output):
    ans = {}
    key = ""
    for line in output.split("\n"):
        if "START copy-file-range-test" in line:
            key = "copy-file-range-test " + line.split(" ")[7].strip('####')
        print(key)    
        if "passed" in line and "copy-file-range-test 1" in key:
            ans[key] = 10
        if "passed" in line and "copy-file-range-test 2" in key:
            ans[key] = 8
        if "passed" in line and "copy-file-range-test 3" in key:
            ans[key] = 6
        if "passed" in line  and "copy-file-range-test 4" in key:
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
