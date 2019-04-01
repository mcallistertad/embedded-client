/*! \file libelg/utilities.h
 *  \brief Skyhook ELG API workspace structures
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
#ifndef SKY_UTILITIES_H
#define SKY_UTILITIES_H

sky_status_t sky_return(sky_errno_t *sky_errno, sky_errno_t code);
int validate_workspace(sky_ctx_t *ctx);
sky_status_t add_beacon(sky_ctx_t *ctx, sky_errno_t *sky_errno, beacon_t *b,
			bool is_connected);

#endif
