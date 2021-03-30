/*! \file sample_client/config.c
 *  \brief Skyhook Embedded Library
 *
 * Copyright (c) 2020 Skyhook, Inc.
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include "libel.h"
#include "config.h"

/* Maximum length of line in config file */
/* Needs to be large enough to hold 256 byte value and longest key */
#define MAX_LINE_LENGTH 300

/*! \brief convert ascii hex string to binary
 *
 *  @param hexstr pointer to the hex string
 *  @param hexlen length of the hex string
 *  @param result pointer to the result buffer
 *  @param reslen length of the result buffer
 *
 *  @return number of result bytes that were successfully parsed
 */
uint32_t hex2bin(char *hexstr, uint32_t hexlen, uint8_t *result, uint32_t reslen)
{
    uint32_t i, j = 0, k = 0;

    for (i = 0; i < hexlen; i++) {
        uint8_t c = (uint8_t)hexstr[i];

        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'a' && c <= 'f')
            c = (uint8_t)((c - 'a') + 10);
        else if (c >= 'A' && c <= 'F')
            c = (uint8_t)((c - 'A') + 10);
        else if (c == '\0')
            break;
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

/*! \brief convert binary buffer to hex string
 *
 *  @param result pointer to the hex string
 *  @param reslen length of the hex string
 *  @param bin pointer to the binary buffer
 *  @param binlen length of the binary buffer
 *
 *  @return 0 for success and -1 if result buffer is 
 */
int32_t bin2hex(char *result, int32_t reslen, uint8_t *bin, int32_t binlen)
{
    const char *hex = "0123456789ABCDEF";

    char *p;
    int32_t i;

    if (reslen < 2 * binlen)
        return -1;

    p = result;
    for (i = 0; i < binlen; i++) {
        *p++ = hex[bin[i] >> 4 & 0x0F];
        *p++ = hex[bin[i] & 0x0F];
    }
    /* add terminating null char if room in result */
    if (reslen > 2 * binlen)
        *p = '\0';

    return 0;
}

/*! \brief Compare two strings S1 and S2, ignoring case, returning less than,
 *  equal to or greater than zero
 *
 *  @param s1 pointer to first string
 *  @param s2 pointer to second string
 *
 *  @return 0 for equality, less or greater than 0 indicating difference
 */
static int strcasecmp_(const char *s1, const char *s2)
{
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    int result;
    if (p1 == p2)
        return 0;
    if (!p1 || !p2)
        return p1 - p2;
    while ((result = tolower(*p1) - tolower(*p2)) == 0)
        if (*p1++ == '\0' || *p2++ == '\0')
            break;
    return result;
}

/*! \brief read configuration from a file
 *
 *  @param filename pointer to the filename
 *  @param config pointer to structure where config is stored
 *  @param client_id the client id that must match
 *
 *  @return 0 for success and -1 if result buffer is 
 */
int load_config(char *filename, Config_t *config)
{
    char line[MAX_LINE_LENGTH + 1]; /* hold line with terminating null */
    char str[MAX_LINE_LENGTH]; /* temporary space for value */
    char *p;
    int val;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("Error: unable to open config file - %s", filename);
        return -1;
    }
    memset(config, '\0', sizeof(*config));
    config->debounce = 1;
    config->statefile[0] = '\0';

    while (fgets(line, sizeof(line), fp)) {
        if (strlen(line) < 4)
            continue;

        if ((p = strchr(line, '#')) != NULL)
            *p = '\0';

        if (sscanf(line, "SERVER %256s", config->server) == 1) {
            continue;
        }

        if (sscanf(line, "PORT %d", &val) == 1) {
            config->port = (uint16_t)(val & 0xFFFF);
            continue;
        }

        if (sscanf(line, "STATE_FILE %256s", config->statefile) == 1) {
            continue;
        }

        if (sscanf(line, "KEY %32s", str) == 1) {
            hex2bin(str, AES_SIZE * 2, config->key, AES_SIZE);
            continue;
        }

        if (sscanf(line, "PARTNER_ID %d", &val) == 1) {
            config->partner_id = (uint16_t)(val & 0xFFFF);
            continue;
        }

        if (sscanf(line, "DEVICE_ID %32s", str) == 1) {
            config->device_len = strlen(str) / 2;
            hex2bin(str, config->device_len * 2, config->device_id, MAX_DEVICE_ID);
            continue;
        }

        if (sscanf(line, "SKU %32s", str) == 1) {
            strncpy(config->sku, str, sizeof(config->sku));
            continue;
        }
        if (sscanf(line, "CC %d", &val) == 1) {
            config->cc = (uint16_t)(val & 0xFFFF);
            continue;
        }
        if (sscanf(line, "DEBOUNCE %5s", str) == 1) {
            if (strcasecmp_(str, "off") == 0 || strcasecmp_(str, "false") == 0)
                config->debounce = 0;
            continue;
        }
        if (sscanf(line, "UL_APP_DATA %s", str) == 1) {
            config->ul_app_data_len = strlen(str) / 2;
            hex2bin(str, config->ul_app_data_len * 2, config->ul_app_data, config->ul_app_data_len);
            continue;
        }
    }
    config->filename = filename;
    fclose(fp);
    printf("Config Loaded\n");
    return 0;
}

/*! \brief print configuration
 *
 *  @param config pointer to structure where config is stored
 *
 *  @return void
 */
void print_config(Config_t *config)
{
    char key[AES_SIZE * 2 + 1];
    char device[MAX_DEVICE_ID * 2 + 1];
    char ul_app_data[SKY_MAX_DL_APP_DATA * 2 + 1];

    bin2hex(key, AES_SIZE * 2, config->key, AES_SIZE);
    key[AES_SIZE * 2] = '\0';
    bin2hex(device, sizeof(device), config->device_id, config->device_len);
    device[config->device_len * 2] = '\0';
    bin2hex(ul_app_data, sizeof(ul_app_data), config->ul_app_data, config->ul_app_data_len);
    ul_app_data[config->ul_app_data_len * 2] = '\0';

    printf("Configuration file: %s\n", config->filename);
    printf("Server: %s\n", config->server);
    printf("Port: %d\n", config->port);
    printf("Key: %32s\n", key);
    printf("State file: %s\n", config->statefile);
    printf("Partner Id: %d\n", config->partner_id);
    printf("Device: %12s\n", device);
    printf("SKU: %s\n", config->sku);
    printf("CC: %d\n", config->cc);
    printf("Debounce: %s\n", config->debounce ? "true" : "false");
    printf("Uplink data: %s\n", ul_app_data);
}
