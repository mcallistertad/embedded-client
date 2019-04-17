#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <unistd.h>

#include "sim_config.h"

/* returns number of result bytes that were successfully parsed */
uint32_t hex2bin(char *hexstr, uint32_t hexlen, uint8_t *result, uint32_t reslen) {
    uint32_t i, j = 0, k = 0;

    for (i = 0; i < hexlen; i++) {
        uint8_t c = (uint8_t) hexstr[i];

        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'a' && c <= 'f')
            c = (uint8_t) ((c - 'a') + 10);
        else if (c >= 'A' && c <= 'F')
            c = (uint8_t) ((c - 'A') + 10);
        else
            continue;

        if (k++ & 0x01)
            result[j++] |= c;
        else
            result[j] = c << 4;

        if (j >= reslen)
            break;
    }

    return j;
}

int32_t bin2hex(char *buff, int32_t buff_len, uint8_t *data, int32_t data_len) {
    const char * hex = "0123456789ABCDEF";

    char *p;
    int32_t i;

    if (buff_len < 2 * data_len)
        return -1;

    p = buff;

    for (i = 0; i < data_len; i++) {
        *p++ = hex[data[i] >> 4 & 0x0F];
        *p++ = hex[data[i] & 0x0F];
    }

    return 0;
}


int load_config(char *filename, Config_t *config, int client_id)
{
    char line[100];
    char str[32];
    char *p;
    bool client_found = false;
    int val;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Error: unable to open config file - %s", filename);
        return -1;
    }
    while (fgets(line, sizeof(line), fp)) {
        if (strlen(line) < 4)
            continue;

        if ((p = strchr(line, '#')) != NULL)
            *p = '\0';

        if (sscanf(line, "SERVER %256s", config->server) == 1) {
            continue;
        }

        if (sscanf(line, "PORT %d", &val) == 1) {
            config->port = (uint16_t) (val & 0xFFFF);
            continue;
        }

        if (sscanf(line, "KEY %s", str) == 1) {
            hex2bin(str, KEY_SIZE * 2, config->key, KEY_SIZE);
            continue;

        }

        if (sscanf(line, "PARTNER_ID %d", &val) == 1) {
            config->partner_id = (uint16_t) (val & 0xFFFF);
            continue;
        }

        if (sscanf(line, "CLIENT_ID %d", &val) == 1) {

            if (client_found)
                break;

            if (val == client_id || client_id == 0)
                client_found = true;
            else
                continue;
            config->client_id = (uint16_t) (val & 0xFFFF);
            continue;
        }

        if (client_found) {

            if (sscanf(line, "SCAN_FILE %256s", config->scan_file) == 1) {
                continue;
            }

            if (sscanf(line, "DEVICE_MAC %s", str) == 1) {
                hex2bin(str, MAC_SIZE * 2, config->device_mac, MAC_SIZE);
                continue;
            }

            if (sscanf(line, "DELAY %d", &val) == 1) {
                config->delay = (uint16_t) (val & 0xFFFF);
                continue;
            }
        }
    }
    fclose(fp);
    print_config(config);
    printf("Config Loaded\n");

    return 0;

}

void print_config(Config_t *config) {
    char key[KEY_SIZE * 2 + 1];
    bin2hex(key, KEY_SIZE * 2, config->key, KEY_SIZE);
    key[KEY_SIZE * 2] = '\0';
    char device[MAC_SIZE * 2 + 1];
    bin2hex(device, MAC_SIZE * 2, config->device_mac, MAC_SIZE);
    device[MAC_SIZE * 2] = '\0';

    printf("Configuration for Client #%d\n",config->client_id);
    printf("Server: %s\n",config->server);
    printf("Port: %d\n",config->port);
    printf("Key: %32s\n",key);
    printf("Partner Id: %d\n",config->partner_id);
    printf("Device: %12s\n",device);
    printf("Scan File: %s\n",config->scan_file);
    printf("Delay: %d\n",config->delay);

}
