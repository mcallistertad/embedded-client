#!/usr/bin/env python
import socketserver
import threading
import logging.config
import elg_proto
import yaml

PORT=9756
SOCKET_TIMEOUT=5

def configure():
    with open("server.yaml") as f:
        config = yaml.load(f)

    logging.config.dictConfig(config['log_config'])


class RequestHandler(socketserver.BaseRequestHandler):
    def handle(self):
        logger = logging.getLogger('handler')

        logger.info("Handling request. Active thread count = {}".format(threading.active_count()))

        self.request.settimeout(SOCKET_TIMEOUT)

        try:
            # Read the header in order to get the partner_id and to determine
            # how long the message is.
            buf = bytearray()

            while len(buf) < elg_proto.RQ_HEADER_LEN:
                buf.extend(self.request.recv(elg_proto.RQ_HEADER_LEN - len(buf)))

            header = elg_proto.decode_rq_header(buf)

            logger.info("---- header: ----\n" + str(header))

            # Read the rest of the message.
            buf = bytearray()

            while len(buf) < header.remaining_length:
                buf.extend(self.request.recv(header.remaining_length - len(buf)))

            logger.info("remaining bytes read: {}".format(len(buf)))

            # TODO: determine the key based on the value of header.partner_id.
            key = b'\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f'

            # Read and decode the remainder of the message.
            body = elg_proto.decode_rq_crypto_info_and_body(buf, key)

            logger.info("---- body: ----\n" + str(body))

            # TODO: Create the corresponding API server request, send it
            # thither, get the API server response, create and send the client
            # response.

            logger.info("Request complete")
        except Exception as e:
            logger.error("exception: " + str(e))


class ThreadedTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    pass


if __name__ == "__main__":
    configure()

    logging.info("Starting server...")

    server = ThreadedTCPServer(("localhost", PORT), RequestHandler)
    server.allow_reuse_address = True

    with server:
        listener_thread = threading.Thread(target=server.serve_forever)

        listener_thread.daemon = True
        listener_thread.start()

        server.serve_forever()
