from test_base import TestBase
import re


class mount_test(TestBase):
    def __init__(self):
        super().__init__("mount", 5)

    def test(self, data):
        self.assert_ge(len(data), 4)
        r = re.findall(r"Mounting dev:(.+) to ./mnt", data[0])
        self.assert_equal(len(r) > 0, True)
        self.assert_equal(data[1], "mount return: 0")
        self.assert_equal(data[2], "mount successfully")
        self.assert_equal(data[3], "umount return: 0")

