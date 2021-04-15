from test_base import TestBase
import re


class mount_test(TestBase):
    def __init__(self):
        super().__init__("mount", 5)

    def test(self, data):
        self.assert_ge(len(data), 4)
        self.assert_equal(data[0], "Mounting dev:/dev/vda2 to ./mnt")
        self.assert_equal(data[1], "mount return: 0")
        self.assert_equal(data[2], "mount successfully")
        self.assert_equal(data[3], "umount return: 0")

