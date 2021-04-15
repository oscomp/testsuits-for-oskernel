from test_base import TestBase
import re


class getdents_test(TestBase):
    def __init__(self):
        super().__init__("getdents", 5)

    def test(self, data):
        self.assert_ge(len(data), 4)
        r = re.findall(r"open fd:(\d+)", data[0])
        if r:
            self.assert_great(int(r[0]), 1)
        r = re.findall(r"getdents fd:(\d+)", data[1])
        if r:
            self.assert_great(int(r[0]), 1)
        self.assert_equal("getdents success.", data[2])
        self.assert_equal(".", data[3])
        