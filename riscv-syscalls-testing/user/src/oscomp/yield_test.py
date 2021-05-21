from test_base import TestBase
import re

class yield_test(TestBase):
    def __init__(self):
        super().__init__("yield", 4)

    def test(self, data):
        self.assert_equal(len(data), 15)
        lst = ''.join(data)
        cnt = {'0': 0, '1': 0, '2': 0, '3': 0, '4': 0}
        for c in lst:
            if c not in ('0', '1', '2', '3', '4'):
                continue
            cnt[c] += 1
        self.assert_ge(cnt['0'], 3)
        self.assert_ge(cnt['1'], 3)
        self.assert_ge(cnt['2'], 3)
        


# BBBBBBBBBB [1/5]
# CCCCCCCCCC [1/5]
# BBBBBBBBBB [2/5]
# CCCCCCCCCC [2/5]
# AAAAAAAAAA [1/5]
# CCCCCCCCCC [3/5]
# BBBBBBBBBB [3/5]
# AAAAAAAAAA [2/5]
# BBBBBBBBBB [4/5]
# CCCCCCCCCC [4/5]
# AAAAAAAAAA [3/5]
# BBBBBBBBBB [5/5]
# CCCCCCCCCC [5/5]
# AAAAAAAAAA [4/5]
# AAAAAAAAAA [5/5]