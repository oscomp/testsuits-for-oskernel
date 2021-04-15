from test_base import TestBase
import re


class getppid_test(TestBase):
    def __init__(self):
        super().__init__("getppid", 2)

    def test(self, data):
        self.assert_ge(len(data), 1)
        self.assert_in("  getppid success. ppid : ", data[0])
