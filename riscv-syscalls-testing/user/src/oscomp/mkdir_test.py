from test_base import TestBase
import re


class mkdir_test(TestBase):
    def __init__(self):
        super().__init__("mkdir", 3)

    def test(self, data):
        self.assert_ge(len(data), 2)
        self.assert_in("mkdir ret:", data[0])
        self.assert_in("  mkdir success.", data[1])
