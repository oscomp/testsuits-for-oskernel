from test_base import TestBase
import re


class munmap_test(TestBase):
    def __init__(self):
        super().__init__("munmap", 4)

    def test(self, data):
        self.assert_ge(len(data), 3)
        r = re.findall(r"file len: (\d+)", data[0])
        if r:
            self.assert_ge(int(r[0]), 27)
        self.assert_equal(data[1], "munmap return: 0")
        self.assert_equal(data[2], "munmap successfully!")

