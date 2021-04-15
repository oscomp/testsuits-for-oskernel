from test_base import TestBase
import re


class fstat_test(TestBase):
    def __init__(self):
        super().__init__("fstat", 3)

    def test(self, data):
        self.assert_ge(len(data), 2)
        res = re.findall("fstat ret: (\d+)", data[0])
        if res:
            self.assert_equal(res[0], "0")
        res = re.findall(r"fstat: dev: \d+, inode: \d+, mode: (\d+), nlink: (\d+), size: \d+, atime: \d+, mtime: \d+, ctime: \d+", data[1])
        if res:
            self.assert_equal(res[0][1], "1")