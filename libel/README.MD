# Skyhook Embedded Location (ELG) Gateway API Version 3.0

## Overview
Skyhook ELG provides proxy access via a lightweight binary exchange, to there XML over HTTP API server.
The ELG API is targetted at embedded devices (and Version 3.0 specifically targets IoT devices).
This release introduces the following features:
- Cached locations. No network traffic when device is at a known location (e.g. not moving)
- Expandable interface which allows nb-iot beacons to be added while locating from AP beacons
- Expandable to include Bluetooth beacons
- Utilizes Google Protobufs providing an expandable protocol to Skyhook ELG Gateway
- Portable; reusable across hardware architectures, software tools
- Shall be re-entrant (thread and process)
