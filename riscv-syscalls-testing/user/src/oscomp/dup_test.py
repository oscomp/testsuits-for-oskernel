from test_base import TestBase
import re


class dup_test(TestBase):
    def __init__(self):
        super().__init__("dup", 2)

    def test(self, data):
        self.assert_ge(len(data), 1)
        res = re.findall("  new fd is (\d+).", data[0])
        if res:
            new_fd = int(res[0])
            self.assert_not_equal(new_fd, 1)