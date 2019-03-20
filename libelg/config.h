/*! \file libelg/config.h
 *  \brief Skyhook ELG API Build time parameters
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
#ifndef SKY_CONFIG_H
#define SKY_CONFIG_H

/*! \brief The maximum number of beacons passed to the ELG server in a request
 */
#define MAX_BEACONS 11

/*! \brief The percentage of beacons that must match in a cached scan/location
 */
#define CACHE_MATCH_THRESHOLD 70

/*! \brief The maximum age (in hr) that a cached value is concidered useful
 */
#define CACHE_AGE_THRESHOLD 24

/*! \brief The number of entries in the scan/response cache
 */
#define CACHE_SIZE 1

#endif
