from test_base import TestBase
import re


class openat_test(TestBase):
    def __init__(self):
        super().__init__("openat", 4)

    def test(self, data):
        self.assert_ge(len(data), 3)
        r = re.findall(r"open dir fd: (\d+)", data[0])
        if r:
            self.assert_great(int(r[0]), 1)
        r1 = re.findall(r"openat fd: (\d+)", data[1])
        if r1:
            self.assert_great(int(r1[0]), int(r[0]))
        self.assert_equal(data[2], "openat success.")


