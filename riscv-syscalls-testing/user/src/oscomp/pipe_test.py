from test_base import TestBase
import re


class pipe_test(TestBase):
    def __init__(self):
        super().__init__("pipe", 4)

    def test(self, data):
        self.assert_ge(len(data), 3)
        cpid0 = False
        cpid1 = False
        for line in data[:3]:
            if line == "cpid: 0":
                cpid0 = True
                continue
            r = re.findall(r"cpid: (\d+)", line)
            if r and int(r[0]) > 0:
                cpid1 = True
                continue
        self.assert_equal(cpid0, True)
        self.assert_equal(cpid1, True)
        self.assert_equal(data[2], "  Write to pipe successfully.")
        



