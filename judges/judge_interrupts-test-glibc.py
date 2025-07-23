import json
import sys


def parse_interrupt_test(output):
    ans = {}
    key = ""
    for line in output.split("\n"):
        if "START interrupts-test" in line:
            key = "InterruptsTest " + line.split(" ")[6].strip('####')
        print(key)    
        if "passed" in line and "interrupts-test1-glibc" in key:
            ans[key] = 15
        if "passed" in line and "interrupts-test2-glibc" in key:
            ans[key] = 15

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
