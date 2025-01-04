import ctypes
import math
import os.path
import queue
import shutil
import threading
import time
from ctypes import *

from Crypto import filehash
from Loadconf import Conf
from basefunctons import *

Replication = 0
RS = 1
ZJC_RSBased = 2
ZJC_XorBased = 3
LRC = 4


class EC:
    def __init__(self):
        self.params = None
        self.base_path = None
        self.name = None
        self.os_type = GetOS()
        self.conf = Conf().Get_ECconf()
        self.filesize = -1

    def Encode(self, **kwargs):
        return Exception("[ERROR]: Invalid EC Policy Selection.")

    def Decode(self, **kwargs):
        return Exception("[ERROR]: Invalid EC Policy Selection.")

    def split(self, **kwargs):
        pass

    def Truncate(self):
        with open(os.path.join(self.base_path, "decode.dat"), 'rb+') as f:
            f.truncate(self.filesize)

    def emerge(self):
        try:
            Flist = Files_list(os.path.join(self.base_path, "blocks"))
            Flist.sort(key=lambda x: int(x[6:-4]))
            with open(os.path.join(self.base_path, "decode.dat"), "wb") as f:
                for file in Flist:
                    with open(os.path.join(self.base_path, "blocks", file), "rb") as b:
                        f.write(b.read())
                        f.flush()
                        b.close()
            return 0
        except Exception as e:
            print("[ERROR]: Emerging File %s Failed. %s" % (self.params["path"], e))
            return -1


class EC_Policy(EC):
    def __init__(self, selection, params):
        super().__init__()
        if selection == Replication:
            self.__class__ = Replicate
            self.__init__(params)
        elif selection == RS:
            self.__class__ = RSEC
            self.__init__(params)
        elif selection == ZJC_RSBased:
            self.__class__ = ZJC_RS
            self.__init__(params)
        elif selection == ZJC_XorBased:
            self.__class__ = ZJC_Xor
            self.__init__(params)
        elif selection == LRC:
            self.__class__ = LRC_EC
            self.__init__(params)
        else:
            raise Exception("[ERROR]: Invalid EC Policy Selection. Your selection is %s." % selection)


class Replicate(EC):
    def __init__(self, params):
        super().__init__()
        self.name = 'Replicate'
        self.params = params
        self.conf = self.conf["replication"]
        self.factor = self.conf["replication-factor"]
        self.block_size = self.conf["block-size"]
        self.base_path = os.path.dirname(self.params["path"])

    def split(self):
        fsize = GetFileSize(self.params["path"])
        if fsize == -1:
            print("[ERROR]: The File %s Operate Failed." % self.params["path"])
            return -1
        try:
            self.block_num = math.ceil(fsize / self.block_size)
            with open(self.params["path"], "rb") as f:
                for i in range(self.block_num):
                    with open(os.path.join(self.base_path, "blocks", "block_%d.dat" % i), "wb") as b:
                        b.write(f.read(self.block_size))
            return 0
        except Exception as e:
            print("[ERROR]: Splitting File %s Failed. %s" % (self.params["path"], e))
            return -1

    def Encode(self):
        block_path = os.path.join(self.base_path, "blocks")
        if not os.path.exists(block_path):
            os.mkdir(block_path)
        return self.split()

    def Decode(self):
        return self.emerge()


class RSEC(EC):
    def __init__(self, params):
        super().__init__()
        self.name = 'RS'
        self.params = params
        self.block_num = int(self.conf["RS"]["n"])
        self.k = self.conf["RS"]["k"]
        self.block_size = -1
        self.filesize = GetFileSize(self.params["path"])
        self.base_path = os.path.dirname(self.params["path"])
        self.dllpath = os.path.join(os.getcwd(), "include", "RS")
        if self.os_type == "Windows":
            self.dllpath = os.path.join(self.dllpath, "rs_ec.dll")
        else:
            self.dllpath = os.path.join(self.dllpath, "rs_ec.so")
        self.dll = cdll.LoadLibrary(self.dllpath)
        self.__initdll()

    def __initdll(self):
        self.dll.encode_file.argtypes = (
            POINTER(c_char_p),
            c_int,
            c_int
        )
        self.dll.encode_file.restype = c_int

        self.dll.decode_file.argtypes = (
            POINTER(c_char_p),
            c_int,
            c_int,
            POINTER(c_int),
            c_int
        )
        self.dll.decode_file.restype = c_int

    def split(self):
        fsize = GetFileSize(self.params["path"])
        if fsize == -1:
            print("[ERROR]: The File %s Operate Failed." % self.params["path"])
            return -1
        try:
            self.block_size = math.ceil(fsize / self.k)
            with open(self.params["path"], "rb") as f:
                for i in range(self.k):
                    with open(os.path.join(self.base_path, "blocks", "block_%d.dat" % i), "wb") as b:
                        b.write(f.read(self.block_size))
            return 0
        except Exception as e:
            print("[ERROR]: Splitting File %s Failed. %s" % (self.params["path"], e))
            return -1

    def __GetBlocks(self, block_path, parity_path):
        data_blocks = ["%s" %
                       (os.path.join(block_path, "block_%d.dat" % i))
                       for i in range(self.k)
                       ]
        parity_blocks = ["%s" %
                         (os.path.join(parity_path, "parity_%d.dat" % i))
                         for i in range(self.block_num - self.k)
                         ]

        return self._TransferBtye(data_blocks + parity_blocks)

    def _TransferBtye(self, source):
        res = []
        for src in source:
            res.append(src.encode("utf-8"))
        return res

    def SubEncode(self, block_path, parity_path):
        blocklist = self.__GetBlocks(block_path, parity_path)
        blocklist = ((c_char_p) * len(blocklist))(*blocklist)
        return self.dll.encode_file(blocklist, self.block_num, self.k)

    def Encode(self, ):
        block_path = os.path.join(self.base_path, "blocks")
        if not os.path.exists(block_path):
            os.mkdir(block_path)
        parity_path = os.path.join(self.base_path, "parity")
        if not os.path.exists(parity_path):
            os.mkdir(parity_path)
        scode = self.split()
        if scode == 0:
            return self.SubEncode(block_path, parity_path)
        return scode

    def Decode(self, loss_idx):
        block_path = os.path.join(self.base_path, "blocks")
        parity_path = os.path.join(self.base_path, "parity")
        loss_num = len(loss_idx)
        blocklist = self.__GetBlocks(block_path, parity_path)
        blocklist = ((c_char_p) * len(blocklist))(*blocklist)
        loss_idx = ((c_int) * loss_num)(*loss_idx)
        return self.dll.decode_file(blocklist, self.block_num, self.k, loss_idx, loss_num)


class ZJC_RS(EC):
    def __init__(self, params):
        super().__init__()
        self.block_size = -1
        self.block_num = -1
        self.name = 'ZJC_RSBased'
        self.params = params
        self.filesize = GetFileSize(self.params["path"])
        self.conf = self.conf["ZJC-RS"]
        self.dllpath = os.path.join(os.getcwd(), "include", "ZJC_RS")
        if self.os_type == "Windows":
            self.dllpath = os.path.join(self.dllpath, "zjc_rs.dll")
        else:
            self.dllpath = os.path.join(self.dllpath, "zjc_rs.so")
        self.dll = cdll.LoadLibrary(self.dllpath)
        self.base_path = os.path.dirname(self.params["path"])

    def split(self):
        fsize = GetFileSize(self.params["path"])
        if fsize == -1:
            print("[ERROR]: The File %s Operate Failed." % self.params["path"])
            return -1
        try:
            self.block_num = self.conf["n"]
            self.block_size = math.ceil(fsize / self.block_num)
            with open(self.params["path"], "rb") as f:
                for i in range(self.block_num):
                    with open(os.path.join(self.base_path, "blocks", "block_%d.dat" % i), "wb") as b:
                        b.write(f.read(self.block_size))
            return 0
        except Exception as e:
            print("[ERROR]: Splitting File %s Failed. %s" % (self.params["path"], e))
            return -1

    def SubEncode(self, idx):
        lp = TransferString(os.path.join(self.base_path, "blocks", "block_%d.dat" % idx))
        rp = TransferString(os.path.join(
            self.base_path, "blocks", "block_%d.dat" % ((idx + 1) % self.block_num)
        ))
        pp = TransferString(os.path.join(
            self.base_path, "parity", "parity_%d_%d.dat" %
                                      (idx, (idx + 1) % self.block_num)
        ))
        result = self.dll.rs_encodef(lp, rp, pp, self.conf["RS-settings"]["k"])
        if result != 0:
            print("[ERROR]: Encoding Error AT block-%d." % idx)

    def Encode(self):
        block_path = os.path.join(self.base_path, "blocks")
        if not os.path.exists(block_path):
            os.mkdir(block_path)
        parity_path = os.path.join(self.base_path, "parity")
        if not os.path.exists(parity_path):
            os.mkdir(parity_path)
        scode = self.split()
        if scode == 0:
            threads = []
            if not os.path.exists(parity_path):
                os.mkdir(parity_path)
            for idx in range(self.block_num):
                td = threading.Thread(target=self.SubEncode, args=(idx,))
                threads.append(td)
                td.start()
            for thread in threads:
                thread.join()
        return scode

    def SubDecode(self, block_path, idx, helper):
        try:
            if idx < self.block_num:
                target_path = TransferString(os.path.join(block_path, "block_%d.dat" % idx))
                if helper == (idx + 1) % self.block_num:
                    helper_path = TransferString(os.path.join(
                        self.base_path, "parity", "parity_%d_%d.dat" % (idx, (idx + 1) % self.block_num)
                    ))
                    survival_path = TransferString(os.path.join(
                        block_path, "block_%d.dat" % ((idx + 1) % self.block_num)
                    ))
                    #print("FIXING FEC: ", idx, helper)
                    result = self.dll.rs_fec(target_path, helper_path, survival_path, 1, self.conf["RS-settings"]["k"])
                else:
                    helper_path = TransferString(os.path.join(
                        self.base_path, "parity", "parity_%d_%d.dat" % ((idx - 1) % self.block_num, idx)
                    ))
                    survival_path = TransferString(os.path.join(
                        block_path, "block_%d.dat" % ((idx - 1) % self.block_num)
                    ))
                    #print("FIXING BEC: ", idx, helper)
                    result = self.dll.rs_fec(target_path, helper_path, survival_path, 0, self.conf["RS-settings"]["k"])
            else:
                target_path = TransferString(os.path.join(
                    self.base_path, "parity", "parity_%d_%d.dat" % (idx, (idx + 1) % self.block_num)
                ))
                helper_path = TransferString(os.path.join(
                    block_path, "block_%d.dat" % (idx % self.block_num)
                ))
                survival_path = TransferString(os.path.join(
                    block_path, "block_%d.dat" % ((idx + 1) % self.block_num)
                ))
                result = self.dll.rs_fec(target_path, helper_path, survival_path, 2, self.conf["RS-settings"]["k"])


            return result
        except Exception as e:
            print("[ERROR]: Decoding Error AT block-%d." % idx, str(e))
            return -1

    def Decode(self, loss_blocks_idx):
        try:
            self.block_num = self.conf["n"]
            blocks_path = os.path.join(self.base_path, "blocks")

            if len(loss_blocks_idx) > self.block_num:
                return -1

            survival = []
            for i in range(self.block_num * 2):
                if i not in loss_blocks_idx:
                    survival.append(i)

            threads = []
            still = set()

            while True:
                recovered = []
                for i in loss_blocks_idx:
                    if i < self.block_num:
                        if ((i + 1) % self.block_num in survival) and ((i + self.block_num) in survival):
                            td = threading.Thread(target=self.SubDecode, kwargs={
                                "block_path": blocks_path, "idx": i,
                                "helper": (i + 1) % self.block_num}
                                                  )
                            threads.append(td)
                            td.start()
                            recovered.append(i)
                            loss_blocks_idx.remove(i)
                            if i in still:
                                still.remove(i)
                        elif ((i - 1) % self.block_num in survival) and (((i - 1) % self.block_num + self.block_num) in survival):
                            td = threading.Thread(target=self.SubDecode, kwargs={
                                "block_path": blocks_path, "idx": i,
                                "helper": (i - 1) % self.block_num}
                                                  )
                            threads.append(td)
                            td.start()
                            recovered.append(i)
                            if i in still:
                                still.remove(i)
                            loss_blocks_idx.remove(i)
                        else:
                            still.add(i)
                    else:
                        if ((i % self.block_num) in survival) and (((i + 1) % self.block_num) in survival):
                            td = threading.Thread(target=self.SubDecode, kwargs={
                                "block_path": blocks_path, "idx": i,
                                "helper": ((i % self.block_num), ((i + 1) % self.block_num))}
                                                  )
                            threads.append(td)
                            td.start()
                            recovered.append(i)
                            if i in still:
                                still.remove(i)
                            loss_blocks_idx.remove(i)
                        else:
                            still.add(i)

                for thread in threads:
                    thread.join()

                for i in recovered:
                    survival.append(i)

                if len(still) == 0:
                    break

            return 0

        except Exception as e:
            print("[ERROR] Decode Error. ", str(e))
            return -1


class ZJC_Xor(EC):
    def __init__(self, params):
        super().__init__()
        self.name = 'ZJC_XorBased'
        self.params = params
        self.filesize = GetFileSize(self.params["path"])
        self.conf = self.conf["ZJC-Xor"]
        self.dllpath = os.path.join(os.getcwd(), "include", "ZJC_Xor")
        if self.os_type == "Windows":
            self.dllpath = os.path.join(self.dllpath, "zjc_xor.dll")
        else:
            self.dllpath = os.path.join(self.dllpath, "zjc_xor.so")
        #self.dll = cdll.LoadLibrary(self.dllpath)
        self.dll = CDLL(self.dllpath)
        self.base_path = os.path.dirname(self.params["path"])

    def split(self):
        fsize = GetFileSize(self.params["path"])
        if fsize == -1:
            print("[ERROR]: The File %s Operate Failed." % self.params["path"])
            return -1
        try:
            self.block_num = self.conf["n"]
            self.block_size = math.ceil(fsize / self.block_num)
            with open(self.params["path"], "rb") as f:
                for i in range(self.block_num):
                    with open(os.path.join(self.base_path, "blocks", "block_%d.dat" % i), "wb") as b:
                        b.write(f.read(self.block_size))
            return 0
        except Exception as e:
            print("[ERROR]: Splitting File %s Failed. %s" % (self.params["path"], e))
            return -1

    def SubEncode(self, idx):
        lp = TransferString(os.path.join(self.base_path, "blocks", "block_%d.dat" % idx))
        rp = TransferString(os.path.join(
            self.base_path, "blocks", "block_%d.dat" % ((idx + 1) % self.block_num)
        ))
        pp = TransferString(os.path.join(
            self.base_path, "parity", "parity_%d_%d.dat" %
                                      (idx, (idx + 1) % self.block_num)
        ))
        result = self.dll.encode_file(lp, rp, pp)
        if result != 0:
            print("[ERROR]: Encoding Error AT block-%d." % idx)

    def Encode(self):
        block_path = os.path.join(self.base_path, "blocks")
        if not os.path.exists(block_path):
            os.mkdir(block_path)
        parity_path = os.path.join(self.base_path, "parity")
        if not os.path.exists(parity_path):
            os.mkdir(parity_path)
        scode = self.split()
        if scode == 0:
            threads = []
            if not os.path.exists(parity_path):
                os.mkdir(parity_path)
            for idx in range(self.block_num):
                td = threading.Thread(target=self.SubEncode, args=(idx,))
                threads.append(td)
                td.start()
            for thread in threads:
                thread.join()
        return scode

    def SubDecode(self, block_path, idx, helper, loss_size):
        try:
            if idx < self.block_num:
                target_path = TransferString(os.path.join(block_path, "block_%d.dat" % idx))
                if helper == (idx + 1) % self.block_num:
                    helper_path = TransferString(os.path.join(
                        self.base_path, "parity", "parity_%d_%d.dat" % (idx, (idx + 1) % self.block_num)
                    ))
                    survival_path = TransferString(os.path.join(
                        block_path, "block_%d.dat" % ((idx + 1) % self.block_num)
                    ))

                else:
                    helper_path = TransferString(os.path.join(
                        self.base_path, "parity", "parity_%d_%d.dat" % ((idx - 1) % self.block_num, idx)
                    ))
                    survival_path = TransferString(os.path.join(
                        block_path, "block_%d.dat" % ((idx - 1) % self.block_num)
                    ))
            else:
                target_path = TransferString(os.path.join(
                    self.base_path, "parity", "parity_%d_%d.dat" % ((idx - 1) % self.block_num, idx)
                ))
                helper_path = TransferString(os.path.join(
                    block_path, "block_%d.dat" % (idx % self.block_num)
                ))
                survival_path = TransferString(os.path.join(
                    block_path, "block_%d.dat" % ((idx + 1) % self.block_num)
                ))
            result = self.dll.FEC(target_path, helper_path, survival_path, loss_size)
            return result
        except Exception as e:
            print("[ERROR]: Decoding Error AT block-%d." % idx, str(e))
            return -1

    def Decode(self, loss_blocks_idx, loss_size):
        try:
            self.block_num = self.conf["n"]
            blocks_path = os.path.join(self.base_path, "blocks")

            if len(loss_blocks_idx) > self.block_num:
                return -1

            survival = []
            for i in range(self.block_num * 2):
                if i not in loss_blocks_idx:
                    survival.append(i)

            threads = []
            still = set()

            while True:
                recovered = []
                for i in loss_blocks_idx:
                    if i < self.block_num:
                        if ((i + 1) % self.block_num in survival) and ((i + self.block_num) in survival):
                            td = threading.Thread(target=self.SubDecode, kwargs={
                                "block_path": blocks_path, "idx": i, "loss_size": loss_size[0],
                                "helper": (i + 1) % self.block_num}
                                                  )
                            threads.append(td)
                            td.start()
                            recovered.append(i)
                            loss_blocks_idx.remove(i)
                            if i in still:
                                still.remove(i)
                        elif ((i - 1) % self.block_num in survival) and (
                                ((i - 1) % self.block_num + self.block_num) in survival):
                            td = threading.Thread(target=self.SubDecode, kwargs={
                                "block_path": blocks_path, "idx": i, "loss_size": loss_size[0],
                                "helper": (i - 1) % self.block_num}
                                                  )
                            threads.append(td)
                            td.start()
                            recovered.append(i)
                            if i in still:
                                still.remove(i)
                            loss_blocks_idx.remove(i)
                        else:
                            still.add(i)
                    else:
                        if ((i % self.block_num) in survival) and (((i + 1) % self.block_num) in survival):
                            td = threading.Thread(target=self.SubDecode, kwargs={
                                "block_path": blocks_path, "idx": i, "loss_size": loss_size[0],
                                "helper": ((i % self.block_num), ((i + 1) % self.block_num))}
                                                  )
                            threads.append(td)
                            td.start()
                            recovered.append(i)
                            if i in still:
                                still.remove(i)
                            loss_blocks_idx.remove(i)
                        else:
                            still.add(i)

                for thread in threads:
                    thread.join()

                for i in recovered:
                    survival.append(i)

                if len(still) == 0:
                    break

            return 0

        except Exception as e:
            print("[ERROR] Decode Error. ", str(e))
            return -1


"""
This LRC code code only workable in our test configurations!
"""


class LRC_EC(EC):
    def __init__(self, params):
        super().__init__()
        self.name = 'LRC'
        self.params = params
        self.filesize = GetFileSize(self.params["path"])
        self.block_num = int(self.conf["LRC"]["k"])
        self.k = self.block_num
        self.l = self.conf["LRC"]["l"]
        self.r = self.conf["LRC"]["r"]
        self.group_len = int(self.block_num // self.l)
        self.base_path = os.path.dirname(self.params["path"])
        self.dllpath = os.path.join(os.getcwd(), "include", "LRC")
        if self.os_type == "Windows":
            self.dllpath = os.path.join(self.dllpath, "lrc_ec.dll")
        else:
            self.dllpath = os.path.join(self.dllpath, "lrc_ec.so")
        #self.dll = cdll.LoadLibrary(self.dllpath)
        self.dll = CDLL(self.dllpath, mode=RTLD_GLOBAL)
        self.__initdll()

    def __initdll(self):
        self.dll.encode_file.argtypes = (
            POINTER(c_char_p),
            c_int,
            c_int,
            c_int
        )
        self.dll.encode_file.restype = c_int

        self.dll.decode_file.argtypes = (
            POINTER(c_char_p),
            c_int,
            c_int,
            c_int,
            POINTER(c_int),
            c_int
        )
        self.dll.decode_file.restype = c_int

    def split(self):
        fsize = GetFileSize(self.params["path"])
        if fsize == -1:
            print("[ERROR]: The File %s Operate Failed." % self.params["path"])
            return -1
        try:
            self.block_size = math.ceil(fsize / self.k)
            with open(self.params["path"], "rb") as f:
                for i in range(self.k):
                    with open(os.path.join(self.base_path, "blocks", "block_%d.dat" % i), "wb") as b:
                        b.write(f.read(self.block_size))
            return 0
        except Exception as e:
            print("[ERROR]: Splitting File %s Failed. %s" % (self.params["path"], e))
            return -1

    def __GetBlocks(self, block_path, parity_path):
        data_blocks = ["%s" %
                       (os.path.join(block_path, "block_%d.dat" % i))
                       for i in range(self.k)
                       ]
        parity_blocks = ["%s" %
                         (os.path.join(parity_path, "parity_%d.dat" % i))
                         for i in range(self.l + self.r)
                         ]

        return self._TransferBtye(data_blocks + parity_blocks)

    def _TransferBtye(self, source):
        res = []
        for src in source:
            res.append(src.encode("utf-8"))
        return res

    def SubEncode(self, block_path, parity_path):
        filelist = self.__GetBlocks(block_path, parity_path)
        filelist = ((c_char_p) * len(filelist))(*filelist)
        return self.dll.encode_file(filelist, self.k, self.l, self.r)

    def Encode(self, ):
        block_path = os.path.join(self.base_path, "blocks")
        if not os.path.exists(block_path):
            os.mkdir(block_path)
        parity_path = os.path.join(self.base_path, "parity")
        if not os.path.exists(parity_path):
            os.mkdir(parity_path)
        scode = self.split()
        if scode == 0:
            return self.SubEncode(block_path, parity_path)
        return scode

    def Decode(self, loss_idx):
        block_path = os.path.join(self.base_path, "blocks")
        parity_path = os.path.join(self.base_path, "parity")
        loss_num = len(loss_idx)
        filelist = self.__GetBlocks(block_path, parity_path)
        filelist = ((c_char_p) * len(filelist))(*filelist)
        loss_idxs = ((c_int) * loss_num)(*loss_idx)
        code = self.dll.decode_file(filelist, self.k, self.l, self.r, loss_idxs, loss_num)
        if code == 0:
            self.TruncateBlock(block_path, parity_path, loss_idxs)
        return code

    def TruncateBlock(self, bp, pp, loss):
        blocks = [os.path.join(bp, "block_%d.dat" % i) for i in range(self.k)]
        parity = [os.path.join(pp, "parity_%d.dat" % i) for i in range(self.l + self.r)]
        paths = blocks + parity

        for i in range(self.k + self.l + self.r):
            if i not in loss:
                size = GetFileSize(paths[i])
                break

        for i in range(self.k + self.l + self.r):
            if i in loss:
                with open(paths[i], "rb+") as f:
                    f.truncate(size)


if __name__ == '__main__':
    src_file = os.path.join(os.getcwd(), "temp", "test", "test.dat")
    """
    ecp = EC_Policy(Replication,
                    {"path": src_file})
    if ecp.Encode() == 0:
        print("[SUCCESS]: EC Policy Encoded Successfully.")
    else:
        print("[FAIL]: EC Policy Encoded Failed.")
    if ecp.Decode() == 0:
        if filehash(src_file) == filehash(os.path.join(os.path.dirname(src_file), "decode.dat")):
            print("[OK]: Decoding Successful.")
        else:
            print("[ERROR]: Hash Error.")
    else:
        print("[ERROR]: Decoding Failed.")
    """

    """
    ecp = EC_Policy(RS, {"path": src_file})
    if ecp.Encode() == 0:
        print("[SUCCESS]: EC Policy Encoded Successfully.")
    else:
        print("[FAIL]: EC Policy Encoded Failed.")
    # Only Data Blocks loss, parity blocks use Encode to recover.
    # idx begin at 0

    idx = [i for i in range(4)]
    if ecp.Decode(idx) == 0:
        ecp.emerge()
        if filehash(src_file) == filehash(os.path.join(os.path.dirname(src_file), "decode.dat")):
            print("[SUCCESS]: Decoding Successful.")
        else:
            print("[ERROR]: Hash Error.")
    else:
        print("[FAIL]: Decoding Failed.")
    """

    """
    ecp = EC_Policy(ZJC_RSBased, {"path": src_file})
    if ecp.Encode() == 0:
        print("[SUCCESS]: EC Policy Encoded Successfully.")
    else:
        print("[FAIL]: EC Policy Encoded Failed.")

    idx = [0, 1, 3, 5]
    if ecp.Decode(idx) == 0:
        ecp.emerge()
        if filehash(src_file) == filehash(os.path.join(os.path.dirname(src_file), "decode.dat")):
            print("[SUCCESS]: FEC Decoding Successful.")
        else:
            print("[FAIL]: FEC Hash Error.")
    else:
        print("[FAIL]: FEC Decoding Failed.")
    """

    """
    xt1 = time.time()
    ecp = EC_Policy(ZJC_XorBased, {"path": src_file})
    if ecp.Encode() == 0:
        print(time.time() - xt1)
        print("[SUCCESS]: EC Policy Encoded Successfully.")
    else:
        print("[FAIL]: EC Policy Encoded Failed.")
    idx = [0, 1, 3, 5]
    size = os.path.getsize(os.path.join(os.getcwd(), "temp", "test", "blocks", "block_0.dat"))
    size = [size] * 8
    xt2 = time.time()
    if ecp.Decode(idx, size) == 0:
        ecp.emerge()
        print(time.time() - xt2)
        if filehash(src_file) == filehash(os.path.join(os.path.dirname(src_file), "decode.dat")):
            print("[SUCCESS]: FEC Decoding Successful.")
        else:
            print("[FAIL]: FEC Hash Error.")
    else:
        print("[FAIL]: FEC Decoding Failed.")
    """

    #"""
    ecp = EC_Policy(LRC, {"path": src_file})
    t1 = time.time()
    code = ecp.Encode()
    if code == 0:
        print(time.time() - t1)
        print("[SUCCESS]: EC Policy Encoded Successfully.")
    else:
        print(code)
        print("[FAIL]: EC Policy Encoded Failed.")
    #loss = [0, 2, 3, 5, 10, 11]
    #loss = [0, 2, 4, 9, 10, 11]
    #loss = [0, 2, 4, 6, 8, 10]
    loss = [0, 2, 4, 6, 8, 10, 12, 14]
    t2 = time.time()
    if ecp.Decode(loss) == 0:
        ecp.emerge()
        ecp.Truncate()
        print(time.time() - t2)
        if filehash(src_file) == filehash(os.path.join(os.path.dirname(src_file), "decode.dat")):
            print("[SUCCESS]: LRC Decoding Successful.")
        else:
            print("[FAIL]: LRC Hash Error.")
    else:
        print("[FAIL]: LRC Decoding Failed.")
    """
    #"""
