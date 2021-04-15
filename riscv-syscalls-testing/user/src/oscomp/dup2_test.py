from test_base import TestBase
import re


class dup2_test(TestBase):
    def __init__(self):
        super().__init__("dup2", 2)

    def test(self, data):
        self.assert_ge(len(data), 1)
        self.assert_equal("  from fd 100", data[0])