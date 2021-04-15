# 用于检查测试脚本是否有低级错误
import os
import re

expermit = ["yield_test.py"]

dir = os.path.join(os.getcwd(), "user", "src", "oscomp")
print(dir)
for file_name in os.listdir(dir):
    if file_name[-3:] != ".py":
        continue
    if file_name in expermit:
        continue
    file_path = os.path.join(dir, file_name)
    print(file_path)
    file_test_name = re.findall(r"([a-zA-Z0-9]+)_test\.py", file_name)
    if not file_test_name:
        continue
    file_test_name = file_test_name[0]
    content = open(file_path, encoding="utf-8").read()
    class_test_name = re.findall(r"class ([a-zA-Z0-9]+)_test\(TestBase\)", content)[0]
    test_name, test_count = re.findall(r"super\(\)\.__init__\(\"([a-zA-Z0-9]+)\", (\d+)\)", content)[0]
    asserts = len(re.findall(r"self\.assert_", content))
    assert class_test_name == file_test_name == test_name
    assert int(test_count) == asserts 

print("OK!")