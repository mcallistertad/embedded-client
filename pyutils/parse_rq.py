#!/usr/bin/env python
import sys
import elg_proto

"""
Reads a file (the first command line argument) containing single ELG rq
message, and prints its contents.
"""

with open(sys.argv[1], "rb") as f:
    buf = f.read()

    header = elg_proto.decode_rq_header(buf[: elg_proto.RQ_HEADER_LEN])

    print("---- header: ----")
    print(header)

    # TODO: determine the key based on the value of header.partner_id.
    key = b'\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f'

    body = elg_proto.decode_rq_crypto_info_and_body(buf[elg_proto.RQ_HEADER_LEN:], key)

    print("---- body: ----")
    print(body)
