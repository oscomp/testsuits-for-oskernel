from test_base import TestBase
import re


class close_test(TestBase):
    def __init__(self):
        super().__init__("close", 2)

    def test(self, data):
        self.assert_ge(len(data), 1)
        self.assert_in_str("  close \d+ success.", data)