from test_base import TestBase
import re


class gettimeofday_test(TestBase):
    def __init__(self):
        super().__init__("gettimeofday", 3)

    def test(self, data):
        self.assert_ge(len(data), 2)
        self.assert_equal("gettimeofday successfully", data[0])
        res = re.findall(r"time: (\d+)", data[1])
        if res:
            self.assert_great(int(res[0]), 0)