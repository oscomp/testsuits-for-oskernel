from test_base import TestBase
import re


class clone_test(TestBase):
    def __init__(self):
        super().__init__("clone", 4)

    def test(self, data):
        self.assert_ge(len(data), 3)
        self.assert_in_str("  Child says successfully!", data)
        self.assert_in_str("pid:\d+", data)
        self.assert_in_str("clone process successfully.", data)