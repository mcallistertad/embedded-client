# Build the elg probobuf header and source file.
include ../.submodules/nanopb/extra/nanopb.mk

PROTOC_OPTS += --python_out=.

TARGETS = elg.pb.c elg.pb.h elg_pb2.py

default: ${TARGETS} ../pyparse/elg_pb2.py

../pyparse/elg_pb2.py: elg_pb2.py
	cp $< $@

clean:
	rm -f ${TARGETS}
