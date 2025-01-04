import json

"""
    This file uses to load configuration file.
"""


class BasicConf:
    def __init__(self):
        pass

    def __readConf(self):
        try:
            with open(self.ConfPath, "r") as f:
                data = f.read()
                return json.loads(data)
        except Exception as e:
            print("Conf File Cannot Load! ", str(e))
            raise e

    def _load(self):
        self.conf = self.__readConf()


class Conf(BasicConf):
    def __init__(self):
        super().__init__()
        self.ConfPath = "./conf/conf.json"
        self.conf = None

        self._load()

    def Get_ECconf(self):
        return self.conf["EC_Policy"]


if __name__ == "__main__":
    print(Conf().Get_ECconf())
