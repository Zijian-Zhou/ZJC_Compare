import hashlib


def texthash(text):
    try:
        sha = hashlib.sha256()
        sha.update(text.encode('utf-8'))
        return sha.hexdigest()
    except Exception as e:
        print("[Wronging] Calculation SHA-256 For TEXT %s Failed." % (text))
        print(str(e))
        return ""


def filehash(filepath):
    try:
        sha = hashlib.sha256()
        with open(filepath, 'rb') as f:
            for byte_block in iter(lambda: f.read(4096), b""):
                sha.update(byte_block)
        return sha.hexdigest()
    except Exception as e:
        print("[Wronging] Calculation SHA-256 For FILE %s Failed." % (filepath))
        print(str(e))
        return ""
