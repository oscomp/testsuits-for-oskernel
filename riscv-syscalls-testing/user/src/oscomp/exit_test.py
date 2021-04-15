from test_base import TestBase
import re


class exit_test(TestBase):
    def __init__(self):
        super().__init__("exit", 2)

    def test(self, data):
        self.assert_ge(len(data), 1)
        self.assert_equal("exit OK.", data[0])