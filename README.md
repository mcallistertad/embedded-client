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

Build Directions
----------------

To build just the library in a Unix-like environment:

    $ make

To build the sample client:

    $ cd sample_client
    $ make
    $ ./sample_client sample_client.conf

Note that `sample_client.conf` will likely require modification (to add your
Skyhook AES key and partner ID).
