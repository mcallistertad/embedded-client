#!/usr/bin/env python
import sys
import elg_pb2

"""
Reads a file (the first command line argument) containing single ELG rq
message, and prints its contents.
"""

with open(sys.argv[1], "rb") as f:
    buffer = f.read()

    # First 28 bytes is the header.
    rq_header = elg_pb2.RqHeader()

    rq_header.ParseFromString(buffer[:28]);

    print("header:")
    print(rq_header)

    rq = elg_pb2.Rq()

    rq.ParseFromString(buffer[28:])

    print("body:")
    print(rq)
