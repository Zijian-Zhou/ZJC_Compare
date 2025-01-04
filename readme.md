ZJ Codes (ZJC): Comparison Codes GuideLines
---

# Introduction
&emsp;&emsp;This repository is the comparison codes for ZJC  with RS and LRC over ``env1``. We recommend to execute our codes on the Ubuntu.


# Preminary
&emsp;&emsp;A preminary project is needed to install before you execute our codes:

&emsp;&emsp;&emsp;&emsp;[lrc-erasure-code](https://github.com/drmingdrmer/lrc-erasure-code)


We prefer you set environment as follows:
```shell
PYTHONUNBUFFERED=1;LD_LIBRARY_PATH=/usr/local/lib
```

where the ``/usr/local/lib`` path is the installation path of [lrc-erasure-code](https://github.com/drmingdrmer/lrc-erasure-code).

# Dirctories Statement

&emsp;&emsp;The major folders of this repository are as follows:

```shell
/---
    /conf ---
        conf.json           # configuration file

    /include
        /LRC                # C source files of LRC
            ef_lrc.c        # major LRC source codes
        /RS                 # C source files of RS
            reed-solomon.c  # major RS source codes
        /ZJC_RS             # C source files of ZJC RS based
            reed-solomon.c  # major ZJC-RS source codes
        /ZJC_Xor            # C source files of ZJC Xor based
            Xor_base_ec.c   # major ZJC-Xor source codes

    /temp                   # folder for temp runing files

    /testfiles              # original data for tests

    basefunctions.py        # some basic functions in our codes

    Loadconf.py             # to load the EC settings

    Crypto.py               # hash operation lib using pyhon hashlib

    ECPolicies.py           # the encode and decode implement of different codes
                            # detailed instances are after the ``if __name__ == "__main__":``

    Compare.py              # the script of experiments, RUN THIS FILE!

```

# Personlized Parameters Configuration
&emsp;&emsp;If you want to change the fixed EC settings, there are two things you need to do.

- First, re-edit the [conf.json](/conf/conf.json). Now, we fixed it as same redundancy with ``ZJC(8)``. You can change the parameters of ZJC. We use ``n`` here to present the amount of data blocks but ``k`` in the paper. After you determine the ``n`` of ZJC, you can calculate the parameters of RS and LRC in the same redundancy i.e. if ``n`` of ZJC is ``a``, we recommend it is a even numver, ``n=2*a`` and ``k=a`` for RS, ``k=a``, ``l=r=k/2`` for LRC. You also can change the RS settings for ZJC-RS, but you should guarantee ``n:k=3:2``. 
```json
{
  "EC_Policy": {
    "replication": {
      "replication-factor": 2,
      "block-size": 8388608
    },
    "RS": {
      "n": 16,
      "k": 8
    },
    "ZJC-RS": {
      "n": 8,
      "RS-settings": {
        "n": 24,
        "k": 16
      }
    },
    "ZJC-Xor": {
      "n": 8
    },
    "LRC": {
      "k": 8,
      "l": 4,
      "r": 4
    }
  }
}
```

- Second, edit the ``start()`` function in ``Compare.py``, Line 88 to 93. You should add decodable cases for you settings, if ``k`` not in ``[4,6,8]``. The item of ``loss_list`` presents the index of lost block.
```python
if max_loss == 4:
    loss_list = [0, 1, 3, 5]
elif max_loss == 6:
    loss_list = [0, 2, 4, 6, 8, 10]
else:
    loss_list = [0, 2, 4, 6, 8, 10, 12, 14]
```

# Run

&emsp;&emsp;After everythins is OK, you just run the ``Compare.py``, it will automatically generates a csv file ``output.csv`` which records the data. Each line of the csv file are as follows:
```shell
| Encode Begin Time | Encode End Time | Decode Begin Time | Encode Time Cost | Decode Time Cost | EC Policy | Amount of Lost blocks | Data Size (MB) |
```

For the EC policies codes:
```python
Replication = 0
RS = 1
ZJC_RSBased = 2
ZJC_XorBased = 3
LRC = 4
```


# Issues You May Occur

## C libs do not work

&emsp;&emsp;It might occurs encode or decode failed, because the bits of python and gcc are differents.

### RS

For RS codes, you need re-build the source codes of RS [reed-solomon.c](/include/RS/reed-solomon.c), if the cpython can not work.

```shell
gcc -fPIC -shared reed-solomon.c -o rs_ec.so
```

### LRC

For RS codes, you need re-build the source codes of LRC [ef_lrc.c](/include/LRC/ef_lrc.c), if the cpython can not work.

```shell
gcc -fPIC -shared ef_lrc.c -o lrc_ec.so
```

### ZJC-RS

For RS codes, you need re-build the source codes of ZJC-RS under the folder [reed-solomon.c](/include/ZJC_RS/reed-solomon.c), if the cpython can not work.

```shell
gcc -fPIC -shared reed-solomon.c -o zjc_rs.so
```

### ZJC-Xor

For RS codes, you need re-build the source codes of ZJC-Xor under the folder [Xor_base_ec.c](/include/ZJC_Xor/Xor_base_ec.c), if the cpython can not work.

```shell
gcc -fPIC -shared Xor_base_ec.c -o zjc_xor.so
```
