/*! \file libelg/utilities.c
 *  \brief utilities - Skyhook ELG API Version 3.0 (IoT)
 *
 * Copyright 2019 Skyhook Inc.
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
#include <stdio.h>
#include <stdarg.h>
#define SKY_LIBELG 1
#include "beacons.h"
#include "config.h"
#include "response.h"
#include "workspace.h"
#include "crc32.h"
#include "libelg.h"

/*! \brief set sky_errno and return sky_status
 *
 *  @param sky_errno sky_errno is the error code
 *  @param code the sky_errno_t code to return
 *
 *  @return sky_status_t SKY_SUCCESS (if code is SKY_ERROR_NONE) or SKY_ERROR
 */
sky_status_t sky_return(sky_errno_t *sky_errno, sky_errno_t code)
{
	if (sky_errno != NULL)
		*sky_errno = code;
	return (code == SKY_ERROR_NONE) ? SKY_SUCCESS : SKY_ERROR;
}

/*! \brief validate the workspace buffer
 *
 *  @param ctx workspace buffer
 *
 *  @return true if workspace is valid, else false
 */
int validate_workspace(sky_ctx_t *ctx)
{
	int i;

	if (ctx != NULL &&
	    ctx->header.crc32 == sky_crc32(&ctx->header.magic,
					   sizeof(ctx->header) -
						   sizeof(ctx->header.crc32))) {
		for (i = 0; i < MAX_BEACONS; i++) {
			if (ctx->beacon[i].h.magic != BEACON_MAGIC ||
			    ctx->beacon[i].h.type >= SKY_BEACON_MAX)
				return false;
		}
	}
	if (ctx == NULL || ctx->len > MAX_BEACONS ||
	    ctx->connected > MAX_BEACONS)
		return false;
	return true;
}

int logfmt(sky_ctx_t *ctx, sky_log_level_t level, const char *fmt, ...)
{
	char buf[96];
	int ret;
	va_list ap;

	if (level > ctx->min_level)
		return 0;
	va_start(ap, fmt);
	ret = vsnprintf(buf, sizeof(buf), fmt, ap);
	(*ctx->logf)(level, buf, sizeof(buf));
	va_end(ap);
	return ret;
}
