#!/usr/bin/env python
import socketserver
import elg_proto

class MyTCPHandler(socketserver.BaseRequestHandler):
    def handle(self):
        buf = self.request.recv(elg_proto.RQ_HEADER_LEN)

        header = elg_proto.decode_rq_header(buf)

        print("---- header: ----")
        print(header)

        buf = self.request.recv(header.remaining_length)

        print("remaining bytes read: {}".format(len(buf)))

        # TODO: determine the key based on the value of header.partner_id.
        key = b'\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f'

        body = elg_proto.decode_rq_crypto_info_and_body(buf, key)

        print("---- body: ----")
        print(body)


if __name__ == "__main__":
    HOST, PORT = "localhost", 9755

    server = socketserver.TCPServer((HOST, PORT), MyTCPHandler)

    server.serve_forever()
