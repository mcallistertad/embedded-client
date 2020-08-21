# Generate the nanopb source files from the el.proto.
include ../../.submodules/nanopb/extra/nanopb.mk
CFLAGS = -Wall -Os --std=c99 -DVERSION=\"$(GIT_VERSION)\"

TARGETS = el.pb.c el.pb.h

default: ${TARGETS}

clean:
	rm -f ${TARGETS}
