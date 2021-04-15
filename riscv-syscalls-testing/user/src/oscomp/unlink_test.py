from test_base import TestBase
import re


class unlink_test(TestBase):
    def __init__(self):
        super().__init__("unlink", 2)

    def test(self, data):
        self.assert_ge(len(data), 1)
        self.assert_equal(data[0], "  unlink success!")

        


