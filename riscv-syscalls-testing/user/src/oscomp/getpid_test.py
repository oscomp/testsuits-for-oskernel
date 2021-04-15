from test_base import TestBase
import re


class getpid_test(TestBase):
    def __init__(self):
        super().__init__("getpid", 3)

    def test(self, data):
        self.assert_ge(len(data), 2)
        self.assert_equal(data[0], "getpid success.")
        r = re.findall(r"pid = (\d+)", data[1])
        if r:
            self.assert_great(int(r[0]), 0)