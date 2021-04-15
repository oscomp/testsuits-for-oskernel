from test_base import TestBase
import re


class chdir_test(TestBase):
    def __init__(self):
        super().__init__("chdir", 3)

    def test(self, data):
        self.assert_ge(len(data), 2)
        p1 = r"chdir ret: (\d)+"
        r1 = re.findall(p1, data[0])
        if r1:
            self.assert_equal(r1[0], "0")
        self.assert_in("test_chdir", data[1])