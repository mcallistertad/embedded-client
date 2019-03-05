#!/usr/bin/env python
import elg_pb2
from Crypto.Cipher import AES

RQ_HEADER_LEN = 10
CRYPTO_INFO_LEN = 20

def encode_rq(rq, crypto_key):
    pass

def decode_rq_header(buf):
    assert len(buf) == RQ_HEADER_LEN, "Invalid buffer length"

    header = elg_pb2.RqHeader()
    length = header.ParseFromString(buf)

    assert length == RQ_HEADER_LEN, "Unexpected parse result length"

    return header

def decode_rq_crypto_info_and_body(buf, crypto_key):
    assert len(buf) > CRYPTO_INFO_LEN, "Buffer too small"

    # Deserialize the CryptoInfo
    crypto_info = elg_pb2.CryptoInfo()
    length = crypto_info.ParseFromString(buf[:CRYPTO_INFO_LEN])

    assert length == CRYPTO_INFO_LEN, "Unexpected parse result length"

    # Decrypt the body.
    cipher = AES.new(crypto_key, AES.MODE_CBC, crypto_info.iv)
    plaintext = cipher.decrypt(buf[length:])

    # Deserialize the decrypted body.
    body = elg_pb2.Rq()
    length = body.ParseFromString(plaintext[:len(plaintext) - crypto_info.aes_padding_length_plus_one + 1])

    return body
