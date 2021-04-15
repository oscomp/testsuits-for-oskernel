from test_base import TestBase
import re


class fork_test(TestBase):
    def __init__(self):
        super().__init__("fork", 3)

    def test(self, data):
        self.assert_ge(len(data), 2)
        self.assert_in_str("  parent process\. wstatus:\d+", data)
        self.assert_in_str("  child process", data)