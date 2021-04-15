import re
from typing import List


class TestBase:
    class AssertFail(RuntimeError):
        pass

    def __init__(self, name, count):
        self.name = "test_" + name
        self.count = count
        self.result = []

    def test(self, data):
        pass

    def assert_util(self, func, rep, msg, *args):
        self.result.append({
            "rep": rep,
            "res": func(*args),
            "arg": args,
            "msg": msg
        })
        if not self.result[-1]["res"]:
            raise self.AssertFail()

    def assert_equal(self, v1, v2, msg=''):
        self.assert_util(lambda a, b: a == b, "=", msg, v1, v2)

    def assert_not_equal(self, v1, v2, msg=''):
        self.assert_util(lambda a, b: a != b, "!=", msg, v1, v2)

    def assert_great(self, v1, v2, msg=''):
        self.assert_util(lambda a, b: a > b, ">", msg, v1, v2)
    
    def assert_ge(self, v1, v2, msg=''):
        self.assert_util(lambda a, b: a >= b, ">=", msg, v1, v2)

    def assert_in_str(self, v1, v2, msg=''):
        def _fun(a: str, b: List[str]):
            pattern = re.compile(a)
            for line in b:
                if re.search(pattern, line) is not None:
                    return True
            return False
        self.assert_util(_fun, "in", msg, v1, v2)

    def assert_in(self, v1, v2, msg=''):
        self.assert_util(lambda a, b: a in b, "in", msg, v1, v2)

    def start(self, data):
        self.result = []
        try:
            self.test(data)
        except Exception:
            pass
    
    def get_result(self):
        return {
            "name": self.name,
            "results": self.result,
            "all": self.count,
            "passed": len([x for x in self.result if x['res']])
        }

