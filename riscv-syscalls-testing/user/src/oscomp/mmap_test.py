from test_base import TestBase
import re


class mmap_test(TestBase):
    def __init__(self):
        super().__init__("mmap", 3)

    def test(self, data):
        self.assert_ge(len(data), 2)
        r = re.findall(r"file len: (\d+)", data[0])
        if r:
            self.assert_ge(int(r[0]), 27)
        self.assert_equal("mmap content:   Hello, mmap successfully!", data[1])

