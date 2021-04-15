from test_base import TestBase
import re


class brk_test(TestBase):
    def __init__(self):
        super().__init__("brk", 3)

    def test(self, data):
        self.assert_ge(len(data), 3)
        p1 = "Before alloc,heap pos: (.+)"
        p2 = "After alloc,heap pos: (.+)"
        p3 = "Alloc again,heap pos: (.+)"
        line1 = re.findall(p1, data[0])
        line2 = re.findall(p2, data[1])
        line3 = re.findall(p3, data[2])
        if line1 == [] or line2 == [] or line3 == []:
            return
        a1 = int(line1[0], 10)
        a2 = int(line2[0], 10)
        a3 = int(line3[0], 10)
        self.assert_equal(a1 + 64, a2)
        self.assert_equal(a2 + 64, a3)