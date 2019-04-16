#ifndef SIM_SEND_H
#define SIM_SEND_H

int send_request(char *request, int req_size, uint8_t *response, char *server, int port);

#endif
