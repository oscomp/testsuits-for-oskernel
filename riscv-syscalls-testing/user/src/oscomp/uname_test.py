from test_base import TestBase
import re


class uname_test(TestBase):
    def __init__(self):
        super().__init__("uname", 2)

    def test(self, data):
        self.assert_ge(len(data), 1)
        self.assert_in("Uname: ", data[0])
