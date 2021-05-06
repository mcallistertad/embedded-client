CFLAGS = -Os -std=c99 ${DEBUG}
LIB_DIR = ../bin
AES_DIR = ../submodules/tiny-AES128-C

INCLUDES = -I. -I../libel -I${AES_DIR}
CLIENT_NAME = sample_client

CLIENT_SRCS = sample_client.c send.c config.c
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

${CLIENT_NAME}: ${CLIENT_OBJS} ${LIB_DIR}/libel.a
	$(CC) -lc -o ${CLIENT_NAME} ${CLIENT_OBJS} ${LIB_DIR}/libel.a -lm

%.o: %.c
	$(CC) -c $(CFLAGS) ${INCLUDES} -o $@ $<

clean:
	rm -f *.o $(CLIENT_NAME) nv_state
