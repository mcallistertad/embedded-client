Skyhook embedded-lib
====================

The Skyhook embeeded-lib is a small library written in C. It is intended to be
included in embedded targets (e.g., IoT devices) to allow those devices to use
the Skyhook Precision Location service in order to obtain an estimate of the
geolocation of the device on which it runs.

Dependencies
------------

The library depends on several third-party repos which are included as
submodules in this repo. In order to ensure that these dependency repos are
properly populated, be sure to add the `--recursive` option to the `git
clone` command when cloning this repo.

All submodules are contained within the `.submodules` directory.

Library Contents
----------------

Building the library following directions below, produces one result, bin/libel.a
which contains the following modules, with the listed header file dependencies:

    libel/libel.c -> libel.o
        libel/beacons.h
        libel/config.h
        libel/crc32.h
        libel/libel.h
        libel/protocol/proto.h
        libel/utilities.h
        libel/workspace.h
    libel/utilities.c -> utilities.o
        libel/beacons.h
        libel/config.h
        libel/crc32.h
        libel/libel.h
        .submodules/tiny-AES128-C/aes.h
        libel/utilities.h
        libel/workspace.h
    libel/beacons.c -> beacons.o
        libel/beacons.h
        libel/config.h
        libel/crc32.h
        libel/libel.h
        .submodules/tiny-AES128-C/aes.h
        libel/utilities.h
        libel/workspace.h
    libel/crc32.c -> crc32.o
    libel/protocol/proto.c -> proto.o
        libel/beacons.h
        libel/config.h
        libel/crc32.h
        libel/libel.h
        libel/protocol/el.pb.h
        libel/protocol/proto.h
        libel/utilities.h
        libel/workspace.h
    libel/protocol/el.pb.c -> el.pb.o*
        libel/protocol/el.pb.h
    .submodules/nanopb/pb_common.c -> pb_common.o
    .submodules/nanopb/pb_encode.c -> pb_encode.o
    .submodules/nanopb/pb_decode.c -> pb_decode.o
    .submodules/tiny-AES128-C/aes.c -> aes.o

*note el.pb.c is generated by Google protoc tool

Build Directions
----------------

To build just the library in a Unix-like environment:

    $ make

To build and run the sample client:

    $ cd sample_client
    $ make
    $ ./sample_client sample_client.conf

Note that `sample_client.conf` will likely require modification (to add your
Skyhook AES key and partner ID).
