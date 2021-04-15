from test_base import TestBase
import re


class open_test(TestBase):
    def __init__(self):
        super().__init__("open", 3)

    def test(self, data):
        self.assert_ge(len(data), 2)
        self.assert_equal("Hi, this is a text file.", data[0])
        self.assert_equal("syscalls testing success!", data[1])


