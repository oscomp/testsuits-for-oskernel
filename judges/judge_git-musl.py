import json
import sys


def parse_interrupt_test(output):
    ans = {}
    key = ""
    for line in output.split("\n"):
        if "These are common Git commands used in various situations" in line:
            ans["git help"] = 6.25
        if "Initialized empty Git repository" in line:
            ans["git init"] = 6.25
        if "1 file changed" in line:
            ans["git commit"] = 6.25
        if "Author:" in line:
            ans["git log"] = 6.25                        
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
