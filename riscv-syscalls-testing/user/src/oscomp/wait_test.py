from test_base import TestBase
import re


class wait_test(TestBase):
    def __init__(self):
        super().__init__("wait", 4)

    def test(self, data):
        self.assert_ge(len(data), 3)
        self.assert_equal(data[0], "This is child process")
        self.assert_equal(data[1], "wait child success.")
        self.assert_equal(data[2], "wstatus: 0")


