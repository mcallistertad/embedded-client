#ifndef SIM_BEACONS_H
#define SIM_BEACONS_H

#include "json.h"

#define MAX_SCANS 1000

typedef struct wifi_scan {
	int8_t num_aps;
	Sky_beacon_type_t wifi_type;
	Sky_beacon_type_t cell_type;
	union Wifi {
		struct ap *aps;
		struct ble *ble;
	} wifi;
	union Cell {
		struct cdma cdma;
		struct gsm gsm;
		struct lte lte;
		struct nbiot nbiot;
		struct umts umts;
	} cell;
	Gps_t gps;
} WifiScan_t;


int load_beacons(char *filename);

void aps_to_beacons(struct json_object *json_aps, struct ap *aps, int num);

void cell_to_beacon(struct json_object *obj, WifiScan_t *scan);

void gps_to_beacon(struct json_object *json_gps, Gps_t *gps);

time_t convert_timestamp(char *timestr);

Sky_beacon_type_t determine_cell_type(char *cell);

void print_aps(struct ap aps[]);

void print_cell(WifiScan_t *scan);

void get_next_ap_scan(WifiScan_t *scan);

#endif
