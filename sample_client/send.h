#ifndef TEST_SEND_H
#define TEST_SEND_H

int send_request(char *request, int req_size, uint8_t *response, int resp_size,
    char *server, int port);

#endif
