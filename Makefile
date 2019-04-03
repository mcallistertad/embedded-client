CFLAGS = -Wall -Werror -Os -std=c99
CFLAGS += -DCRYPTO_ENABLED # Currently a no-op.
BIN_DIR = bin
SKY_PROTO_DIR = protocol
PROTO_BUFS_DIR = .submodules/nanopb
AES_DIR = .submodules/tiny-AES128-C
INCLUDES = -I${SKY_PROTO_DIR} -I${PROTO_BUFS_DIR} -I${AES_DIR}

PROTO_SRCS = elg.pb.c proto.c pb_common.c pb_encode.c pb_decode.c
PROTO_OBJS = $(addprefix ${BIN_DIR}/, $(PROTO_SRCS:.c=.o))

LIBELG_SRCS = libelg.c utilities.c beacons.c crc32.c
LIBELG_OBJS = $(addprefix ${BIN_DIR}/, $(LIBELG_SRCS:.c=.o))

# .PHONY: eg_client unit_test proto skylib client pyparse aeslib libelg
.PHONY: unit_test proto skylib client pyparse aeslib libelg

skylib: ${BIN_DIR} ${BIN_DIR}/skylib.a

aeslib: ${AES_DIR}/aes.c ${AES_DIR}/aes.h
	make -C ${AES_DIR} aes.a

unit_test: ${BIN_DIR}/unit_test.o ${BIN_DIR}/libelg.a
	$(CC) -lc -o ${BIN_DIR}/unit_test \
	${BIN_DIR}/unit_test.o ${BIN_DIR}/libelg.a

eg_client: ${BIN_DIR}/eg_client.o ${BIN_DIR}/libelg.a
	$(CC) -lc -o ${BIN_DIR}/eg_client \
	${BIN_DIR}/eg_client.o ${BIN_DIR}/libelg.a

client: skylib ${BIN_DIR}/client.o aeslib
	$(CC) -lc -o ${BIN_DIR}/client \
	${BIN_DIR}/client.o ${BIN_DIR}/skylib.a ${AES_DIR}/aes.a

${BIN_DIR}/libelg.a: ${LIBELG_OBJS}
	ar rcs $@ ${LIBELG_OBJS}

${BIN_DIR}/skylib.a: proto ${PROTO_OBJS}
	ar rcs $@ ${PROTO_OBJS}

${BIN_DIR}:
	mkdir -p ${BIN_DIR}

# Generates the elg protobuf source files.
proto:
	make -C ${SKY_PROTO_DIR}

# Explicitly defined dependencies.
${BIN_DIR}/libelg.o: libelg/libelg.c
${BIN_DIR}/crc32.o: libelg/crc32.c
${BIN_DIR}/utilities.o: libelg/utilities.c
${BIN_DIR}/beacons.o: libelg/beacons.c
${BIN_DIR}/eg_client.o: libelg/eg_client.c
${BIN_DIR}/unit_test.o: libelg/unit_test.c
${BIN_DIR}/client.o: client/client.c
${BIN_DIR}/elg.pb.o: ${SKY_PROTO_DIR}/elg.pb.c ${SKY_PROTO_DIR}/elg.pb.h
${BIN_DIR}/proto.o: ${SKY_PROTO_DIR}/proto.c ${SKY_PROTO_DIR}/proto.h
${BIN_DIR}/pb_common.o: ${PROTO_BUFS_DIR}/pb_common.c ${PROTO_BUFS_DIR}/pb_common.h ${PROTO_BUFS_DIR}/pb.h
${BIN_DIR}/pb_encode.o: ${PROTO_BUFS_DIR}/pb_encode.c ${PROTO_BUFS_DIR}/pb_common.h ${PROTO_BUFS_DIR}/pb.h ${PROTO_BUFS_DIR}/pb_encode.h 
${BIN_DIR}/pb_decode.o: ${PROTO_BUFS_DIR}/pb_decode.c ${PROTO_BUFS_DIR}/pb_common.h ${PROTO_BUFS_DIR}/pb.h ${PROTO_BUFS_DIR}/pb_decode.h 

${PROTO_OBJS} ${BIN_DIR}/client.o:
	$(CC) -c $(CFLAGS) ${INCLUDES} -o $@ $<

${LIBELG_OBJS} ${BIN_DIR}/unit_test.o:
	$(CC) -c $(CFLAGS) ${INCLUDES} -o $@ $<

${LIBELG_OBJS} ${BIN_DIR}/eg_client.o:
	$(CC) -c $(CFLAGS) ${INCLUDES} -o $@ $<

clean:
	make -C ${SKY_PROTO_DIR} clean
	make -C ${AES_DIR} clean
	rm -f ${BIN_DIR}/*
