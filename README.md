Skyhook Embedded Client Library
===============================

The Skyhook Embedded Client is a small library written in C. It is intended to be
included in embedded targets (e.g., IoT devices) to allow those devices to use
the Skyhook Precision Location service in order to obtain an estimate of the
geolocation of the device on which it runs. This repo also includes a sample client application
which illustrates how the library can be used by your application (see below).

Instructions for cloning and building the library are below. Instructions for
integrating the library into your application are
[here](http://resources.skyhookwireless.com/wiki/type/documentation/precision-location/embedded-client-library-api-documentation/199269838).

Dependencies
------------

### Git Submodules
The library depends on several third-party repos which are included as
submodules in this repo. In order to ensure that these dependency repos are
properly populated, be sure to add the `--recursive` option to the `git
clone` command when cloning this repo.

All submodules are contained within the `.submodules` directory.

### Google Protocol Buffers And Python
The client/server network protocol used by the Skyhook library is based on
[Google Protocol Buffers](https://developers.google.com/protocol-buffers/).
Therefore the following components are required to build the library:
* Python version 3.6.0 or later. Under Linux, python2 is often the default
  version. Make the appropriate adjustments to your path in order to prioritize
  python3 (and pip3) in this case. E.g.,
```
$ ln -s /usr/bin/python3 /usr/local/bin/python
$ ln -s /usr/bin/pip3 /usr/local/bin/pip
```
(actual paths may differ depending on your system configuration)
* Google Protocol Buffers `protoc` compiler version 3.3.0 or later. A pre-built
  binary version of the compiler for Linux x64 (and other platforms) can be
  downloaded from https://github.com/protocolbuffers/protobuf/releases. For
  example, download protoc-3.9.0-linux-x86_64.zip (or a more recent version)
  from that page, and copy `bin/protoc` from the archive to an appropriate
  place in your path (e.g., `/usr/local/bin`). The other files in the
  downloaded archive are not needed, and can be discarded.
* Google protobuf Python module version 3.6.1 or later (normally installed via
  `$ pip install protobuf`)

On Linux, you should see results similar to the below if the appropriate
versions are installed and active:
```
$ python --version
Python 3.6.8
$ protoc --version
libprotoc 3.9.0
$ pip list | grep proto
protobuf        3.6.1
```

Library Source Files
--------------------

The library includes the following C source files:

* All `.h` and `.c` files in the `libel/` directory
* All `.h` and `.c` files in the `libel/protocol` directory (note that `el.pb.h`
  and `el.pb.c` are generated at build time by the protocol buffers compiler)
* `.submodules/tiney-AES128-C/aes.{h,c}`
* `.submodules/nanopb/pb_common.c`
* `.submodules/nanopb/pb_encode.c`
* `.submodules/nanopb/pb_decode.c`

Build Directions
----------------

To build just the library in a Unix-like environment:

    $ make

This creates the file `bin/libel.a`, which can then be statically linked into
your executable (in a Unix-like environment). Of course it may be necessary to
modify the provided Makefile (or replace it altogether) in order to build the
library for your platform.

To build and run the sample client (after building the library itself):

    $ cd sample_client
    $ make
    $ ./sample_client sample_client.conf

Note that `sample_client.conf` will likely require modification (to add your
Skyhook AES key and partner ID).
