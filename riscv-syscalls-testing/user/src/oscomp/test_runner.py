import json
import sys
import os
import re
import importlib

file_path = os.path.split(os.path.realpath(__file__))[0]

test_file_pattern = re.compile(r"(.+)_test\.py")

tests = []

for name in os.listdir(file_path):
    t = test_file_pattern.findall(name)
    if not t:
        continue
    t = t[0]
    tests.append(getattr(importlib.import_module(f"{t}_test"), t + "_test")())


runner = {x.name: x for x in tests}

def get_runner(name):
    # return runner.get(name, runner.get("test_"+name, runner[name+"_test"]))
    return runner[name]
# print(runner)


if __name__ == '__main__':
    file_name = sys.argv[1]
    serial_out = open(file_name).read()
    serial_out = serial_out.split('\n')

    test_name = None
    state = 0
    data = []
    for line in serial_out:
        if line in ('', '\n'):
            continue
        if state == 0:
            # 寻找测试样例开头
            if "========== START " in line:
                # print(line, file=sys.stderr)
                test_name = line.replace("=", '').replace(" ", "").replace("START", "")
                if data:
                    # 只找到了开头没找到结尾，说明某个样例内部使用assert提前退出
                    get_runner(test_name).start(data)
                data = []
                state = 1
        elif state == 1:
            if "========== END " in line:
                # 测试样例结尾
                get_runner(test_name).start(data)
                state = 0
                data = []
                continue
            # 测试样例中间
            data.append(line)
    test_results = [x.get_result() for x in tests]
    print(json.dumps(test_results))
