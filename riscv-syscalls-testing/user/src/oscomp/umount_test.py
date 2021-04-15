from test_base import TestBase
import re


class umount_test(TestBase):
    def __init__(self):
        super().__init__("umount", 5)

    def test(self, data):
        self.assert_ge(len(data), 4)
        self.assert_equal(data[0], "Mounting dev:/dev/vda2 to ./mnt")
        self.assert_equal("mount return: 0", data[1])
        self.assert_equal("umount success.", data[2])
        self.assert_equal("return: 0", data[3])

