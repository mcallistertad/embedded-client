#!/usr/bin/env python
import sys
import elg_pb2

"""
Reads a file (the first command line argument) containing single ELG rq
message, and prints its contents.
"""

rq = elg_pb2.Rq()

with open(sys.argv[1], "rb") as f:
    rq.ParseFromString(f.read())

print(rq)
