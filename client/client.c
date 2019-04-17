#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include "sys/time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aes.h"
#include "proto.h"

//#define SERVER_HOST "elg.skyhook.com"
#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 9756
#define PARTNER_ID 2
#define AES_KEY "000102030405060708090a0b0c0d0e0f"
#define CLIENT_MAC "deadbeefdead"

/* ------- Standin for Geoff's stuff --------------------------- */
struct Ap {
    uint8_t mac[6];
    uint32_t age; // ms
    uint32_t channel;
    int32_t rssi;
    bool connected;
};

static struct Ap aps[] = {
    { {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}, /* age */ 2222, /* channel */ 10, /* rssi */ -150, false},
    { {0xde, 0xad, 0xbe, 0xef, 0xde, 0xad}, /* age */ 2222, /* channel */ 10, /* rssi */ -150, false},
    { {0xde, 0xad, 0xbe, 0xef, 0xde, 0xad}, /* age */ 2222, /* channel */ 10, /* rssi */ -150, false},
    { {0xde, 0xad, 0xbe, 0xef, 0xde, 0xad}, /* age */ 2222, /* channel */ 10, /* rssi */ -150, false},
    { {0xde, 0xad, 0xbe, 0xef, 0xde, 0xad}, /* age */ 2222, /* channel */ 10, /* rssi */ -150, false},
    { {0xde, 0xad, 0xbe, 0xef, 0xde, 0xad}, /* age */ 2222, /* channel */ 162, /* rssi */ -150, true},
    { {0xde, 0xad, 0xbe, 0xef, 0xde, 0xad}, /* age */ 2222, /* channel */ 10, /* rssi */ -150, false},
    { {0xde, 0xad, 0xbe, 0xef, 0xde, 0xad}, /* age */ 2222, /* channel */ 10, /* rssi */ -150, false},
    { {0xde, 0xad, 0xbe, 0xef, 0xde, 0xad}, /* age */ 2222, /* channel */ 10, /* rssi */ -150, false},
    { {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}, /* age */ 2222, /* channel */ 160, /* rssi */ -10, false}
};

uint8_t* get_ap_mac(Sky_ctx_t* ctx, uint32_t idx)
{
    return aps[idx].mac;
}

bool get_ap_is_connected(Sky_ctx_t* ctx, uint32_t idx)
{
    return aps[idx].connected;
}

uint32_t get_num_aps(Sky_ctx_t* ctx)
{
    return sizeof(aps) / sizeof(struct Ap);
}

int64_t get_ap_channel(Sky_ctx_t* ctx, uint32_t idx)
{
    return aps[idx].channel;
}

int64_t get_ap_rssi(Sky_ctx_t* ctx, uint32_t idx)
{
    return aps[idx].rssi;
}

int64_t get_ap_age(Sky_ctx_t* ctx, uint32_t idx)
{
    return aps[idx].age;
}

struct Gsm {
    uint32_t mcc;
    uint32_t mnc;
    uint32_t lac;
    uint32_t ci;
    int32_t rssi;
    uint32_t age;
    bool connected;
};

static struct Gsm gsm_cells[] = {
    {310, 410, 512, 6676, -130, 1000, false}, 
    //{510, 610, 513, 6677, -13, 1001, true}
};

int64_t get_gsm_mcc(Sky_ctx_t* ctx, uint32_t idx)
{
    return gsm_cells[idx].mcc;
}

int64_t get_gsm_mnc(Sky_ctx_t* ctx, uint32_t idx)
{
    return gsm_cells[idx].mnc;
}

int64_t get_gsm_lac(Sky_ctx_t* ctx, uint32_t idx)
{
    return gsm_cells[idx].lac;
}

int64_t get_gsm_ci(Sky_ctx_t* ctx, uint32_t idx)
{
    return gsm_cells[idx].ci;
}

bool get_gsm_is_connected(Sky_ctx_t* ctx, uint32_t idx)
{
    return gsm_cells[idx].connected;
}

uint32_t get_num_gsm(Sky_ctx_t* ctx)
{
    return sizeof(gsm_cells) / sizeof(struct Gsm);
}

int64_t get_gsm_rssi(Sky_ctx_t* ctx, uint32_t idx)
{
    return gsm_cells[idx].rssi;
}

int64_t get_gsm_age(Sky_ctx_t* ctx, uint32_t idx)
{
    return gsm_cells[idx].age;
}

struct Nbiot {
    uint32_t mcc;
    uint32_t mnc;
    uint32_t tac;
    uint32_t ecid;
    int32_t nrsrp;
    uint32_t age;
    bool connected;
};

static struct Nbiot nbiot_cells[] = {
    {310, 410, 512, 6676, -130, 1000, false}, 
    {510, 610, 513, 6677, -13, 1001, true}
};

int64_t get_nbiot_mcc(Sky_ctx_t* ctx, uint32_t idx)
{
    return nbiot_cells[idx].mcc;
}

int64_t get_nbiot_mnc(Sky_ctx_t* ctx, uint32_t idx)
{
    return nbiot_cells[idx].mnc;
}

int64_t get_nbiot_tac(Sky_ctx_t* ctx, uint32_t idx)
{
    return nbiot_cells[idx].tac;
}

int64_t get_nbiot_ecellid(Sky_ctx_t* ctx, uint32_t idx)
{
    return nbiot_cells[idx].ecid;
}

bool get_nbiot_is_connected(Sky_ctx_t* ctx, uint32_t idx)
{
    return nbiot_cells[idx].connected;
}

uint32_t get_num_nbiot(Sky_ctx_t* ctx)
{
    return sizeof(nbiot_cells) / sizeof(struct Nbiot);
}

int64_t get_nbiot_rssi(Sky_ctx_t* ctx, uint32_t idx)
{
    return nbiot_cells[idx].nrsrp;
}

int64_t get_nbiot_age(Sky_ctx_t* ctx, uint32_t idx)
{
    return nbiot_cells[idx].age;
}

static unsigned char buf[1024];

uint8_t *get_ctx_request(Sky_ctx_t *ctx) { return buf; }

size_t get_ctx_request_size(Sky_ctx_t *ctx) { return sizeof(buf); }

uint32_t get_ctx_partner_id(Sky_ctx_t *ctx) { return 2; }

static void hex_str_to_bin(const char* hex_str, uint8_t bin_buff[], size_t buff_len)
{
    const char* pos = hex_str;
    size_t i;

    for (i=0; i < buff_len; i++)
    {
        sscanf(pos, "%2hhx", &bin_buff[i]);
        pos += 2;
    }
}

uint8_t *get_ctx_aes_key(Sky_ctx_t *ctx)
{
    static unsigned char aes_key[16];
    hex_str_to_bin(AES_KEY, aes_key, sizeof(aes_key));
    return aes_key;
}

uint32_t get_ctx_aes_key_id(Sky_ctx_t *ctx) { return 0; }

static unsigned char device_id[6]; // e.g., MAC address.

uint8_t *get_ctx_device_id(Sky_ctx_t *ctx)
{
    hex_str_to_bin(CLIENT_MAC, device_id, sizeof(device_id));
    return device_id;
}

uint32_t get_ctx_id_length(Sky_ctx_t *ctx) { return sizeof(device_id); }

/* ------- End of standin for Geoff's stuff --------------------- */

bool hostname_to_ip(char* hostname, char* ip, uint16_t ip_len)
{
    struct hostent* he = gethostbyname(hostname);

    if (he == NULL)
    {
        printf("gethostbyname failed");
        return false;
    }

    struct in_addr** addr_list = (struct in_addr **) he->h_addr_list;

    for (size_t i = 0; addr_list[i] != NULL; i++)
    {
        //Return the first one;
        strncpy(ip, inet_ntoa(*addr_list[i]), ip_len);
        return true;
    }

    return false;
}

int main(int argc, char** argv)
{
    (void) argc;
    (void) argv;

    // Serialize request.
    int32_t len = serialize_request(0);

    if (len < 0)
    {
        printf("Failed to serialize (buf too small?)\n");
        exit(-1);
    }

    // Write request to a file.
    FILE* fp = fopen("rq.bin", "wb");

    fwrite(buf, len, 1, fp);
    fclose(fp);

    // Send request to server.
    //
    struct sockaddr_in serv_addr;

    memset(&serv_addr, 0, sizeof(serv_addr));

    char ipaddr[16]; // the char representation of an ipv4 address

    // Lookup server ip address.
    if (!hostname_to_ip(SERVER_HOST, ipaddr, sizeof(ipaddr)))
    {
        printf("Could not resolve host %s\n", SERVER_HOST);
        exit(-1);
    }

    // Init server address struct and set ip and port.
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    serv_addr.sin_addr.s_addr = inet_addr(ipaddr);

    // Open socket.
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1)
    {
        printf("cannot open socket\n");
        exit(-1);
    }

    // Set socket timeout to 10 seconds.
    struct timeval tv = {10, 0};

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(struct timeval)))
    {
        printf("setsockopt failed\n");
        exit(-1);
    }

    // Connect.
    int32_t rc = connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));

    if (rc < 0)
    {
        close(sockfd);
        printf("connect to server failed\n");
        exit(-1);
    }

    // Send request.
    rc = send(sockfd, buf, (size_t) len, 0);

    printf("Sent %d bytes to server\n", len);

    if (rc != (int32_t)len)
    {
        close(sockfd);
        printf("send() sent a different number of bytes (%d) from expected\n", rc);
        exit(-1);
    }

    // Read response.
    rc = recv(sockfd, &buf, sizeof(buf), MSG_WAITALL);

    float lat, lon;
    uint32_t hpe;

    if (deserialize_response(0, get_ctx_request(0), get_ctx_request_size(0), &lat, &lon, &hpe) < 0)
    {
        printf("deserialization failed!\n");
    }
    else
    {
        printf("lat/lon/hpe = %f/%f/%u\n", lat, lon, hpe);
    }
}
