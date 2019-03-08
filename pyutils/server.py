#!/usr/bin/env python
import socketserver
import threading
import logging.config
import elg_proto
import yaml
from xml.etree import ElementTree as ET
from io import BytesIO
import urllib.request

PORT=9756
SOCKET_TIMEOUT=5

def configure():
    with open("server.yaml") as f:
        config = yaml.load(f)

    logging.config.dictConfig(config['log_config'])


def forward_rq_to_api_server(rq, url, api_key):
    result = None
    error_msg = None

    try:
        root = ET.Element('LocationRQ')
        root.set('xmlns:xsi', 'http://www.w3.org/2001/XMLSchema-instance')
        root.set('xsi:schemaLocation', 'http://skyhookwireless.com/wps/2005 ../../src/xsd/location.xsd')
        root.set('xmlns', 'http://skyhookwireless.com/wps/2005')
        root.set('version', '2.25')

        auth = ET.SubElement(root, 'authentication')
        auth.set('version', '2.2')

        key = ET.SubElement(auth, 'key')
        key.set('key', api_key)
        key.set('username', 'elg')

        event = None

        # AP scans.
        if 'aps' in rq:
            aps = list(zip(rq.aps.mac, rq.aps.rssi, rq.aps.band))

            for ap in aps:
                ap = ET.SubElement(root, 'access-point')

                ET.SubElement(ap, 'mac').text = ap[0]
                ET.SubElement(ap, 'signal-strength').text = str(ap[1])

        et = ET.ElementTree(root)
        body = BytesIO()
        et.write(body, encoding='utf-8', xml_declaration=True) 

        api_request = body.getvalue()

        req = urllib.request.Request(url, body.getvalue(), {'Content-Type': 'text/xml'})

        try:
            f = urllib.request.urlopen(req)
            api_response = f.read().strip()
        finally:
            try:
                f.close()
            except:
                pass

        # Parse the XML response and convert it to JSON.
        #
        # Get the XML root after stripping off pesky namespace attribute.
        resp_root = ET.fromstring(re.sub(' xmlns="[^"]+"', '', api_response, count=1))

        location = resp_root.find('location')

        # FIXME: populate and return a protobuf RS message (not a "result" dict).
        result = {}

        result['user'] = event['username']
        result['lat'] = location.find('latitude').text
        result['lon'] = location.find('longitude').text
        result['hpe'] = location.find('hpe').text
        result['time'] = int(calendar.timegm(time.gmtime()))

    except Exception as e:
        error_msg = type(e).__name__ + ': ' + str(e)

    return result 

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
