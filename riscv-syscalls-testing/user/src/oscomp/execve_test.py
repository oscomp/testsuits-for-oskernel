from test_base import TestBase
import re


class execve_test(TestBase):
    def __init__(self):
        super().__init__("execve", 3)

    def test(self, data):
        self.assert_ge(len(data), 2)
        self.assert_equal("  I am test_echo.", data[0])
        self.assert_equal("execve success.", data[1])