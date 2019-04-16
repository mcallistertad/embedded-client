CFLAGS = -Wall -Werror -Os -std=c99
BIN_DIR = ../bin
LIBELG_DIR = ../libelg
INCLUDES = -I. -I${LIBELG_DIR}
CLIENT_NAME = test_client

CLIENT_SRCS = test_client.c test_send.c test_config.c
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

.PHONY: ${CLIENT_NAME}

${CLIENT_NAME}: ${CLIENT_OBJS} ${BIN_DIR}/libelg.a
	$(CC) -lc -o ${CLIENT_NAME} ${CLIENT_OBJS} ${BIN_DIR}/libelg.a

# Explicitly defined dependencies.
test_client.o: test_client.c
test_send.o: test_send.c
test_config.o: test_config.c

test_client.o:
	$(CC) -c $(CFLAGS) ${INCLUDES} -o $@ $<
	
test_send.o:
	$(CC) -c $(CFLAGS) ${INCLUDES} -o $@ $<

test_config.o:
	$(CC) -c $(CFLAGS) ${INCLUDES} -o $@ $<
	
clean:
	rm  *.o $(CLIENT_NAME)
