import json
import sys


def parse_interrupt_test(output):
    ans = {}
    key = ""
    for line in output.split("\n"):
        if "Cloning into 'proj'..." in line:
            key = "git basic network"
        elif "Cloning into 'proj1'..." in line:
            key = "git multithread network"

        if not key:
            continue

        if not "Receiving objects" in line:
            continue
        res = line.strip().split()
        if not "100%" in res[2]:
            continue
        ans[key] = 15 + float(res[7])*10/10
    return ans

serial_out = sys.stdin.read()

interrupt_output = parse_interrupt_test(serial_out)


results = [{
    "name": k,
    "pass": 1,
    "total": 1,
    "score": v,
} for k, v in interrupt_output.items()]
print(json.dumps(results))
