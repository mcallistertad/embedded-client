ARMCC = $(shell which armcc)

ifeq ($(ARMCC), )
CFLAGS = -Wall -Os --std=c99 -DVERSION=\"$(GIT_VERSION)\"
else
CC = armcc
CFLAGS = --c90 --no_strict -Ospace -DVERSION=\"$(GIT_VERSION)\"
endif

GIT_VERSION := $(shell git describe --dirty --always --tags)

# Disposable build products are deposited in build dir
# Durable build products are deposited in bin dir
BUILD_DIR = build
BIN_DIR = bin
API_DIR = libel
SKY_PROTO_DIR = $(API_DIR)/protocol
NANO_PB_DIR = .submodules/nanopb
AES_DIR = .submodules/tiny-AES128-C

GENERATED_SRCS = ${SKY_PROTO_DIR}/el.pb.h ${SKY_PROTO_DIR}/el.pb.c

INCLUDES = -I${SKY_PROTO_DIR} -I${NANO_PB_DIR} -I${AES_DIR} -I${API_DIR}

VPATH = ${SKY_PROTO_DIR}:${API_DIR}:${NANO_PB_DIR}:${AES_DIR}

LIBELG_SRCS = libel.c utilities.c beacons.c crc32.c
PROTO_SRCS = ${SKY_PROTO_DIR}/proto.c ${SKY_PROTO_DIR}/el.pb.c p${NANO_PB_DIR}/pb_common.c ${NANO_PB_DIR}/pb_encode.c ${NANO_PB_DIR}/pb_decode.c
TINYAES_SRCS = ${AES_DIR}/aes.c

LIBELG_ALL = ${LIBELG_SRCS} ${PROTO_SRCS} ${TINYAES_SRCS} 
LIBELG_OBJS = $(addprefix ${BUILD_DIR}/, $(notdir $(LIBELG_ALL:.c=.o)))

all: .submodules/nanopb/.git .submodules/tiny-AES128-C/.git .submodules/embedded-protocol/.git lib unit_test

.submodules/nanopb/.git:
	@echo "submodule nanopb must be provided! Did you download embedded-client-X.X.X.tgz? Exiting..."
	exit 1

.submodules/tiny-AES128-C/.git:
	@echo "submodule tiny-AES128-C must be provided! Did you download embedded-client-X.X.X.tgz? Exiting..."
	exit 1

.submodules/embedded-protocol/.git:
	@echo "submodule embedded-protocol must be provided! Did you download embedded-client-X.X.X.tgz? Exiting..."
	exit 1

lib: ${BIN_DIR} ${BUILD_DIR} ${BIN_DIR}/libel.a

unit_test: ${BUILD_DIR}/unit_test.o ${BIN_DIR}/libel.a
	$(CC) -lc -lm -o ${BIN_DIR}/unit_test \
	${BUILD_DIR}/unit_test.o ${BIN_DIR}/libel.a

${BIN_DIR}/libel.a: ${GENERATED_SRCS} ${LIBELG_OBJS}
	ar rcs $@ ${LIBELG_OBJS}

${BIN_DIR} ${BUILD_DIR}:
	mkdir -p $@

# Generates the protobuf source files.
${GENERATED_SRCS}:
	make -C ${SKY_PROTO_DIR}

# Need an explicit rule for this one since the source file is generated code.
${BUILD_DIR}/el.pb.o: ${SKY_PROTO_DIR}/el.pb.c
	$(CC) -c $(CFLAGS) ${INCLUDES} -o $@ $<

${BUILD_DIR}/%.o: %.c
	$(CC) -c $(CFLAGS) ${INCLUDES} -o $@ $<

clean:
	make -C ${SKY_PROTO_DIR} clean
	rm -rf ${BIN_DIR} ${BUILD_DIR}
