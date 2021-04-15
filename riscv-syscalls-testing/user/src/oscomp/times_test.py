from test_base import TestBase
import re


class times_test(TestBase):
    def __init__(self):
        super().__init__("times", 6)

    def test(self, data):
        self.assert_ge(len(data), 2)
        self.assert_equal(data[0], "mytimes success")
        r = re.findall(r"\{tms_utime:(.+), tms_stime:(.+), tms_cutime:(.+), tms_cstime:(.+)}", data[1])
        if r:
            self.assert_ge(int(r[0][0]), 0)
            self.assert_ge(int(r[0][1]), 0)
            self.assert_ge(int(r[0][2]), 0)
            self.assert_ge(int(r[0][3]), 0)

