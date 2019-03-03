#!/usr/bin/env python
import sys
import elg_proto
from Crypto.Cipher import AES

"""
Reads a file (the first command line argument) containing single ELG rq
message, and prints its contents.
"""
RQ_HEADER_LEN = 10
CRYPTO_INFO_LEN = 20

with open(sys.argv[1], "rb") as f:
    buf = f.read()
    rq = elg_proto.decode_rq(buf)

    print(rq)
