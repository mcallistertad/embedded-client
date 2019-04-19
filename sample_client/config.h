/*! \file sample_client/config.h
 *  \brief Skyhook Embedded Library
 *
 * Copyright 2015-present Skyhook Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

#include "../libel/config.h"

typedef struct client_config {
    char *filename;
    uint16_t port;
    uint16_t partner_id;
    uint16_t client_id;
    uint16_t delay;

    char server[80];
    char scan_file[256];

    uint8_t device_mac[MAC_SIZE];
    uint8_t key[AES_SIZE];
} Config_t;

uint32_t hex2bin(
    char *hexstr, uint32_t hexlen, uint8_t *result, uint32_t reslen);

int32_t bin2hex(char *result, int32_t reslen, uint8_t *bin, int32_t binlen);

int load_config(char *filename, Config_t *config, int client_id);

void print_config(Config_t *config);

#endif
