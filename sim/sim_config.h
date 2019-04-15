#ifndef SIM_CONFIG_H
#define SIM_CONFIG_H

#define MAC_SIZE 6
#define KEY_SIZE 16

typedef struct client_config {

	uint16_t port;
	uint16_t partner_id;
	uint16_t client_id;
	uint16_t delay;

	char server[80];
	char scan_file[256];

	uint8_t device_mac[MAC_SIZE];
	uint8_t key[KEY_SIZE];
} Config_t;

uint32_t hex2bin(char *hexstr, uint32_t hexlen, uint8_t *result, uint32_t reslen);

int32_t bin2hex(char *buff, int32_t buff_len, uint8_t *data, int32_t data_len);

int load_config(char *filename, Config_t *config, int client_id);

void print_config(Config_t *config);

#endif
