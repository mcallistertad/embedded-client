#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#define __USE_XOPEN
#include <sys/time.h>
#include "beacons.h"

#include "sim_beacons.h"

WifiScan_t scans[MAX_SCANS];

int load_beacons(char *filename)
{
	char buf[100000];
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		printf("Error: unable to open config file - %s", filename);
		return -1;
	}

	int scan_count = 0;
	json_object *obj;
	while (fgets(buf, sizeof(buf), fp)) {
		obj = json_tokener_parse(buf);
		struct json_object *json_aps = json_object_object_get(obj, "aps");
		if (json_aps != NULL) {
			int num_aps = json_object_array_length(json_aps);
			scans[scan_count].num_aps = num_aps;
			scans[scan_count].wifi_type = SKY_BEACON_AP;
			scans[scan_count].wifi.aps = malloc(sizeof(struct ap) * num_aps);
			aps_to_beacons(json_aps, scans[scan_count].wifi.aps, num_aps);
		}

		struct json_object *obj_cell = json_object_object_get(obj, "cell");
		if (obj_cell != NULL) {
			cell_to_beacon(obj_cell, &scans[scan_count]);
		}
		scan_count++;
		if (scan_count == 2)
			break;

	}
	fclose(fp);
	printf("Total scans loaded: %d\n", scan_count);
	return 0;
}

void aps_to_beacons(struct json_object *json_aps, struct ap *aps, int num) {
	for (int i=0; i<num; i++) {
		json_object *obj = json_object_array_get_idx(json_aps, i);
		char *mac = (char *) json_object_get_string(json_object_object_get(obj, "mac"));
		char str[2];
		int str_idx = 0;
		for (int m=0; m<MAC_SIZE; m++) {
			strncpy(str, &mac[str_idx], 2);
			aps[i].mac[m] = strtoul(str, NULL, 16);
			str_idx += 2;
		}
		aps[i].magic = BEACON_MAGIC;
		aps[i].type = SKY_BEACON_AP;
		aps[i].age = convert_timestamp((char *) json_object_get_string(json_object_object_get(obj, "timestamp")));
		aps[i].channel = (uint32_t) json_object_get_int(json_object_object_get(obj, "channel"));
		aps[i].rssi = (int8_t) json_object_get_int(json_object_object_get(obj, "rssi"));
		aps[i].flag = 1;
	}
	print_aps(aps);
}

void cell_to_beacon(struct json_object *obj, WifiScan_t *scan) {
	scan->cell_type = determine_cell_type((char *) json_object_get_string(json_object_object_get(obj, "type")));
	switch (scan->cell_type) {
		case SKY_BEACON_GSM:
			scan->cell.gsm.mcc = (uint16_t) json_object_get_int(json_object_object_get(obj, "mcc"));
			scan->cell.gsm.mnc = (uint16_t) json_object_get_int(json_object_object_get(obj, "mnc"));
			scan->cell.gsm.lac = (uint16_t) json_object_get_int(json_object_object_get(obj, "lac"));
			scan->cell.gsm.ci = (uint32_t) json_object_get_int(json_object_object_get(obj, "ci"));
			scan->cell.gsm.age = convert_timestamp((char *) json_object_get_string(json_object_object_get(obj, "timestamp")));
			scan->cell.gsm.rssi = (int16_t) json_object_get_int(json_object_object_get(obj, "rssi"));
			scan->cell.gsm.magic = BEACON_MAGIC;
			scan->cell.gsm.type = scan->cell_type;
			break;
		case SKY_BEACON_UMTS:
			scan->cell.umts.mcc = (uint16_t) json_object_get_int(json_object_object_get(obj, "mcc"));
			scan->cell.umts.mnc = (uint16_t) json_object_get_int(json_object_object_get(obj, "mnc"));
			scan->cell.umts.lac = (uint16_t) json_object_get_int(json_object_object_get(obj, "lac"));
			scan->cell.umts.ci = (uint32_t) json_object_get_int(json_object_object_get(obj, "ci"));
			scan->cell.umts.age = convert_timestamp((char *) json_object_get_string(json_object_object_get(obj, "timestamp")));
			scan->cell.umts.rssi = (int16_t) json_object_get_int(json_object_object_get(obj, "rssi"));
			scan->cell.umts.magic = BEACON_MAGIC;
			scan->cell.umts.type = scan->cell_type;
			break;
		case SKY_BEACON_LTE:
			scan->cell.lte.mcc = (uint16_t) json_object_get_int(json_object_object_get(obj, "mcc"));
			scan->cell.lte.mnc = (uint16_t) json_object_get_int(json_object_object_get(obj, "mcc"));
			scan->cell.lte.eucid = (uint32_t) json_object_get_int(json_object_object_get(obj, "eucid"));
			scan->cell.lte.age = convert_timestamp((char *) json_object_get_string(json_object_object_get(obj, "timestamp")));
			scan->cell.lte.rssi = (int16_t) json_object_get_int(json_object_object_get(obj, "rssi"));
			scan->cell.lte.magic = BEACON_MAGIC;
			scan->cell.lte.type = scan->cell_type;
			break;
		case SKY_BEACON_CDMA:
			scan->cell.cdma.sid = (uint16_t) json_object_get_int(json_object_object_get(obj, "sid"));
			scan->cell.cdma.nid = (uint16_t) json_object_get_int(json_object_object_get(obj, "nid"));
			scan->cell.cdma.bsid = (uint16_t) json_object_get_int(json_object_object_get(obj, "bsid"));
			scan->cell.cdma.age = convert_timestamp((char *) json_object_get_string(json_object_object_get(obj, "timestamp")));
			scan->cell.cdma.rssi = (int16_t) json_object_get_int(json_object_object_get(obj, "rssi"));
			scan->cell.cdma.magic = BEACON_MAGIC;
			scan->cell.cdma.type = scan->cell_type;
			break;
		case SKY_BEACON_NBIOT:
			scan->cell.nbiot.mcc = (uint16_t) json_object_get_int(json_object_object_get(obj, "mcc"));
			scan->cell.nbiot.mnc = (uint16_t) json_object_get_int(json_object_object_get(obj, "mnc"));
			scan->cell.nbiot.tac = (uint16_t) json_object_get_int(json_object_object_get(obj, "tac"));
			scan->cell.nbiot.e_cellid = (uint32_t) json_object_get_int(json_object_object_get(obj, "e_cellid"));
			scan->cell.nbiot.age = convert_timestamp((char *) json_object_get_string(json_object_object_get(obj, "timestamp")));
			scan->cell.nbiot.rssi = (int16_t) json_object_get_int(json_object_object_get(obj, "rssi"));
			scan->cell.nbiot.magic = BEACON_MAGIC;
			scan->cell.nbiot.type = scan->cell_type;
			break;

		default:
			printf("Cell not added.\n");
			return;
	}

	print_cell(scan);
}

void gps_to_beacon(struct json_object *json_gps, Gps_t *gps) {
	struct json_object *obj = json_object_object_get(json_gps, "cellScan");
	gps->lat = json_object_get_double(json_object_object_get(obj, "latitude"));
	gps->lon = json_object_get_double(json_object_object_get(obj, "longitude"));
	gps->hdop = 0;
	gps->alt = (float) json_object_get_double(json_object_object_get(obj, "longitude"));
	gps->hpe = (float) json_object_get_double(json_object_object_get(obj, "accuracy"));
	gps->speed = (float) json_object_get_double(json_object_object_get(obj, "speed"));
	gps->nsat = (uint8_t) json_object_get_int(json_object_object_get(obj, "nsat"));
	gps->fix = 1;
	gps->age = convert_timestamp((char *) json_object_get_string(json_object_object_get(obj, "timestamp")));
}

time_t convert_timestamp(char *ts) {
	char newts[10];
	strncpy(newts, ts, 10);
	return strtoul(newts, NULL, 10);
}

Sky_beacon_type_t determine_cell_type(char *ctype) {
	char *upper = ctype;
	while (*upper) {
	    *upper = toupper((unsigned char) *upper);
	    upper++;
	}
	if (strcmp(ctype, "GSM") == 0)
		return SKY_BEACON_GSM;
	else if (strcmp(ctype, "UMTS") == 0)
		return SKY_BEACON_UMTS;
	else if (strcmp(ctype, "LTE") == 0)
		return SKY_BEACON_LTE;
	else if (strcmp(ctype, "CDMA") == 0)
		return SKY_BEACON_CDMA;
	else if (strcmp(ctype, "NBIOT") == 0)
		return SKY_BEACON_NBIOT;
	else {
		printf("Error - Bad cell type found: (%s)\n", ctype);
	}
	return 0;

}

void print_aps(struct ap aps[]) {
	for (int i = 0; i < sizeof(aps); i++) {
		printf("AP #%d - type: %d, mac: %02X:%02X:%02X:%02X:%02X:%02X, chan: %u, rssi: %d, time: %u, flag: %d\n",
		       i, aps[i].type, aps[i].mac[0], aps[i].mac[1], aps[i].mac[2],
			   aps[i].mac[3], aps[i].mac[4], aps[i].mac[5],
			   aps[i].channel, aps[i].rssi, aps[i].age, aps[i].flag);
	}
}

void print_cell(WifiScan_t *scan) {
	switch (scan->cell_type) {
		case SKY_BEACON_GSM:
			printf("Cell - type: GSM, mcc: %d, mnc: %d, lac: %d, ci: %d, rssi: %d, time: %d\n",
					scan->cell.gsm.mcc, scan->cell.gsm.mnc, scan->cell.gsm.lac,
					scan->cell.gsm.ci, scan->cell.gsm.rssi, scan->cell.gsm.age);
			break;
		case SKY_BEACON_UMTS:
			printf("Cell - type: UMTS, mcc: %d, mnc: %d, lac: %d, ci: %d, rssi: %d, time: %d\n",
					scan->cell.umts.mcc, scan->cell.umts.mnc, scan->cell.umts.lac,
					scan->cell.umts.ci, scan->cell.umts.rssi, scan->cell.umts.age);
			break;
		case SKY_BEACON_LTE:
			printf("Cell - type: LTE, mcc: %d, mnc: %d, eucid: %d, rssi: %d, time: %d\n",
					scan->cell.lte.mcc, scan->cell.lte.mnc, scan->cell.lte.eucid,
					scan->cell.lte.rssi, scan->cell.lte.age);
			break;
		case SKY_BEACON_CDMA:
			printf("Cell - type: CDMA, sid: %d, nid: %d, bsid: %d, rssi: %d, time: %d\n",
					scan->cell.cdma.sid, scan->cell.cdma.nid, scan->cell.cdma.bsid, scan->cell.cdma.rssi, scan->cell.cdma.age);
			break;
		case SKY_BEACON_NBIOT:
			printf("Cell - type: NBIOT, mcc: %d, mnc: %d, tac: %d, e_cellid: %d, rssi: %d, time: %d\n",
					scan->cell.nbiot.mcc, scan->cell.nbiot.mnc, scan->cell.nbiot.tac,
					scan->cell.nbiot.e_cellid, scan->cell.nbiot.rssi, scan->cell.nbiot.age);
			break;
		default:
			printf("Error: Bad cell type: %d", scan->cell_type);
	}

}

void get_next_ap_scan(struct wifi_scan *scan) {
	memcpy (scan, &scans[0], sizeof(WifiScan_t));
}
