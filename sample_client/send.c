/*
 * Copyright (c) 2019 Skyhook, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include "sys/time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "send.h"

bool hostname_to_ip(char *hostname, char *ip, uint16_t ip_len)
{
    struct hostent *he = gethostbyname(hostname);

    if (he == NULL) {
        printf("Error: unable to host by name\n");
        return false;
    }

    struct in_addr **addr_list = (struct in_addr **)he->h_addr_list;

    for (size_t i = 0; addr_list[i] != NULL; i++) {
        //Return the first one;
        strncpy(ip, inet_ntoa(*addr_list[i]), ip_len);
        return true;
    }

    return false;
}

int send_request(
    char *request, int req_size, uint8_t *response, int resp_size, char *server, int port)
{
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    char ipaddr[16]; // the char representation of an ipv4 address

    // Lookup server ip address.
    if (!hostname_to_ip(server, ipaddr, sizeof(ipaddr))) {
        printf("Error: Could not resolve host %s\n", server);
        return -1;
    }

    // Init server address struct and set ip and port.
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(ipaddr);

    // Open socket.
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Error: cannot open socket\n");
        return -1;
    }

    // Set socket timeout to 10 seconds.
    struct timeval tv = { 10, 0 };
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval))) {
        printf("Error: setsockopt failed\n");
        return -1;
    }

    // Connect.
    int32_t rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (rc < 0) {
        close(sockfd);
        printf("Error: unable to connect to server\n");
        return -1;
    }

    // Send request.
    rc = send(sockfd, request, (size_t)req_size, 0);
    if (rc != (int32_t)req_size) {
        close(sockfd);
        printf("Error: sent a different number of bytes (%d) from expected\n", rc);
        return -1;
    }

    // Read response.
    rc = recv(sockfd, response, resp_size, MSG_WAITALL);
    if (rc < 0) {
        printf("Error: unable to receive response\n");
        return -1;
    }

    return (int)rc;
}
