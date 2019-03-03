#!/usr/bin/env python
import elg_pb2
from Crypto.Cipher import AES

RQ_HEADER_LEN = 10
CRYPTO_INFO_LEN = 20

def encode_rq(rq):
    pass

def decode_rq(buf, key_lookup_func):
    msg_start = 0
    msg_end = RQ_HEADER_LEN

    # Deserialze the header
    header = elg_pb2.RqHeader()
    len = header.ParseFromString(buf[msg_start:msg_end])

    msg_start = len

    # Deserialize the CryptoInfo message.
    crypto_info = None

    msg_end = msg_start + CRYPTO_INFO_LEN

    crypto_info = elg_pb2.CryptoInfo()
    len = crypto_info.ParseFromString(buf[msg_start:msg_end])

    # Decrypt the body.
    msg_start += len

    key = key_lookup_func(header.partner_id)

    try:
        cipher = AES.new(key, AES.MODE_CBC, crypto_info.iv)

        plaintext = cipher.decrypt(buf[msg_start:])
    except:
        print("error decrypting buffer")

    try:
        body = elg_pb2.Rq()

        len = body.ParseFromString(plaintext[:-crypto_info.aes_padding_length_plus_one + 1])
    except:
        print("error decoding buffer")

    return header, crypto_info, body
