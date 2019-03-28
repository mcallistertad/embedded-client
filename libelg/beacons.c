/*! \file libelg/beacons.c
 *  \brief utilities - Skyhook ELG API Version 3.0 (IoT)
 *
 * Copyright 2015-2019 Skyhook Inc.
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
#include <stdbool.h>
#include <string.h>
#include <time.h>
#define SKY_LIBELG 1
#include "config.h"
#include "response.h"
#include "beacons.h"
#include "workspace.h"
#include "crc32.h"
#include "libelg.h"
#include "utilities.h"

/*! \brief test two MAC addresses for being virtual aps
 *
 *  @param macA pointer to the first MAC
 *  @param macB pointer to the second MAC
 *
 *  @return true or false
 */
static int similar(uint8_t macA[], uint8_t macB[])
{
	/* Return 1 (true) if OUIs are identical and no more than 1 hex digits
     * differ between the two MACs. Else return 0 (false).
     */
	if (memcmp(macA, macB, 3) != 0)
		return false;

	size_t num_diff = 0; // Num hex digits which differ
	size_t i;

	for (i = 3; i < MAC_SIZE; i++) {
		if (((macA[i] & 0xF0) != (macB[i] & 0xF0) && ++num_diff > 1) ||
		    ((macA[i] & 0x0F) != (macB[i] & 0x0F) && ++num_diff > 1))
			return false;
	}

	return true;
}

/*! \brief shuffle list to remove the beacon at index
 *
 *  @param ctx Skyhook request context
 *  @param int index 0 based index of AP to remove
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
static sky_status_t remove_beacon(sky_ctx_t *ctx, size_t index)
{
	if (index >= ctx->len)
		return SKY_ERROR;
	memmove(&ctx->beacon[index], &ctx->beacon[index + 1],
		sizeof(beacon_t) * (ctx->len - index - 1));
	ctx->len -= 1;
	return SKY_SUCCESS;
}

/*! \brief insert beacon in list based on type and AP rssi
 *
 *  @param ctx Skyhook request context
 *  @param sky_errno pointer to errno
 *  @param b beacon to add
 *  @param index pointer where to save the insert position
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
static sky_status_t insert_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno,
				  beacon_t *b, int *index)
{
	int i;

	/* sanity checks */
	if (!validate_workspace(ctx) || b->h.magic != BEACON_MAGIC ||
	    b->h.type > SKY_BEACON_MAX)
		return sky_return(sky_errno, SKY_ERROR_BAD_PARAMETERS);

	/* find correct position to insert based on type */
	for (i = 0; i < ctx->len; i++)
		if (ctx->beacon[i].h.type >= b->h.type)
			break;
	/* add beacon at the end */
	if (i == ctx->len) {
		ctx->beacon[i] = *b;
		ctx->len++;
	} else {
		/* if AP, add in rssi order */
		if (b->h.type == SKY_BEACON_AP) {
			for (; i < ctx->len; i++)
				if (ctx->beacon[i].h.type != SKY_BEACON_AP ||
				    ctx->beacon[i].ap.rssi > b->ap.rssi)
					break;
		}
		/* shift beacons to make room for the new one */
		memmove(&ctx->beacon[i + 1], &ctx->beacon[i],
			sizeof(beacon_t) * (ctx->len - i));
		ctx->beacon[i] = *b;
		ctx->len++;
	}
	if (index != NULL)
		*index = i;
	return SKY_SUCCESS;
}

/*! \brief try to reduce AP by filtering out virtual AP
 *
 *  @param ctx Skyhook request context
 *  @param int index 0 based index of last AP added
 *
 *  @return true if AP removed, or false
 */
static sky_status_t filter_virtual_aps(sky_ctx_t *ctx, size_t index)
{
	int low, high, i;

	if (index < 0 || index > ctx->ap_len)
		return false;

	/* look for any AP beacon that is 'similar' to the one just added */
	if (ctx->beacon[index].h.type != SKY_BEACON_AP)
		return false;

	/* find first AP, and last AP in the list */
	for (low = index; low >= 0 && ctx->beacon[low].h.type == SKY_BEACON_AP;
	     low--)
		;
	for (high = index;
	     high < ctx->ap_len && ctx->beacon[high].h.type == SKY_BEACON_AP;
	     high++)
		;
	for (i = low; i <= high; i++) {
		if (i == index)
			continue;
		if (similar(ctx->beacon[index].ap.mac, ctx->beacon[i].ap.mac)) {
			remove_beacon(ctx, i);
			return true;
		}
	}
	return false;
}

/*! \brief add beacon to list
 *  if beacon is AP, filter
 *
 *  @param ctx Skyhook request context
 *  @param beacon pointer to new beacon
 *
 *  @return SKY_SUCCESS if beacon successfully added or SKY_ERROR
 */
sky_status_t add_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno, beacon_t *b)
{
	int i = -1;

	/* check if maximum number of non-AP beacons already added */
	if (b->h.type == SKY_BEACON_AP &&
	    ctx->len - ctx->ap_len > (MAX_BEACONS - MAX_AP_BEACONS))
		return sky_return(sky_errno, SKY_ERROR_TOO_MANY);

	/* insert the beacon */
	if (insert_beacon(ctx, sky_errno, b, &i) == SKY_ERROR)
		return SKY_ERROR;

	/* done if no filtering needed */
	if (b->h.type != SKY_BEACON_AP || ctx->ap_len <= MAX_AP_BEACONS)
		return sky_return(sky_errno, SKY_ERROR_NONE);

	/* beacon is AP and need filter */
	filter_virtual_aps(ctx, i);

	return sky_return(sky_errno, SKY_ERROR_NONE);
}
#if 0
int rssi_cmp(const void *a, const void *b)
{
	// Sort on RSSI, low-to-high.
	int8_t rssiA = ((struct ap_t *)a)->rssi;
	int8_t rssiB = ((struct ap_t *)b)->rssi;

	return rssiA < rssiB ? -1 : rssiA > rssiB;
}

int similar(uint8_t macA[], uint8_t macB[])
{
	// Return 1 (true) if OUIs are identical and no more than 1 hex digits
	// differ between the two MACs. Else return 0 (false).
	//
	if (memcmp(macA, macB, 3) != 0)
		return 0;

	size_t num_diff = 0; // Num hex digits which differ
	size_t i;

	for (i = 3; i < MAC_SIZE; i++) {
		if (((macA[i] & 0xF0) != (macB[i] & 0xF0) && ++num_diff > 1) ||
		    ((macA[i] & 0x0F) != (macB[i] & 0x0F) && ++num_diff > 1))
			return 0;
	}

	return 1;
}

size_t filter_virtual_aps(struct ap_t aps[], size_t num_aps, size_t min_aps)
{
	// If two APs are "similar" (i.e., not more than 1 of their MAC hex digits
	// differ), the one with the higher MAC value is assumed to be "virtual"
	// relative to the other one. Remove this "virtual" AP if the number of APs
	// is still above the min_aps threshold.
	//
	// group_id[] maps aps array index to virtual group ID. All APs within a
	// given "group" have "similar" MACs. I.e., all of their MAC addresses are
	// virtual w.r.t. one another.
	//
	uint8_t group_id[MAX_APS];

	// Initialize group_ids of all APs to zero (meaning "not yet assigned to a
	// group").
	memset(group_id, 0, num_aps * sizeof(*group_id));

	int i, j;

	int num_groups = 0;

	// Collect APs together into virtual groups (i.e., create the AP
	// index-to-group-id mapping) by comparing all pairs of APs.
	for (i = 0; i < num_aps; i++) {
		if (group_id[i] == 0)
			// Hey, a new group!
			group_id[i] = ++num_groups;

		for (j = i + 1; j < num_aps; j++)
			if (group_id[j] == 0)
				if (similar(aps[i].MAC, aps[j].MAC))
					group_id[j] = group_id[i];
	}

	// Collapse the virtual groups into a single AP per group. The single
	// surviving AP within a group is the one with the lowest MAC address. The
	// other APs within the group are marked for garbage collection.
	//
	// remove_it[] maps AP index to flag indicating whether or not that AP
	// should be deleted.
	//
	uint8_t remove_it[MAX_APS];

	memset(remove_it, 0, num_aps * sizeof(*remove_it));

	size_t max_to_remove = 0;

	if (num_aps > min_aps)
		max_to_remove = num_aps - min_aps;

	size_t num_removed = 0;

	for (i = 0; i < num_aps - 1; i++) {
		if (!remove_it[i]) {
			for (j = i + 1; j < num_aps; j++) {
				if (!remove_it[j] &&
				    group_id[i] == group_id[j]) {
					// Keep the AP with the lower MAC address value.
					if (num_removed < max_to_remove) {
						num_removed++;

						if (memcmp(aps[i].MAC,
							   aps[j].MAC,
							   MAC_SIZE) < 0) {
							remove_it[j] = 1;
						} else {
							remove_it[i] = 1;
							break;
						}
					}
				}
			}
		}
	}

	// Garbage collect.
	for (i = num_aps - 1; i >= 0; i--)
		if (remove_it[i])
			remove_ap(aps, num_aps--, i);

	return num_aps;
}

size_t down_select_by_rssi(struct ap_t aps[], size_t num_aps, size_t max_aps)
{
	// Reduce the number of scanned APs (which will be sent to Skyhook) to
	// max_aps. Choose them to maximize RSSI diversity. Always include
	// the one with the lowest RSSI, and the one with the highest RSSI.
	//
	if (max_aps == 1) {
		// In this special case, return the AP with the biggest RSSI value (the
		// last entry).
		//
		memcpy(&aps[0], &aps[num_aps - 1], sizeof(struct ap_t));

		return 1;
	} else if (max_aps == 2) {
		// In this special case, return the AP with the smallest RSSI value
		// (the first entry), and the AP with the biggest RSSI value (the last
		// entry).
		//
		memcpy(&aps[1], &aps[num_aps - 1], sizeof(struct ap_t));

		return 2;
	}

	size_t aps_per_band = (num_aps - 2) / (max_aps - 2);

	size_t i, n = 1;

	for (i = 1; i < num_aps - 1 && n < max_aps - 1;
	     i += aps_per_band, n++) {
		size_t j = i + aps_per_band / 2;

		if (j != n)
			memcpy(&aps[n], &aps[j], sizeof(struct ap_t));
	}

	memcpy(&aps[max_aps - 1], &aps[num_aps - 1], sizeof(struct ap_t));

	return max_aps;
}

size_t filter_and_reduce_aps(struct ap_t aps[], size_t num_aps, size_t max_aps)
{
	if (num_aps <= max_aps)
		return num_aps;

	// Sort by RSSI, low-to-high.
	qsort(aps, num_aps, sizeof(struct ap_t), rssi_cmp);

	num_aps = filter_virtual_aps(aps, num_aps, max_aps);

#if defined UNIT_TEST
	printf("--- virtual filtered (%zu):\n", num_aps);

	size_t i;

	for (i = 0; i < num_aps; i++) {
		printf("%3d: ", aps[i].rssi);
		print_buff(aps[i].MAC, MAC_SIZE);
	}
#endif

	num_aps = down_select_by_rssi(aps, num_aps, max_aps);

#if defined UNIT_TEST
	printf("--- down-selected (%zu):\n", num_aps);

	for (i = 0; i < num_aps; i++) {
		printf("%3d: ", aps[i].rssi);
		print_buff(aps[i].MAC, MAC_SIZE);
	}
#endif

	return num_aps;
}

#if defined UNIT_TEST
// gcc -g -DUNIT_TEST -I../../common/inc/ ../../common/src/utilities/sky_util.c ap_filter.c

void sky_set_ap(struct ap_t *ap, int8_t rssi, uint8_t *mac, bool is_connected,
		enum SKY_BAND band)
{
	ap->rssi = rssi;
	strncpy((char *)ap->MAC, (const char *)mac, MAC_SIZE);
}

void hex_str_to_bin(const char *hex_str, uint8_t bin_buff[], size_t buff_len)
{
	const char *pos = hex_str;
	size_t i;

	for (i = 0; i < buff_len; i++) {
		sscanf(pos, "%2hhx", &bin_buff[i]);
		pos += 2;
	}
}

int main(int argc, char **argv)
{
	struct ap_t aps[100];
	uint8_t num_aps = 0;
	uint8_t mac[MAC_SIZE];

	{
		hex_str_to_bin("881C413B9430", mac, MAC_SIZE);
		sky_set_ap(&aps[num_aps++], -23, mac, false, BAND_5G);
	}
	{
		hex_str_to_bin("881C413B9431", mac, MAC_SIZE);
		sky_set_ap(&aps[num_aps++], -24, mac, false, BAND_5G);
	}
	{
		hex_str_to_bin("111C413B942D", mac, MAC_SIZE);
		sky_set_ap(&aps[num_aps++], -25, mac, false, BAND_5G);
	}
	{
		hex_str_to_bin("E01C413BDF29", mac, MAC_SIZE);
		sky_set_ap(&aps[num_aps++], 26, mac, false, BAND_5G);
	}
	{
		hex_str_to_bin("771C411B942C", mac, MAC_SIZE);
		sky_set_ap(&aps[num_aps++], -27, mac, false, BAND_5G);
	}
	{
		hex_str_to_bin("881C413B9432", mac, MAC_SIZE);
		sky_set_ap(&aps[num_aps++], -28, mac, false, BAND_5G);
	}

	size_t max_aps = 5;

	size_t i;

	printf("filter threshold=%zu\n", max_aps);

	printf("--- scanned (%d):\n", num_aps);

	for (i = 0; i < num_aps; i++) {
		printf("%3d: ", aps[i].rssi);
		print_buff(aps[i].MAC, MAC_SIZE);
	}

	num_aps = filter_and_reduce_aps(aps, num_aps, max_aps);
}
#endif
#endif
