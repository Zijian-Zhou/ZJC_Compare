import shutil

from Loadconf import Conf
import platform
import os
from ctypes import *


def GetTargetINF(datanode_name):
    conf = Conf().Get_DNList()
    for item in conf.items():
        if item[0] == datanode_name:
            return item[1]


def CopyFile(src, dest):
    try:
        shutil.copyfile(src, dest)
        return (True, None)
    except Exception as e:
        return (False, str(e))


def DeleteFile(path):
    try:
        os.remove(path)
        return (True, None)
    except Exception as e:
        return (False, str(e))


def DeleteDir(path):
    try:
        shutil.rmtree(path)
        return (True, None)
    except Exception as e:
        return (False, str(e))


def TransferString(src):
    return create_string_buffer(src.encode('utf-8'))


def GetOS():
    os_type = platform.system()
    if os_type == 'Darwin':
        print("Using an unsupported OS.")
    return os_type


def GetFileSize(filepath):
    try:
        size = os.path.getsize(filepath)
        return size
    except OSError as e:
        print("[Error] File Operate Failed At %s. " % filepath, str(e))
        return -1


def Files_list(directory):
    try:
        files = [f for f in os.listdir(directory) if os.path.isfile(os.path.join(directory, f))]
        return files
    except FileNotFoundError:
        print(f"Directory '{directory}' not found")
        return []
    except Exception as e:
        print(f"Error: {e}")
        return []


if __name__ == '__main__':
    #model = NameNodeModel()
    #ans = SqlDNlist2Dict(model.Get("SELECT * FROM datanode_list"))
    #
    print(GetTargetINF("datanode4"))
