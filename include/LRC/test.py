from ctypes import *
import os

class LRC:
	def __init__(self, k, l, r):
		self.dllpath = os.path.join(os.getcwd(), "lrc.so")
		self.dll = cdll.LoadLibrary(self.dllpath)
		self.k = k
		self.l = l
		self.r = r
	
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
	
	def _TransferBtye(self, source):
		res = []
		for src in source:
			res.append(src.encode("utf-8"))
		return res
	
	def Encode(self, filelist):
		filelist = self._TransferBtye(filelist)
		filelist = ((c_char_p) * len(filelist))(*filelist)
		self.dll.encode_file(filelist, self.k, self.l, self.r)
		
	def Decode(self, filelist, loss_idx, loss_num):
		filelist = self._TransferBtye(filelist)
		filelist = ((c_char_p) * len(filelist))(*filelist)
		loss_idx = ((c_int) * loss_num)(*loss_idx)
		self.dll.decode_file(filelist, self.k, self.l, self.r, loss_idx, loss_num)


if __name__ == "__main__":
	
	file_list = [
		"blocks/block_0.dat",
        "blocks/block_1.dat",
        "blocks/block_2.dat",
        "blocks/block_3.dat",
        "parity/parity_0.dat",
        "parity/parity_1.dat",
        "parity/parity_2.dat",
        "parity/parity_3.dat",
	]
	ecp = LRC(4, 2, 2)
	ecp.Encode(file_list)
	loss = [0, 2, 6, 7]
	for i in range(1, 5):
		ecp.Decode(file_list, loss[:i], i)
	
	
