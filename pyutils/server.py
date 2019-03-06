#!/usr/bin/env python
import socketserver
import logging
import elg_proto

logging.basicConfig(format='%(asctime)s: %(message)s', filename='server.log',level=logging.DEBUG)

class MyTCPHandler(socketserver.BaseRequestHandler):
    def handle(self):
        buf = bytearray()

        while len(buf) < elg_proto.RQ_HEADER_LEN:
            buf.extend(self.request.recv(elg_proto.RQ_HEADER_LEN))

        header = elg_proto.decode_rq_header(buf)

        logging.info("---- header: ----\n" + str(header))

        buf = bytearray()

        while len(buf) < header.remaining_length:
            buf.extend(self.request.recv(header.remaining_length))

        logging.info("remaining bytes read: {}".format(len(buf)))

        # TODO: determine the key based on the value of header.partner_id.
        key = b'\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f'

        body = elg_proto.decode_rq_crypto_info_and_body(buf, key)

        logging.info("---- body: ----\n" + str(body))


if __name__ == "__main__":
    HOST, PORT = "localhost", 9755

    server = socketserver.TCPServer((HOST, PORT), MyTCPHandler)
    server.allow_reuse_address = True

    server.serve_forever()
