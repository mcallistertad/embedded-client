#!/usr/bin/env python
import elg_pb2
from Crypto.Cipher import AES
from Crypto.Random import get_random_bytes

RQ_HEADER_LEN = 10
CRYPTO_INFO_LEN = 20

def encode_rs(crypto_key, lat, lon, hpe):
    # Create and serialize the response body.
    rs = elg_pb2.Rs()

    rs.lat = lat
    rs.lon = lon
    rs.hpe = hpe

    rs_buf = rs.SerializeToString()

    # Create and serialize the crypto info.
    aes_padding_length = (16 - len(rs_buf) % 16) % 16

    crypto_info = elg_pb2.CryptoInfo()

    crypto_info.iv = get_random_bytes(16)
    crypto_info.aes_padding_length_plus_one = aes_padding_length + 1

    crypto_info_buf = crypto_info.SerializeToString()

    # Create and serialize the header.
    rs_header = elg_pb2.RsHeader()

    rs_header.remaining_length = len(crypto_info_buf) + len(rs_buf) + aes_padding_length

    rs_header_buf = rs_header.SerializeToString()

    # Encrypt the body.
    cipher = AES.new(crypto_key, AES.MODE_CBC, crypto_info.iv)
    rs_buf = cipher.encrypt(rs_buf + get_random_bytes(aes_padding_length))

    return rs_header_buf + crypto_info_buf + rs_buf


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
