#!/usr/bin/env python
import sys
import elg_pb2
from Crypto.Cipher import AES

"""
Reads a file (the first command line argument) containing single ELG rq
message, and prints its contents.
"""
RQ_HEADER_LEN = 10
CRYPTO_INFO_LEN = 20

with open(sys.argv[1], "rb") as f:
    buffer = f.read()
    msg_start = 0
    msg_end = RQ_HEADER_LEN

    # Deserialze the header
    rq_header = elg_pb2.RqHeader()
    len = rq_header.ParseFromString(buffer[msg_start:msg_end])

    print("header (len={}):".format(len))
    print(rq_header)

    msg_start = len

    # Deserialize the CryptoInfo message.
    crypto_info = None

    msg_end = msg_start + CRYPTO_INFO_LEN

    crypto_info = elg_pb2.CryptoInfo()
    len = crypto_info.ParseFromString(buffer[msg_start:msg_end])

    print("crypto_info (len={}):".format(len))
    print(crypto_info)

    # Decrypt the body.
    msg_start += len

    key = b'\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f'

    cipher = AES.new(key, AES.MODE_CBC, crypto_info.iv)
    #cipher = AES.new(crypto_info.iv, AES.MODE_CBC, crypto_info.iv)

    plaintext = cipher.decrypt(buffer[msg_start:])

    rq = elg_pb2.Rq()

    len = rq.ParseFromString(plaintext[:-crypto_info.aes_padding_length_plus_one + 1])

    print("body (len={}):".format(len))
    print(rq)
