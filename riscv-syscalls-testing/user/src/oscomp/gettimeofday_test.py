from test_base import TestBase
import re


class gettimeofday_test(TestBase):
    def __init__(self):
        super().__init__("gettimeofday", 3)

    def test(self, data):
        self.assert_ge(len(data), 3)
        self.assert_equal("gettimeofday success.", data[0])
        res = re.findall(r"interval: (\d+)", data[2])
        if res:
            self.assert_great(int(res[0]), 0)