import os.path
import shutil
import time

from Crypto import filehash
from ECPolicies import EC_Policy
from Loadconf import Conf

def RS_Decode(ecp, idx, src_file):
    if ecp.Decode(idx) == 0:
        ecp.emerge()
        ecp.Truncate()
        if filehash(src_file) == filehash(os.path.join(os.path.dirname(src_file), "decode.dat")):
            return 0
        else:
            return -2
    else:
        return -1

def ZJC_RS_Decode(ecp, idx, src_file):
    if ecp.Decode(idx) == 0:
        ecp.emerge()
        ecp.Truncate()
        if filehash(src_file) == filehash(os.path.join(os.path.dirname(src_file), "decode.dat")):
            return 0
        else:
            return -2
    else:
        return -1

def ZJC_Xor_Decode(ecp, idx, size, src_file):
    if ecp.Decode(idx, size) == 0:
        ecp.emerge()
        ecp.Truncate()
        if filehash(src_file) == filehash(os.path.join(os.path.dirname(src_file), "decode.dat")):
            return 0
        else:
            return -2
    else:
        return -1


def LRC_Decode(ecp, idx, src_file):
    if ecp.Decode(idx) == 0:
        ecp.emerge()
        ecp.Truncate()
        if filehash(src_file) == filehash(os.path.join(os.path.dirname(src_file), "decode.dat")):
            return 0
        else:
            return -2
    else:
        return -1

def  add_recode(t1, t2, t3, t4, encode_code, Decode_code, ec, loss, size):
    ec += 1
    print(t1, t2, t3, t4, encode_code, Decode_code, ec, loss, size)
    with (open("output.csv", "a+") as f):
        line = "%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s\n" % (t1, t2, t2 - t1, t3, t4, t4 - t3, encode_code, Decode_code, ec, loss, size)
        f.write(line)

def start():
    #sizes = [24, 48, 72, 96, 200, 500, 1024]
    sizes = [24, 48, 72, 96]
    sizes = [200]
    conf = Conf().Get_ECconf()
    for ec in range(4):
        for size in sizes:

            if os.path.exists(os.path.join(os.getcwd(), "testfiles", "parity")):
                shutil.rmtree(os.path.join(os.getcwd(), "testfiles", "blocks"))

            if os.path.exists(os.path.join(os.getcwd(), "testfiles", "parity")):
                shutil.rmtree(os.path.join(os.getcwd(), "testfiles", "parity"))

            base_path = os.path.join(os.getcwd(), "testfiles",
                                     "%sMB-%s.dat" % (size, ec)
                                     )

            ecp = EC_Policy(ec + 1, {"path": base_path})

            t1 = time.time()

            encode_code = ecp.Encode()

            t2 = time.time()

            max_loss = conf["RS"]["k"]
            if max_loss == 4:
                loss_list = [0, 1, 3, 5]
            elif max_loss == 6:
                loss_list = [0, 2, 4, 6, 8, 10]
            else:
                loss_list = [0, 2, 4, 6, 8, 10, 12, 14]

            if ec == 0:
                max_loss = conf["RS"]["k"]
                for loss in range(1, max_loss + 1):
                    print(ec, size , loss)

                    t3 = time.time()

                    Decode_code = RS_Decode(ecp, loss_list[: loss], base_path)

                    t4 = time.time()

                    add_recode(t1, t2, t3, t4, encode_code, Decode_code, ec, loss, size)
            elif ec == 1:
                max_loss = conf["ZJC-RS"]["n"]
                for loss in range(1, max_loss + 1):
                    print(ec, size , loss)

                    t3 = time.time()

                    Decode_code = ZJC_RS_Decode(ecp, loss_list[: loss], base_path)

                    t4 = time.time()

                    add_recode(t1, t2, t3, t4, encode_code, Decode_code, ec, loss, size)
            elif ec == 2:
                max_loss = conf["ZJC-Xor"]["n"]
                for loss in range(1, max_loss + 1):
                    print(ec, size , loss)

                    size_list = os.path.getsize(os.path.join(os.path.dirname(base_path), "blocks", "block_0.dat"))
                    size_list = [size_list] * max_loss

                    t3 = time.time()

                    Decode_code = ZJC_Xor_Decode(ecp, loss_list[: loss], size_list, base_path)

                    t4 = time.time()

                    add_recode(t1, t2, t3, t4, encode_code, Decode_code, ec, loss, size)
            else:
                max_loss = conf["LRC"]["k"]
                for loss in range(1, max_loss + 1):
                    print(ec, size , loss)

                    t3 = time.time()

                    Decode_code = LRC_Decode(ecp, loss_list[:loss], base_path)

                    t4 = time.time()

                    add_recode(t1, t2, t3, t4, encode_code, Decode_code, ec, loss, size)


if __name__ == "__main__":
    start()