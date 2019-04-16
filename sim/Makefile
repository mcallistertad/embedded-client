CFLAGS = -g -Wall -Werror -Os -std=c99
BIN_DIR = ../bin
LIBELG_DIR = ../libelg
JSON_DIR = libjson
INCLUDES = -I. -I${LIBELG_DIR} -I${JSON_DIR}
SIM_NAME = elgsim
CLIENT_NAME = client

SIM_SRCS = elgsim.c sim_beacons.c sim_send.c sim_config.c
SIM_OBJS = $(SIM_SRCS:.c=.o)

CLIENT_SRCS = client.c sim_send.c sim_config.c
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

.PHONY: ${SIM_NAME} ${CLIENT_NAME}

${SIM_NAME}: ${SIM_OBJS} ${BIN_DIR}/libelg.a ${JSON_DIR}/.libs/libjson-c.a
	$(CC) -lc -o $(SIM_NAME) ${SIM_OBJS} ${BIN_DIR}/libelg.a ${JSON_DIR}/.libs/libjson-c.a

${CLIENT_NAME}: ${CLIENT_OBJS} ${BIN_DIR}/libelg.a ${JSON_DIR}/.libs/libjson-c.a
	$(CC) -lc -o ${CLIENT_NAME} ${CLIENT_OBJS} ${BIN_DIR}/libelg.a


# Explicitly defined dependencies.
elgsim.o: elgsim.c
sim_beacons.o: sim_beacons.c
sim_send.o: sim_send.c
sim_config.o: sim_config.c
client.o: client.c

elgsim.o:
	$(CC) -c $(CFLAGS) ${INCLUDES} -o $@ $<

sim_beacons.o:
	$(CC) -c $(CFLAGS) ${INCLUDES} -o $@ $<

sim_send.o:
	$(CC) -c $(CFLAGS) ${INCLUDES} -o $@ $<

sim_config.o:
	$(CC) -c $(CFLAGS) ${INCLUDES} -o $@ $<
	
client.o:
	$(CC) -c $(CFLAGS) ${INCLUDES} -o $@ $<

clean:
	rm  *.o $(SIM_NAME) $(CLIENT_NAME)
