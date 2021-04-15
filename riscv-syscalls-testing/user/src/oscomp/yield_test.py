from test_base import TestBase
import re


class yield_test(TestBase):
    def __init__(self):
        super().__init__("yield", 4)

    def test(self, data):
        self.assert_equal(len(data), 15)
        lst = ''.join(data)
        cnt = {}
        begin = {}
        end ={}
        idx = 0
        for c in lst:
            if c not in ('A', 'B', 'C'):
                continue
            if not c in begin:
                begin[c] = idx
            end[c] = idx
            if not c in cnt:
                cnt[c] = 0
            cnt[c] += 1
            idx += 1
        # self.assert_equal(cnt[' '], 15)
        self.assert_equal(cnt['A'], 50)
        self.assert_equal(cnt['B'], 50)
        self.assert_equal(cnt['C'], 50)
        # self.assert_equal(cnt['['], 15)
        # self.assert_equal(cnt['/'], 15)
        # self.assert_equal(cnt[']'], 15)
        


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