#!/usr/bin/env python
import elg_pb2
from Crypto.Cipher import AES

RQ_HEADER_LEN = 10
CRYPTO_INFO_LEN = 20

def encode_rq(rq):
    pass

def decode_rq(buf):
    msg_start = 0
    msg_end = RQ_HEADER_LEN

    # Deserialze the header
    rq_header = elg_pb2.RqHeader()
    len = rq_header.ParseFromString(buf[msg_start:msg_end])

    msg_start = len

    # Deserialize the CryptoInfo message.
    crypto_info = None

    msg_end = msg_start + CRYPTO_INFO_LEN

    crypto_info = elg_pb2.CryptoInfo()
    len = crypto_info.ParseFromString(buf[msg_start:msg_end])

    # Decrypt the body.
    msg_start += len

    key = b'\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f'

    cipher = AES.new(key, AES.MODE_CBC, crypto_info.iv)
    #cipher = AES.new(crypto_info.iv, AES.MODE_CBC, crypto_info.iv)

    plaintext = cipher.decrypt(buf[msg_start:])

    rq = elg_pb2.Rq()

    len = rq.ParseFromString(plaintext[:-crypto_info.aes_padding_length_plus_one + 1])

    return rq
