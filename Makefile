ARMCC = $(shell which armcc)

ifeq ($(ARMCC), )
CFLAGS = -Wall -Werror -Os -std=c99
else
CC = armcc
CFLAGS = --c99 -Ospace
endif

# Disposable build products are deposited in build dir
# Durable build products are deposited in bin dir
BUILD_DIR = build
BIN_DIR = bin
API_DIR = libel
SKY_PROTO_DIR = $(API_DIR)/protocol
NANO_PB_DIR = .submodules/nanopb
AES_DIR = .submodules/tiny-AES128-C

GENERATED_SRCS = el.pb.h el.pb.c

INCLUDES = -I${SKY_PROTO_DIR} -I${NANO_PB_DIR} -I${AES_DIR} -I${API_DIR}

VPATH = ${SKY_PROTO_DIR}:${API_DIR}:${NANO_PB_DIR}:${AES_DIR}

PROTO_SRCS = el.pb.c proto.c el.pb.c pb_common.c pb_encode.c pb_decode.c aes.c

LIBELG_SRCS = libel.c utilities.c beacons.c crc32.c ${PROTO_SRCS}
LIBELG_OBJS = $(addprefix ${BUILD_DIR}/, $(LIBELG_SRCS:.c=.o))

.PHONY: unit_test proto lib

lib: ${BIN_DIR} ${BUILD_DIR} ${BIN_DIR}/libel.a

unit_test: ${BUILD_DIR}/unit_test.o ${BIN_DIR}/libel.a
	$(CC) -lc -o ${BIN_DIR}/unit_test \
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
