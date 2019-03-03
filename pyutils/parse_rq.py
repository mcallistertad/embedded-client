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

def getKey(partner_id):
    return b'\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f'

with open(sys.argv[1], "rb") as f:
    buf = f.read()

    header, crypto_info, body = elg_proto.decode_rq(buf, getKey)

    print(header)
    print(crypto_info)
    print(body)
