from test_base import TestBase
import re


class waitpid_test(TestBase):
    def __init__(self):
        super().__init__("waitpid", 4)

    def test(self, data):
        self.assert_ge(len(data), 3)
        self.assert_equal(data[0], "This is child process")
        self.assert_equal(data[1], "waitpid successfully.")
        self.assert_equal(data[2], "wstatus: 3")
