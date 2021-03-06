# Skyhook Embedded Client Library

## Description

The Skyhook Embedded Client is a small library written in C. It is intended to be included in embedded targets (e.g.,
IoT devices) to allow those devices to use the Skyhook Precision Location service in order to obtain an estimate of the
geolocation of the device on which it runs. It is known as the 'embedded library' or simply 'LibEL'. This repo also
includes a sample client application which illustrates how the library can be used by your application (see below).

Instructions for cloning and building the library are below.

## Change Log

### [Release 5.1.0](https://github.com/SkyhookWireless/embedded-client/tree/5.0.0)

* Cache performance has been improved
* Library configuration includes the ability to configure larger cache sizes

### [Release 5.0.0](https://github.com/SkyhookWireless/embedded-client/tree/5.0.0)

* Library is more compact and can be configured to include only the required features.
* sky_add_cell_gsm_beacon() allows bsic and arfcn to be included in the beacon info.
* sky_add_cell_gsm_neighbor_beacon() has been added to the API
* When porting application code from a previous release of the library, any calls to sky_add_cell_gsm_beacon must be
  modified to include bsic and arfcn data if available on your device, or SKY_UNKNOWN_ID5 and SKY_UNKNOWN_ID6 respectively.
* Maximum time that service will be blocked due to TBR authorization failure is now 24 hours.
* All time comparisons use the function difftime(), improving portability of the library.
* Bug fixes from hotfix releases 3.0.3 and 3.0.4 have been incorporated

### [Release 4.0.1](https://github.com/SkyhookWireless/embedded-client/tree/4.0.1)

* Bug Fix - Fix build with no cache i.e. CACHE_SIZE=0
* Document Location Source SKY_LOCATION_SOURCE_RFPM

### [Release 4.0.0](https://github.com/SkyhookWireless/embedded-client/tree/4.0.0)

#### API simplifications.

* LibEL accepts timestamps with value 0 which indicates that time has not been synchronized.
* LibEL no longer holds state and cache in a static buffer. User allocates both request context buffer and session
  context buffer.
* User may call new API call, sky_search_cache(), to determine whether the current request has a match in the cache.
* User may tune some configuration parameters at runtime using sky_set_option().

#### Code size optimizations.

* Common code sequences have been factored out.
* Some of the largest functions have been re-written.

### [Release 3.0.4](https://github.com/SkyhookWireless/embedded-client/tree/3.0.4)

* Cache miss when request context buffer has gnss but cache does not
* sample_client improvements

### [Release 3.0.3](https://github.com/SkyhookWireless/embedded-client/tree/3.0.3)

* Remove strict check so that a GNSS fix with NSAT value of 0 can be added.

### [Release 3.0.2](https://github.com/SkyhookWireless/embedded-client/tree/3.0.2)

* Bug fix - server side tuning of cache match thresholds was being ignored by LibEL
* Bug fix - low AP count in scan should require exact cache match
* Bug fix - scan with gnss should never match a cache entry without gnss

### [Release 3.0.1](https://github.com/SkyhookWireless/embedded-client/tree/3.0.1)

* Added support for Token Based Registration and authentication.
* Added support for Uplink and downlink data. Downlink data can be configured through a new web service/API.
* Added 'debounce' parameter to sky_open() which allows the option, in the case of a cache match, to report the cached
  information in a subsequent server request.
* Added plugin modules infrastructure, allowing basic or premium algorithms for filtering scans and interpreting server
  responses, and ability to expand LibEL's capabilities in the future.
* Cellular beacons now allow Timing Advance values to be added.
* Updated the sample_client application.
* Renamed .submodules to submodules.
* Additional bug fixes and improvements.

### [Release 2.1.3](https://github.com/SkyhookWireless/embedded-client/tree/2.1.3)

* Change default configuration to 20 total beacons, 15 APs
* Update to allow ID3 to be -1 SKY_UNKNOWN_ID3
* Bug fix - cache match fails if serving cell has changed
* Bug fix - copy actual number of beacons in request context to cache
* Bug fix - sky_add_cell_lte_beacon(), tac must be signed and wider for SKY_UNKNOWN_ID3

### [Release 2.0.0](https://github.com/SkyhookWireless/embedded-client/tree/2.0.0)

* Additional arguments provided to sky_add_cell_lte_beacon(), sky_add_cell_umts_beacon()
  and sky_add_cell_nb_iot_beacon() to allow the user to optionally provide pci/psc/ncid and uarfcn/earfcn values.
* Support for optionally adding scanned neighbor cells (for LTE, UMTS, and NB_IOT cell types) via 3 new functions
  sky_add_cell_lte_neighbor_beacon(), sky_add_cell_umts_neighbor_beacon() and sky_add_cell_nb_iot_neighbor_beacon.

### [Release 1.1.4](https://github.com/SkyhookWireless/embedded-client/tree/1.1.4)

* Device ID length fix and a few changes to sample_client to allow full length of device_id to be used.

### [Release 1.1.3](https://github.com/SkyhookWireless/embedded-client/tree/1.1.3)

* Update to avoid ignoring server results in case no local clock is available, plus logging and other minor cleanups

### [Release 1.1.1](https://github.com/SkyhookWireless/embedded-client/tree/1.1.1)

* Initial public release

## Table of Contents

* [Skyhook Embedded Client Library](#skyhook-embedded-client-library)
    * [Building](#building)
        * [Dependencies](#dependencies)
            * [Git Submodules](#git-submodules)
        * [Library Source Files](#library-source-files)
        * [Build Directions](#build-directions)
    * [API Guide](#api-guide)
        * [Summary](#summary)
        * [Requirements](#requirements)
        * [API conventions](#api-conventions)
        * [Client Configuration](#client-configuration)
        * [General Sequence of Operations](#general-sequence-of-operations)
        * [API Reference](#api-reference)
            * [sky_open() - Initialize Skyhook library and verify access to resources](#sky_open---initialize-skyhook-library-and-verify-access-to-resources)
            * [sky_sizeof_session_ctx() - Get the size of the non-volatile memory required to save and restore the library state.](#sky_sizeof_session_ctx---get-the-size-of-the-non-volatile-memory-required-to-save-and-restore-the-library-state)
            * [sky_sizeof_request_ctx() - Determines the size of the work space required to process the request](#sky_sizeof_request_ctx---determines-the-size-of-the-work-space-required-to-process-the-request)
            * [sky_new_request() - Initializes request context for a new request](#sky_new_request---initializes-request-context-for-a-new-request)
            * [sky_add_ap_beacon() - Add a Wi-Fi beacon to request context](#sky_add_ap_beacon---add-a-wi-fi-beacon-to-request-context)
            * [sky_add_cell_lte_beacon() - Add an lte or lte-CatM1 cell beacon to request context](#sky_add_cell_lte_beacon---add-an-lte-or-lte-catm1-cell-beacon-to-request-context)
            * [sky_add_cell_lte_neighbor_beacon() - Adds an LTE neighbor cell beacon to the request context](#sky_add_cell_lte_neighbor_beacon---adds-an-lte-neighbor-cell-beacon-to-the-request-context)
            * [sky_add_cell_gsm_beacon() - Adds a GSM cell beacon to the request context](#sky_add_cell_gsm_beacon---adds-a-gsm-cell-beacon-to-the-request-context)
            * [sky_add_cell_gsm_neighbor_beacon() - Adds an GSM neighbor cell beacon to the request context](#sky_add_cell_gsm_neighbor_beacon---adds-an-gsm-neighbor-cell-beacon-to-the-request-context)
            * [sky_add_cell_umts_beacon() - Adds a umts cell beacon to the request context](#sky_add_cell_umts_beacon---adds-a-umts-cell-beacon-to-the-request-context)
            * [sky_add_cell_umts_neighbor_beacon() - Adds a umts neighbor cell beacon to the request context](#sky_add_cell_umts_neighbor_beacon---adds-a-umts-neighbor-cell-beacon-to-the-request-context)
            * [sky_add_cell_cdma_beacon() - Adds a cdma cell beacon to the request context](#sky_add_cell_cdma_beacon---adds-a-cdma-cell-beacon-to-the-request-context)
            * [sky_add_cell_nb_iot_beacon() - Adds a nb_iot cell beacon to the request context](#sky_add_cell_nb_iot_beacon---adds-a-nb_iot-cell-beacon-to-the-request-context)
            * [sky_add_cell_nb_iot_neighbor_beacon() - Adds a neighbor nb_iot cell beacon to the request context](#sky_add_cell_nb_iot_neighbor_beacon---adds-a-neighbor-nb_iot-cell-beacon-to-the-request-context)
            * [sky_add_cell_nr_beacon() - Adds a nr cell beacon to the request context](#sky_add_cell_nr_beacon---adds-a-nr-cell-beacon-to-the-request-context)
            * [sky_add_cell_nr_neighbor_beacon() - Adds a neighbor nr cell beacon to the request context](#sky_add_cell_nr_neighbor_beacon---adds-a-neighbor-nr-cell-beacon-to-the-request-context)
            * [sky_add_gnss() - Adds the position of the device from GNSS (GPS, GLONASS, or others) to the request context](#sky_add_gnss---adds-the-position-of-the-device-from-gnss-gps-glonass-or-others-to-the-request-context)
            * [sky_search_cache()  - compares the new request beacons with those in the cache](#sky_search_cache---compares-the-new-request-beacons-with-those-in-the-cache)
            * [sky_ignore_cache_hit()  - allows the result of sky_search_cache() to be overridden](#sky_ignore_cache_hit---allows-the-result-of-sky_search_cache-to-be-overridden)
            * [sky_sizeof_request_buf()  - determines the size of the network request buffer which must be provided by the user](#sky_sizeof_request_buf----determines-the-size-of-the-network-request-buffer-which-must-be-provided-by-the-user)
            * [sky_encode_request() - generate a Skyhook request from the request context](#sky_encode_request---generate-a-skyhook-request-from-the-request-context)
            * [sky_decode_response() - decodes a Skyhook server response](#sky_decode_response---decodes-a-skyhook-server-response)
            * [sky_get_option() - query the value of a configuration parameter](#sky_get_option---query-the-value-of-a-configuration-parameter)
            * [sky_set_option() - set the value of a configuration parameter](#sky_set_option---set-the-value-of-a-configuration-parameter)
            * [sky_perror() - returns a string which describes the meaning of sky_errno codes](#sky_perror---returns-a-string-which-describes-the-meaning-of-sky_errno-codes)
            * [sky_pbeacon() - returns a string which describes the type of a beacon](#sky_pbeacon---returns-a-string-which-describes-the-type-of-a-beacon)
            * [sky_pserver_status() - returns a string which describes the meaning of status codes](#sky_pserver_status---returns-a-string-which-describes-the-meaning-of-status-codes)
            * [sky_psource() - returns a string which describes the source of the location](#sky_psource---returns-a-string-which-describes-the-source-of-the-location)
            * [sky_close() - frees any resources in use by the Skyhook library](#sky_close---frees-any-resources-in-use-by-the-skyhook-library)
        * [Appendix](#appendix)
            * [API Return Codes](#api-return-codes)
            * [API Error Codes](#api-error-codes)
            * [API Location Result](#api-location-result)
            * [API sky_finalize_request() result](#api-sky_finalize_request-result)
            * [API Logging levels](#api-logging-levels)
    * [License](#license)

## Building

### Dependencies

#### Git Submodules

The library depends on several third-party repos which are included as submodules in this repo. In order to ensure that
these dependency repos are properly populated, be sure to add the `--recursive` option to the `git clone` command when
cloning this repo.

All submodules are contained within the `submodules` directory.

### Library Source Files

The library includes the following C source files:

* All `.h` and `.c` files in the `libel/` directory
* All `.h` and `.c` files in the `libel/protocol` directory (note that `el.pb.h`
  and `el.pb.c` can be regenerated by the protocol buffers compiler)
* `submodules/tiney-AES128-C/aes.{h,c}`
* `submodules/nanopb/pb_common.c`
* `submodules/nanopb/pb_encode.c`
* `submodules/nanopb/pb_decode.c`

### Build Directions

To build just the library in a Unix-like environment:

    $ make

This creates the file `bin/libel.a`, which can then be statically linked into your executable (in a Unix-like
environment). Of course it may be necessary to modify the provided Makefile (or replace it altogether) in order to build
the library for your platform.

To build and run the sample client (after building the library itself):

    $ cd sample_client
    $ make
    $ ./sample_client sample_client.conf

Note that `sample_client.conf` will likely require modification (to add your Skyhook AES key and partner ID).

## API Guide

### Summary

Skyhook's Embedded Library API for IoT provides an interface for the Client Application to assemble a Skyhook Location
Request and get an immediate location response when the IoT device is stationary (or in a remembered location), and an
encoded request that can be sent to the ELG server. The library also provides decoding and extracting of information (
including location) from the ELG server response message.

Requests consist of a set of beacons (Wi-Fi Access Points and Cell towers) which are within range of the device. These
are packaged into the request and the Skyhook server processes the available information to determine the most accurate
location. The library balances accuracy of location against bandwidth of communication with the Skyhook server by having
a configurable, maximum size of each request. This is achieved as each Wi-Fi or Cellular beacon is added to the request;
the library selectively keeps or discards beacon information based on a carefully crafted algorithm.

The library will save the information provided in a request along with the location determined by the Skyhook server in
a cache and whenever subsequent requests are a reasonable match to the cache, the library will use the known location to
satisfy the request, eliminating the need to communicate with the server for this request. The number of locations in
the cache is configurable, and a value of 0 will disable caching.

### Requirements

1. Low memory use, both code and data size.
1. Low bandwidth i.e. Small messages, infrequently.
1. Avoid platform specific dependencies i.e. ELG library is written to be portable to many platforms.

### API conventions

* Application/device developer is responsible for providing the space used to hold LibEL session state which includes
  the cache and configuration parameters. This space can be freed after the library has been closed and the space has
  been saved to non-volatile memory (if it is to be restored for the next session).
* Application/device developer is responsible for providing the temporary work space used to assemble request/response
  transaction.
* The address of this work space is considered the unique identifier for a given location request/response transaction.
* sky_errno is always set, either to `SKY_ERROR_NONE` or to the error code.
* The user may free the allocated work space after a location (lat/long) results from either sky_finalize_request() or
  sky_decode_response(), or an error from the API.
* The user is responsible for sending requests generated by the library to the server, and reporting responses received
  from the server to the library.
* The user must send request to the server, even when a location is returned from a cached response, if any of the three
  conditions are required: 1) downlink data is to be collected in a response, 2) uplink data needs to be sent to the
  server, 3) the cached location needs to be reported to the server to be forwarded by the ECHO service (debounce).
* If time() returns a date prior to March 1, 2019, API will not match cached scans/locations.
* The library is not thread safe.
* The user may preserve cache state by storing the state buffer provided by the call to sky_close(), and restoring the
  state buffer and passing it to sky_open().
* The library can interact with the server in either of two authentication modes, Token Based Registration (TBR) or a
  simple key based scheme. The best one for you will depend on licensing terms, privacy considerations and other
  specific implementation requirements
* The API calls used to add beacons from a scan provide a `is_connected` parameter which is used to indicate whether a
  particular beacon is actively serving the device i.e. a Wi-Fi AP which is connected and/or a Serving Cell.

### Authentication

For a full list of errors that can be reported when decoding a server response, see sky_decode_response().

#### Token Based Registration (TBR)

A device will remain authenticated during a valid licensed period, and thereafter if the license period is extended.
However, if there are repeated authentication failures, perhaps due to an expired contract, the library will enforce
service prohibition periods. An error will be returned, and sky_errno set to `SKY_ERROR_SERVICE_DENIED`, if the user
attempts to generate a request too soon after decoding an unauthorized response. Specific codes are reported when the
user is expected to delay the next request.

* `SKY_ERROR_SERVICE_DENIED` Service blocked due to repeated errors
* `SKY_AUTH_RETRY` retry immediately
* 'SKY_AUTH_RETRY_8HR' delay for 8 hours
* 'SKY_AUTH_RETRY_16HR' delay for 16 hours
* 'SKY_AUTH_RETRY_1D' delay for 1 day (24 hours)
* 'SKY_AUTH_RETRY_30D' delay for 30 days

#### Simple Key based

If the user has chosen to use the library in this mode, there are a few features that are not available, these are noted
in the remainder of the document with (TBR only). Authentication errors will result in an error being returned, and
sky_errno set to `SKY_ERROR_AUTH`.

### Client Configuration

#### Credential Configuration

Your LibEL needs to be configured with a Skyhook partner ID and AES (encryption) key. You can obtain these parameters
via the following steps:

1. Register a new account on http://my.skyhookwireless.com
2. Once registration is completed you will be placed immediately into the ???Start a new project??? workflow.
3. Create a new Precision Location project and name the project whatever you like.
4. After creating the project, in the platform selection screen, select the ???ELG??? embedded project type which can be
   found below the standard OS selection icons.
5. Once your new embedded project has been created, your partner ID and AES key will be visible on the right side of
   your project home screen. To enable Token Based Registration for your project, please reach out to your Skyhook
   account representative or contact support@skyhook.com

|  Description                |  Value (Example)                                     |
|:----------------------------|:--------------------------------------------|
|                             |                                             |
| Skyhook API key             |"1E5D0AAEA1DEC1CD10DC2DD1CA11CE6B"           |
| Partner ID                  |1                                            |
| SKU                         |"" (defined only for TBR Authentication)     |
| Skyhook ELG Server hostname |"elg.skyhook.com"                            |
| Skyhook ELG Server port     |9756                                         |

**Note** These values must be stored within the client application and passed to sky_open().

The LibEL creates (see sky_encode_request()) and interprets (see sky_decode_response()) messages which are exchanged via
a simple TCP (not HTTP) connection established by the client device (the "user") to the Skyhook Embedded Library
Gateway (ELG) server. This message exchange must be done for each call to sky_encode_request() (
unless the library is able to use a previously cached location). Because they are usually platform-dependent, the
details associated with the establishment of this connection, and the associated send/receive mechanisms, are the
responsibility of the application developer. See the sample_client directory within the library repo for an example of
how this can be done using standard socket API calls.

In general, the user must take the following steps in order to perform this exchange:

1. Establish a TCP connection to the configured host and port number (see the Configuration Parameters table above)
2. Send the request
3. Wait for and then receive the response (use whatever timeout value is appropriate for the network in which the device
   is expected to run; the additional latency overhead imposed by the Skyhook service itself should be well under one
   second)
4. Close the TCP connection

The user may decide to add beacons from multiple scans to a single request. In an environment with few APs, high RF
interference or poor HW WiFi scan, this technique may improve accuracy and yield significantly. In an environment with
many APs, it has no impact. Time to fix in this case may be up to 10 seconds longer (1-3 seconds per scan depending on
the environment). This needs to be accounted in application layer and confirmed acceptable. There is minor battery
impact on power consumption due to multiple scans in some cases detailed above. The timestamp associated with each
beacon must accurately reflect the time at which the scan was captured.

#### Build Time Configuration (libel/config.h)

| Preprocessor Symbol         | Description                                         | Default Value    |
|:----------------------------|:----------------------------------------------------|:-----------------|
| `CACHE_SIZE`                | typically set to '1' meaning that there is one available cache entry reserved. Setting `CACHE_SIZE` to 0 will disable the cache. Values of `CACHE_SIZE` of `2` and `3` provide further small improvement to cache performance at the cost of higher memory requirements. Contact your Skyhook representative for help tuning the library for your application. | 1             |
| `SKY_MAX_DL_APP_DATA`       | allows the maximum size of downlink application data to be defined, however the default of `100` is recommended. This provides the ability to limit the buffer space required to receive a response message. This value must accommodate the length of downlink application date set at the server. The server will not send application data that is longer than this value in response messages. |100    |
| `SKY_TBR_DEVICE_ID`         | this boolean value chooses whether a TBR location request carries with it the unique device ID. Devices using TBR authentication, which also make use of the ECHO service and wish to receive an identifier in Skyhook's device_id field, will need to build with `SKY_TBR_DEVICE_ID` `true' in order to correlate locations with a device. Alternatively, this information can be transmitted through uplink application data. |true |
| `SKY_LOGGING`               | controls whether debug information is generated by the library. By default, it includes `SKY_LOG_LEVEL_DEBUG` logging to assist with integration efforts. To remove this, build the library with `SKY_LOGGING` false. Passing a min_level value to sky_open() allows intermediate levels of logging. |true |
| `SKY_LOG_LENGTH`            | can be used to adjust the maximum line length of logging, if required. | 120 |
| `SANITY_CHECKS`             | controls whether or not parameter range and other consistency checking code is included in the library. Enabling (true) this feature can be useful for discovering issues during application development. Disabling (false) the feature (e.g., for final product deployment) results in a smaller code footprint size. | false |
| `SKY_EXCLUDE_WIFI_SUPPORT`  | may be set to true if no Wi-Fi scan data will be added to a request. This reduces the size of the library code. When set to true Wi-Fi support is excluded | false |
| `SKY_EXCLUDE_CELL_SUPPORT`  |  may be set to true if no Wi-Fi scan data will be added to a request. This reduces the size of the library code. When set to true Cell support is excluded| false |
| `SKY_EXCLUDE_GNSS_SUPPORT`  |  may be set to true if no Wi-Fi scan data will be added to a request. This reduces the size of the library code. When set to true GNSS support is excluded| false |

When a server response is decoded, the location and scan information is stored in the cache. Susequent calls to
sky_get_cache_hit() will compare scan information in the request with the cache. If a good match is found, the cached
location is returned along with a request buffer. The application may use the cached location (
reduced network traffic) or send the request (update server with the uplink application data and position). For a
stationary device the scan matching helps to significantly reduce the number of transactions to server (by 80 - 90%)
and allows the client to report last known location without accuracy impact. This results in significant power
consumption savings. For high speed moving devices (driving), scan matching fails typically and as a result it has no
impact on battery or accuracy. For slow speed moving devices (walking/biking) the stationary logic helps reduce number
of transactions to server by as much as 50% but may introduce some lag in reported fixes relative to device location.

<div style="page-break-after: always"></div>

### General Sequence of Operations

![missing image](images/elg_embedded_image.png?raw=true)

Figure 1 - The User is expected to make a sequence of calls as shown
<div style="page-break-after: always"></div>

## API Reference

### sky_open() - Initialize Skyhook library and verify access to resources

```c
Sky_status_t sky_open(Sky_errno_t *sky_errno,
    uint8_t *device_id,
    uint32_t id_len,
    uint32_t partner_id,
    uint8_t aes_key[AES_KEYLEN],
    char *sku,
    uint32_t cc,
    Sky_sctx_t *sctx,
    Sky_log_level_t min_level,
    Sky_loggerfn_t logf,
    Sky_randfn_t rand_bytes,
    Sky_timefn_t gettime)

/* Parameters
 * sky_errno    if sky_open() returns failure, sky_errno is set to the error code
 * device_id    Device unique ID (example mac address of the device)
 * id_len       length if the Device ID, typically 6, Max 16 bytes
 * partner_id   Skyhook assigned credentials
 * aes_key      Skyhook assigned encryption key
 * sku          Skyhook assigned model number / product identifier
 * cc           Country Code (optional) country in which the device was registered. 0 if undefined
 * sctx         pointer to a session context buffer , empty, or restored from previous session
 * min_level    logging function is called for msg with equal or greater level
 * logf         pointer to logging function
 * rand_bytes   pointer to random function
 * gettime      pointer to time function
 *
 * Returns      `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
 ```

Called once to set up any resources needed by the library (e.g. cache space). Returns `SKY_SUCCESS` on
success, `SKY_ERROR` on error and sets sky_errno. If the buffer pointed to by sctx is not empty (filled with zeros) it
must be a valid copy of a session context buffer from a previously closed session, otherwise it is it must be empty. If
valid, it is restored including cache content if not stale. If the session buffer is not valid and not empty, it is
considered an error (`SKY_ERROR_BAD_SESSION_CTX`). If sku is a non-zero length string, LibEL will attempt to use TBR
authentication otherwise a simple key based authentication is used. cc is the Mobile Country Code of the country in
which the device was registered to operate. If logf() is not NULL, the log messages will be generated if they were
turned on during compilation (`SKY_LOGGING`). If rand_bytes is not NULL, this function pointer is used to generate
random sequences of bytes, otherwise the library calls rand(). If min_level can be set to block less severe log
messages, e.g. if min_level is set to `SKY_LOG_LEVEL_ERROR`, only log messages with level `SKY_LOG_LEVEL_CRITICAL`
and `SKY_LOG_LEVEL_ERROR` will be generated. gettime is required. This function pointer is used to request the current
time (Unix time aka POSIX time aka UNIX **Epoch** time). If the system may use LibEL before time has been synchronized,
gettime must return 0 to indicate time is not known. When debounce is false, the generated request always reports the
beacons from the request context to the server. When true, and a previously cached location is a match, the cached
beacons are reported to the server. While a device is stationary, and using the cached location, any server requests
will resolve to a stable location.

`sky_open()` may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_NO_PLUGIN`                           | At least one plugin must be registered
| `SKY_ERROR_ALREADY_OPEN`                        | sky_open() called more than once with different parameters. Call sky_close()
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal

### sky_sizeof_session_ctx() - Get the size of the non-volatile memory required to save and restore the library state.

```c
int32_t sky_sizeof_session_ctx(Sky_sctx_t *sctx)

/* Parameters
 * sctx             Pointer to session context buffer, or NULL to request the size of a new empty session context buffer.

 * Returns          Size of session context buffer or 0 to indicate that the buffer was invalid
 */
 ```

Can be used to determine the size of a session context buffer that is being restored from a pervious LibEL session, or
the size of a new empty buffer that the User is allocating for an initial session. When LibEL is opened, a session
buffer must be provided, in RAM, either initialized with the contents from a previous, saved, buffer, or an empty
buffer. The session buffer contains the cache and other library state. Copying the session buffer to non-volatile memory
after closing the library and copying from non-volatile memory to RAM before opening the library allows the state of the
library to be preserved during periods where RAM contents may be lost e.g. low power modes.

### sky_sizeof_request_ctx() - Determines the size of the work space required to process the request

```c
int32_t sky_sizeof_request_ctx(void)

/* Returns          Size of request context buffer required
 */
```

Reports the number of bytes of request context buffer required to handle encoding the request. This should be allocated
by the caller and a pointer to this allocated space passed to sky_new_request(). This space is used to accumulate the
beacon information to form the server request. It also holds any downlink application data. This space should be freed
after receiving a location (lat/long) from either sky_get_cache_hit() or sky_decode_response(), or an error from the API
and after processing downlink application data.

### sky_new_request() - Initializes request context for a new request

```c
Sky_rctx_t* sky_new_request(Sky_rctx_t *rctx,
    uint32_t rbufsize,
    Sky_sctx_t *sctx,
    uint8_t *ul_app_data,
    uint32_t ul_app_data_len,
    Sky_errno_t *sky_errno
)

/* Parameters
 * rctx                 Pointer to request context buffer provided by user
 * rbufsize             Request buffer size (from sky_sizeof_request_ctx)
 * sctx                 Pointer to session context buffer provided to sky_open
 * ul_app_data          Pointer to uplink data buffer
 * ul_app_data_len      Length of uplink data buffer
 * sky_errno            Pointer to error code

 * Returns              Pointer to the initialized request context buffer or NULL
 */
```

Initializes context buffer for a new request. Returns request context (pointer to the work space) or NULL in case of
failure. In case of failure sky_errno is set to indicate the error. User can decode the error using sky_perror().
Initializes request context with Magic numbers, sets downlink app data buffer to empty and saves uplink app data.

sky_new_request() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_NEVER_OPEN`                          | sky_open() must be called before the current operation can succeed
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_SERVICE_DENIED`                      | LibEL temporarily blocked this service after repeated fails to authenticate (TBR only)

### sky_add_ap_beacon() - Add a Wi-Fi beacon to request context

```c
Sky_status_t sky_add_ap_beacon(Sky_rctx_t *rctx,
    Sky_errno_t *sky_errno,
    uint8_t mac[6],
    time_t timestamp,
    int16_t rssi,
    int32_t frequency,
    bool is_connected
)

/* Parameters
 * rctx         Skyhook request context
 * sky_errno    sky_errno is set to the error code
 * mac          pointer to mac address of the Wi-Fi beacon
 * timestamp    time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 * rssi         Received Signal Strength Intensity, -10 through -127, -1 if unknown
 * frequency    Center Frequency of Channel in MHz, 2400 through 6000, -1 if unknown
 * is_connected this beacon is currently connected, false if unknown

 * Returns      `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
 ```

Adds the Wi-Fi access point information to the request context. Returns `SKY_ERROR` for failure or the `SKY_SUCCESS`. In
case of failure sky_errno is set to error code. If enough Wi-Fi beacons are already in the request context,
sky_add_ap_beacon() may remove the least useful beacon.

sky_add_ap_beacon() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_NEVER_OPEN`                          | sky_open() must be called before the current operation can succeed
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_INTERNAL`                            | An unexpected error occured

### sky_add_cell_lte_beacon() - Add an lte or lte-CatM1 cell beacon to request context

```c
Sky_status_t sky_add_cell_lte_beacon(Sky_rctx_t *rctx,
    Sky_errno_t *sky_errno,
    int32_t tac,
    int64_t e_cellid,
    uint16_t mcc,
    uint16_t mnc,
    int16_t pci,
    int32_t earfcn,
    int32_t ta,
    time_t timestamp,
    int16_t rsrp,
    bool is_connected
)

/* Parameters
 * rctx         Skyhook request context
 * sky_errno    sky_errno is set to the error code
 * tac          lte tracking area code identifier (1-65535), 'SKY_UNKNOWN_ID3' if unknown
 * e_cellid     lte beacon identifier 28bit (0-268435455)
 * mcc          mobile country code (200-799)
 * mnc          mobile network code (0-999)
 * pci          mobile pci (0-503), 'SKY_UNKNOWN_ID5' if unknown
 * earfcn,      channel (0-45589), 'SKY_UNKNOWN_ID6' if unknown
 * ta           timing-advance (0-7690), `SKY_UNKNOWN_TA` if unknown
 * timestamp    time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 * rsrp         Received Signal Receive Power, range -140 to -40dbm, -1 if unknown
 * is_connected this beacon is currently connected, false if unknown

 * Returns      `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
```

Adds the cell lte or lte-CatM1 information to the request context. Returns `SKY_ERROR` for failure or the `SKY_SUCCESS`.
In case of failure sky_errno is set if there is a bad parameter or other error.

sky_add_cell_lte_beacon() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_NEVER_OPEN`                          | sky_open() must be called before the current operation can succeed
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt
| `SKY_ERROR_INTERNAL`                            | An unexpected error occured

### sky_add_cell_lte_neighbor_beacon() - Adds an LTE neighbor cell beacon to the request context

```c
Sky_status_t sky_add_cell_lte_neighbor_beacon(Sky_rctx_t *rctx,
    Sky_errno_t *sky_errno,
    int16_t pci,
    int32_t earfcn,
    time_t timestamp,
    int16_t rsrp)

/* Parameters
 * rctx         Skyhook request context
 * sky_errno    sky_errno is set to the error code
 * pci          mobile pci (0-503), 'SKY_UNKNOWN_ID5' if unknown
 * earfcn,      channel (0-45589), 'SKY_UNKNOWN_ID6' if unknown
 * timestamp    time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 * rsrp         Received Signal Receive Power, range -140 to -40dbm, -1 if unknown

 * Returns      `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */

 ```

Adds the lte or lte-CatM1 neighbor cell information to the request context. Returns `SKY_ERROR` for failure or
the `SKY_SUCCESS`. In case of failure sky_errno is set if there is a bad parameter or other error.

sky_add_cell_lte_neighbor_beacon() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_NEVER_OPEN`                          | sky_open() must be called before the current operation can succeed
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt
| `SKY_ERROR_INTERNAL`                            | An unexpected error occured

### sky_add_cell_gsm_beacon() - Adds a gsm cell beacon to the request context

```c
Sky_status_t sky_add_cell_gsm_beacon(Sky_rctx_t * rctx,
    Sky_errno_t *sky_errno,
    int32_t lac,
    int64_t ci,
    uint16_t mcc,
    uint16_t mnc,
    int16_t bsic,
    int32_t arfcn
    int32_t ta,
    time_t timestamp,
    int16_t rssi,
    bool is_connected
)

/* Parameters
 * rctx         Skyhook request context
 * sky_errno    sky_errno is set to the error code
 * lac          gsm location area code identifier (1-65535)
 * ci           gsm cell identifier (0-65535)
 * mcc          mobile country code (200-799)
 * mnc          mobile network code (0-999)
 * bsic         Base Station Identify Code (0-63), SKY_UNKNOWN_ID5 if unknown
 * arfcn        channel (1-1023), SKY_UNKNOWN_ID6 if unknown
 * ta           timing-advance (0-63), `SKY_UNKNOWN_TA` if unknown
 * timestamp    time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 * rssi         Received Signal Strength Intensity, range -128 to -32dbm, -1 if unknown
 * is_connected this beacon is currently connected, false if unknown

 * Returns      `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
 ```

### sky_add_cell_gsm_neighbor_beacon() - Adds an GSM neighbor cell beacon to the request context

```c
Sky_status_t sky_add_cell_gsm_neighbor_beacon(Sky_rctx_t *rctx,
    Sky_errno_t *sky_errno,
    int16_t bsic,
    int32_t arfcn,
    time_t timestamp,
    int16_t rssi)

/* Parameters
 * rctx         Skyhook request context
 * sky_errno    sky_errno is set to the error code
 * bsic         mobile pci (0-63), 'SKY_UNKNOWN_ID5' if unknown
 * arfcn,       channel (1-1023), 'SKY_UNKNOWN_ID6' if unknown
 * timestamp    time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 * rssi         Received Signal Strength Intensity, range -128 to -32dbm, -1 if unknown

 * Returns      `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */

 ```

Adds the gsm neighbor cell information to the request context. Returns `SKY_ERROR` for failure or the `SKY_SUCCESS`. In case of
failure sky_errno is set if there is a bad parameter or other error.

sky_add_cell_gsm_neighbor_beacon() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_NEVER_OPEN`                          | sky_open() must be called before the current operation can succeed
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt
| `SKY_ERROR_INTERNAL`                            | An unexpected error occured

### sky_add_cell_umts_beacon() - Adds a umts cell beacon to the request context

```c
Sky_status_t sky_add_cell_umts_beacon(Sky_rctx_t *rctx,
    Sky_errno_t *sky_errno,
    int32_t lac,
    int64_t ucid,
    uint16_t mcc,
    uint16_t mnc,
    int16_t psc,
    int16_t uarfcn,
    time_t timestamp,
    int16_t rscp,
    bool is_connected
)

/* Parameters
 * rctx         Skyhook request context
 * sky_errno    sky_errno is set to the error code
 * lac          umts location area code identifier (1-65535), `SKY_UNKNOWN_ID`3 if unknown
 * ucid         umts cell identifier 28bit (0-268435455)
 * mcc          mobile country code (200-799)
 * mnc          mobile network code (0-999)
 * psc          primary scrambling code (0-511), `SKY_UNKNOWN_ID`5 if unknown
 * uarfcn       channel (412-10838), `SKY_UNKNOWN_ID`6 if unknown
 * timestamp    time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 * rscp         Received Signal Code Power, range -120dbm to -20dbm, -1 if unknown
 * is_connected This beacon is currently connected, false if unknown

 * Returns      `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
 ```

Adds the cell umts information to the request context. Returns `SKY_ERROR` for failure or the `SKY_SUCCESS`. In case of
failure sky_errno is set if there is a bad parameter or other error.

sky_add_cell_umts_beacon() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_NEVER_OPEN`                          | sky_open() must be called before the current operation can succeed
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt
| `SKY_ERROR_INTERNAL`                            | An unexpected error occured

### sky_add_cell_umts_neighbor_beacon() - Adds a umts neighbor cell beacon to the request context

```c
Sky_status_t sky_add_cell_umts_neighbor_beacon(Sky_rctx_t *rctx,
    Sky_errno_t *sky_errno,
    int16_t psc,
    int16_t uarfcn,
    time_t timestamp,
    int16_t rscp
)

/* Parameters
 * rctx         Skyhook request context
 * sky_errno    sky_errno is set to the error code
 * psc          primary scrambling code (0-511), `SKY_UNKNOWN_ID`5 if unknown
 * uarfcn       channel (412-10838), `SKY_UNKNOWN_ID`6 if unknown
 * timestamp    time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 * rscp         Received Signal Code Power, range -120dbm to -20dbm, -1 if unknown

 * Returns      `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
 ```

Adds the umts neighbor cell information to the request context. Returns `SKY_ERROR` for failure or the `SKY_SUCCESS`. In
case of failure sky_errno is set if there is a bad parameter or other error.

sky_add_cell_umts_neighbor_beacon() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_NEVER_OPEN`                          | sky_open() must be called before the current operation can succeed
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt
| `SKY_ERROR_INTERNAL`                            | An unexpected error occured

### sky_add_cell_cdma_beacon() - Adds a cdma cell beacon to the request context

```c
Sky_status_t sky_add_cell_cdma_beacon(Sky_rctx_t *rctx,
    Sky_errno_t *sky_errno,
    uint32_t sid,
    uint32_t nid,
    uint64_t bsid,
    time_t timestamp,
    int16_t rssi,
    bool is_connected
)

/* Parameters
 * rctx         Skyhook request context
 * sky_errno    sky_errno is set to the error code
 * sid          cdma system identifier (0-32767)
 * nid          cdma network identifier(0-65535)
 * bsid         cdma base station identifier (0-65535)
 * timestamp    time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 * rssi         RSSI (Received Signal Receive Power level) range -140 to -49dbm, -1 if unknown
 * is_connected this beacon is currently connected, false if unknown

 * Returns      `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
 ```

Adds the cell cdma information to the request context. Returns NULL and sets sky_errno if there is a bad parameter or
other error.

sky_add_cell_cdma_beacon() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_NEVER_OPEN`                          | sky_open() must be called before the current operation can succeed
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt
| `SKY_ERROR_INTERNAL`                            | An unexpected error occured

### sky_add_cell_nb_iot_beacon() - Adds a nb_iot cell beacon to the request context

```c
Sky_status_t sky_add_cell_nb_iot_beacon(Sky_rctx_t *rctx,
    Sky_errno_t *sky_errno,
    uint16_t mcc,
    uint16_t mnc,
    int64_t e_cellid,
    int32_t tac,
    int16_t ncid,
    int32_t earfcn,
    time_t timestamp,
    int16_t nrsrp,
    bool is_connected
)

/* Parameters
 * rctx         Skyhook request context
 * sky_errno    sky_errno is set to the error code
 * mcc          mobile country code (200-799)
 * mnc          mobile network code (0-999)
 * e_cellid     nbiot beacon identifier (0-268435455)
 * tac          nbiot tracking area code identifier (1-65535), `SKY_UNKNOWN_ID`3 if unknown
 * ncid         mobile cell ID  (0-503), `SKY_UNKNOWN_ID`5 if unknown
 * earfcn       channel (0-45589), `SKY_UNKNOWN_ID`6 if unknown
 * timestamp    time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 * nrsrp        Narrowband Reference Signal Received Power, range -156 to -44dbm, -1 if unknown
 * is_connected this beacon is currently connected, false if unknown

 * Returns      `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
 ```

Adds the cell nb-IoT information to the request context. Returns `SKY_ERROR` for failure or the `SKY_SUCCESS`. In case
of failure sky_errno is set if there is a bad parameter or other error.

sky_add_cell_nb_iot_beacon() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_NEVER_OPEN`                          | sky_open() must be called before the current operation can succeed
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt
| `SKY_ERROR_INTERNAL`                            | An unexpected error occured

### sky_add_cell_nb_iot_neighbor_beacon() - Adds a neighbor nb_iot cell beacon to the request context

```c
Sky_status_t sky_add_cell_nb_iot_neighbor_beacon(Sky_rctx_t *rctx,
    Sky_errno_t *sky_errno,
    int16_t ncid,
    int32_t earfcn,
    time_t timestamp,
    int16_t nrsrp
)

/* Parameters
 * rctx         Skyhook request context
 * sky_errno    sky_errno is set to the error code
 * ncid         mobile cell ID  (0-503), `SKY_UNKNOWN_ID`4 if unknown
 * earfcn       channel (0-45589), `SKY_UNKNOWN_ID`6 if unknown
 * timestamp    time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 * nrsrp        Narrowband Reference Signal Received Power, range -156 to -44dbm, -1 if unknown

 * Returns      `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
 ```

Adds the nb-IoT neighbor cell information to the request context. Returns `SKY_ERROR` for failure or the `SKY_SUCCESS`.
In case of failure sky_errno is set if there is a bad parameter or other error.

sky_add_cell_nb_iot_neighbor_beacon() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_NEVER_OPEN`                          | sky_open() must be called before the current operation can succeed
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt
| `SKY_ERROR_INTERNAL`                            | An unexpected error occured

### sky_add_cell_nr_beacon() - Adds a NR cell beacon to the request context

```c
Sky_status_t sky_add_cell_nr_beacon(Sky_rctx_t *rctx,
    Sky_errno_t *sky_errno,
    uint16_t mcc,
    uint16_t mnc,
    int64_t nci,
    int32_t tac,
    int16_t pci,
    int32_t nrarfcn,
    int32_t ta,
    time_t timestamp,
    int16_t nrsrp,
    bool is_connected
)

/* Parameters
 * rctx         Skyhook request context
 * sky_errno    sky_errno is set to the error code
 * mcc          mobile country code (200-799)
 * mnc          mobile network code (0-999)
 * nci          nr cell identity (0-68719476735)
 * tac          tracking area code identifier (1-65535), `SKY_UNKNOWN_ID`3 if unknown
 * pci          physical cell ID (0-1007), `SKY_UNKNOWN_ID`5 if unknown
 * nrarfcn      channel (0-3279165), `SKY_UNKNOWN_ID`6 if unknown
 * ta           timing-advance (0-63), `SKY_UNKNOWN_TA` if unknown
 * timestamp    time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 * nrsrp        Narrowband Reference Signal Received Power, range -156 to -44dbm, -1 if unknown
 * is_connected this beacon is currently connected, false if unknown

 * Returns      `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
 ```

Adds the cell NR information to the request context. Returns `SKY_ERROR` for failure or the `SKY_SUCCESS`. In case of
failure sky_errno is set if there is a bad parameter or other error.

sky_add_cell_nr_beacon() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_NEVER_OPEN`                          | sky_open() must be called before the current operation can succeed
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt
| `SKY_ERROR_INTERNAL`                            | An unexpected error occurred

### sky_add_cell_nr_neighbor_beacon() - Adds a neighbor NR cell beacon to the request context

```c
Sky_status_t sky_add_cell_nr_neighbor_beacon(Sky_rctx_t *rctx,
    Sky_errno_t *sky_errno,
    int16_t pci,
    int32_t nrarfcn,
    time_t timestamp,
    int16_t nrsrp
)

/* Parameters
 * rctx         Skyhook request context
 * sky_errno    sky_errno is set to the error code
 * pci          physical cell ID (0-1007), `SKY_UNKNOWN_ID`5 if unknown
 * nrarfcn      channel (0-3279165), `SKY_UNKNOWN_ID`6 if unknown
 * timestamp    time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown
 * nrsrp        Narrowband Reference Signal Received Power, range -156 to -44dbm, -1 if unknown

 * Returns      `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
 ```

Adds the NR neighbor cell information to the request context. Returns `SKY_ERROR` for failure or the `SKY_SUCCESS`. In
case of failure sky_errno is set if there is a bad parameter or other error.

sky_add_cell_nr_neighbor_beacon() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_NEVER_OPEN`                          | sky_open() must be called before the current operation can succeed
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt
| `SKY_ERROR_INTERNAL`                            | An unexpected error occured

### sky_add_gnss() - Adds the position of the device from GNSS (GPS, GLONASS, or others) to the request context

```c
sky_status_t sky_add_gnss(sky_ctx_t *rctx,
sky_errno_t *sky_errno,
float lat,
float lon,
uint16_t hpe,
float altitude,
uint16_t vpe,
float speed,
float bearing,
uint16_t nsat,
time_t timestamp)

/* Parameters
 * rctx         Skyhook request context
 * sky_errno    sky_errno is set to the error code
 * lat          device latitude in degrees, NaN if unknown
 * lon          device longitude in degrees, NaN if unknown
 * hpe          pointer to horizontal Positioning Error in meters with 68% confidence, 0 if unknown
 * altitude     pointer to altitude above mean sea level, in meters, NaN if unknown
 * vpe          pointer to vertical Positioning Error in meters with 68% confidence, 0 if unknown
 * speed        pointer to speed in meters per second, Nan if unknown
 * bearing      pointer to bearing of device in degrees, counterclockwise from north
 * nsat         number of satellites used to determine location, 0 if unknown
 * timestamp    time in seconds (from 1970 epoch) indicating when the scan was performed, (time_t)-1 if unknown

 * Returns      `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
 ```

Adds the GNSS information to the request context. Returns `SKY_ERROR` for failure or the `SKY_SUCCESS`. In case of
failure sky_errno is set to error code.

sky_add_gnss() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt

### sky_search_cache() - compares the new request beacons with those in the cache.

```c
Sky_status_t sky_search_cache(Sky_rctx_t *rctx,
Sky_errno_t *sky_errno,
bool *cache_hit,
Sky_location_t *loc
)

/*
 * Parameters
 * rctx             Skyhook request context
 * sky_errno        sky_errno is set to the error code
 * cache_hit        pointer to location where true or false is stored based on the match between new scan and cache
 * loc              where to save device latitude, longitude etc from cache if known

 * Returns          `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
 ```

Optionally called after all beacon and GNSS data has been added (via the sky_add_*() functions) in order to determine
whether there is a good match to the new scan in the cache. Loc is set to the location of the matching cache when a
cache hit.

sky_search_cache() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt

### sky_ignore_cache_hit() - allows the result of sky_search_cache to be overridden

```c
Sky_status_t sky_ignore_cache_hit(Sky_rctx_t *rctx,
Sky_errno_t *sky_errno
)

/*
 * Parameters
 * rctx             Skyhook request context
 * sky_errno        sky_errno is set to the error code

 * Returns          `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
 ```

Optionally called after sky_search_cache(). If a cache hit had been found, the hit is cleared.

A warning is logged if sky_ignore_cache_hit is called after a cache miss. sky_ignore_cache_hit() reports SKY_SUCCESS
even if a cache hit is not cleared.

sky_ignore_cache_hit() is provided to clear a cache_hit when the cached location is determined to be less accurate than
the new scan. sky_override_cache_hit() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt
| `SKY_ERROR_BAD_SESSION_CTX`                     | The session context buffer is corrupt

### sky_sizeof_request_buf()  - determines the size of the network request buffer which must be provided by the user

```c
Sky_status_t sky_sizeof_request_buf(Sky_rctx_t *rctx,
    uint32_t *size,
    Sky_errno_t *sky_errno
)

/*
 * Parameters
 * rctx             Skyhook request context
 * sky_errno        sky_errno is set to the error code
 * size             Request size in bytes stored here

 * Returns          `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
 ```

Called after all beacon and GNSS data has been added (via the sky_add_*() functions) in order to determine the size of
the request buffer that must be allocated by the user. The size returned includes the length of any uplink application
data reported to sky_new_request().

sky_sizeof_request_buf() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt
| `SKY_ERROR_ENCODE_ERROR`                        | Could not encode the request

### sky_encode_request() - generate a Skyhook request from the request context

```c
Sky_status_t sky_encode_request(Sky_rctx_t *rctx,
    Sky_errno_t *sky_errno,
    void *request_buf,
    uint32_t bufsize,
    uint32_t *response_size
)

/* Parameters
 * rctx             Skyhook request context
 * sky_errno        sky_errno is set to the error code
 * request_buf      Request buffer (allocated by the user) into which the Skyhook server request will be encoded
 * bufsize          Request buffer size in bytes (should be set to the result of the call to sky_sizeof_request_buf() function)
 * response_size    the space required to hold the server response

 * Returns          SKY_SUCCESS if request was encoded, SKY_ERROR and sets sky_errno with error code
 */
 ```

Returns `SKY_ERROR` and sets sky_errno if an error occurs. If the result is `SKY_SUCCESS` buffer is filled in with the
serialized request data which the user must then send to the Skyhook server, and the response_size is set to the maximum
buffer size needed to receive the Skyhook server response. If the error `
SKY_ERROR_SERVICE_DENIED` is returned, the request was submitted too often with repeated Authentication failures. The
user may generate a new request after correcting the problem (TBR only).

sky_encode_request() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt
| `SKY_ERROR_ENCODE_ERROR`                        | Could not encode the request
| `SKY_ERROR_SERVICE_DENIED`                      | LibEL temporarily blocked this service after repeated fails to authenticate (TBR only)
| `SKY_ERROR_NO_BEACONS`                          | Beacons must be added before a request can be encoded

### sky_decode_response() - decodes a Skyhook server response

```c
Sky_status_t sky_decode_response(Sky_rctx_t *rctx,
    Sky_errno_t *sky_errno,
    void *response_buf,
    uint32_t bufsize,
    Sky_location_t *loc
)

/* Parameters
 * rctx             Skyhook request context
 * sky_errno        sky_errno is set to the error code
 * response_buf     buffer holding the skyhook server response
 * bufsize          Size of response buffer in bytes as returned by sky_finalize_request()
 * loc              where to save device latitude, longitude etc from cache if known

 * Returns          `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
```

User calls this to process the network response from the Skyhook server. Returns `SKY_ERROR` and sets sky_errno if the
response buffer could not be decoded or other error occurred, otherwise `SKY_SUCCESS` is returned. Cache is updated with
scan and location information. Note that sky_decode_response() modifies the response buffer (it is written with
decrypted bytes). A DEBUG level log message is available to help diagnose any errors that may happen. If the server
sends downlink application data in the response, it is copied into the request context and loc is updated with its
length and a pointer to the data. Zero length indicates that there was no data. Note that if the user allocated the
memory for the workspace, the downlink data is unavailable after this memory has been freed. If the request required an
authentication registration step, sky_decode_response() returns `SKY_ERROR`, and sets sky_errno to `SKY_RETRY_AUTH`. The
user is expected to call sky_finalize_request() again and re-send the request (TBR only).

If `SKY_SUCCESS` is returned, the locations_source field provides information about how the location was derived from
the scan.

sky_decode_response() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_DECODE_ERROR`                        | LibEL was unable to decode the response message
| `SKY_ERROR_SERVER_ERROR`                        | Server sent a response which indicated an error processing the request
| `SKY_ERROR_LOCATION_UNKNOWN`                    | Server failed to determine a location
| `SKY_ERROR_AUTH`                                | Server failed to authenticate the request
| `SKY_AUTH_RETRY`                                | Server indicated that authentication was required and user must retry (TBR only)
| `SKY_AUTH_RETRY_8HR`                            | Server indicated that authentication failed. Service is blocked for 8 hours (TBR only)
| `SKY_AUTH_RETRY_16HR`                           | Server indicated that authentication failed. Service is blocked for 16 hours (TBR only)
| `SKY_AUTH_RETRY_24HR`                           | Server indicated that authentication failed. Service is blocked for 24 hours (TBR only)

### sky_get_option() - query the value of a configuration parameter

```c
Sky_status_t sky_get_option(Sky_rctx_t *rctx,
    Sky_errno_t *sky_errno,
    Sky_config_name_t name,
    uint32_t *value
)

/* Parameters
 * rctx             Skyhook request context
 * sky_errno        sky_errno is set to the error code
 * name             configuration parameter to read
 * value             Pointer to location where value should be returned

 * Returns          `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
```

User may call this to interrogate the current value of the dynamic configuration parameters listed below.

| Configuration parameter name                    | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `CONF_TOTAL_BEACONS`                            | Maximum number of beacons that can be held in a request
| `CONF_MAX_AP_BEACONS`                           | Maximum number of Access points that can be held in a request
| `CONF_CACHE_MATCH_THRESHOLD`                    | Obsolete
| `CONF_CACHE_AGE_THRESHOLD`                      | Life span in hours of a cache entry. Typically 24 hr
| `CONF_CACHE_BEACON_THRESHOLD`                   | The threshold below which an exact match is required for a cache hit. The following score thresholds are used when more than this number of APs are present in the new scan
| `CONF_CACHE_NEG_RSSI_THRESHOLD`                 | The signal strength below which an AP is considered of low priority
| `CONF_CACHE_MATCH_ALL_THRESHOLD`                | The score threshold used to compare new scan to cache
| `CONF_CACHE_MATCH_USED_THRESHOLD`               | The score threshold used when an enhanced matching algorithm can be used (Premium)
| `CONF_MAX_VAP_PER_AP`                           | Maximum number of Virtual APs that can be added to a group (Premium)
| `CONF_MAX_VAP_PER_RQ`                           | Maximum number of Virtual AP groups that can be compressed in a request (Premium)
| `CONF_LOGGING_LEVEL`                            | The severity level below which logged messages are suppressed

sky_get_option() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal

### sky_set_option() - set the value of a configuration parameter

```c
Sky_status_t sky_set_option(Sky_rctx_t *rctx,
    Sky_errno_t *sky_errno,
    Sky_config_name_t name,
    uint32_t value
)

/* Parameters
 * rctx             Skyhook request context
 * sky_errno        sky_errno is set to the error code
 * name             configuration parameter to set
 * value            value should be assigned

 * Returns          `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
```

User may call this to set the value of the dynamic configuration parameters listed below.

| Configuration parameter name                    | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `CONF_TOTAL_BEACONS`                            | Maximum number of beacons that can be held in a request
| `CONF_MAX_AP_BEACONS`                           | Maximum number of Access points that can be held in a request
| `CONF_CACHE_MATCH_THRESHOLD`                    | Obsolete
| `CONF_CACHE_AGE_THRESHOLD`                      | Life span in hours of a cache entry. Typically 24 hr
| `CONF_CACHE_BEACON_THRESHOLD`                   | The threshold below which an exact match is required for a cache hit. The following score thresholds are used when more than this number of APs are present in the new scan
| `CONF_CACHE_NEG_RSSI_THRESHOLD`                 | The signal strength below which an AP is considered of low priority
| `CONF_CACHE_MATCH_ALL_THRESHOLD`                | The score threshold used to compare new scan to cache
| `CONF_CACHE_MATCH_USED_THRESHOLD`               | The score threshold used when an enhanced matching algorithm can be used (Premium)
| `CONF_MAX_VAP_PER_AP`                           | Maximum number of Virtual APs that can be added to a group (Premium)
| `CONF_MAX_VAP_PER_RQ`                           | Maximum number of Virtual AP groups that can be compressed in a request (Premium)
| `CONF_LOGGING_LEVEL`                            | The severity level below which logged messages are suppressed

The following parameters can not be assigned a larger value than that used when LibEL is built:
`CONF_TOTAL_BEACONS`, `CONF_MAX_AP_BEACONS`, `CONF_MAX_VAP_PER_AP`, `CONF_MAX_VAP_PER_RQ`

If `SKY_SUCCESS` is returned, the value of the identified parameter is updated.

sky_set_option() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal

### sky_perror() - returns a string which describes the meaning of sky_errno codes

```c
char* sky_perror(sky_errno_t sky_errno)

/* Parameter
 * sky_errno        Error code for which to provide descriptive string

 * Returns          pointer to string or NULL if the code is invalid
 */
```

Return a static string which describes the meaning of the value passed in sky_errno, or "Unknown error" if sky_errno has
an unexpected value.

### sky_pbeacon() - returns a string which describes the type of a beacon

```c
char* sky_pbeacon(Beacon_t *beacon)

/* Parameter
 * beacon           pointer to Beacon_t structure for which to provide descriptive string

 * Returns          pointer to string or NULL if the code is invalid
 */
```

Return a static string which describes the type of the beacon passed in pointer beacon, or "Unknown" if beacon type has
an unexpected value.

### sky_pserver_status() - returns a string which describes the meaning of status codes

```c
char* sky_pserver_status(sky_server_status_t status)

/* Parameter
 * status           Status code for which to provide descriptive string

 * Returns          pointer to string or NULL if the status is invalid
 */
```

Return a static string which describes the meaning of the value passed in status, or "Unknown server status" if status
has an unexpected value. status is available in the location_status field of the Sky_location_t structure when
sky_decode_response() returns `SKY_SUCCESS`.

### sky_psource() - returns a string which describes the source of the location

```c
char *sky_psource(struct sky_location *loc)

/* Parameter
 * loc              location for which to provide descriptive string for the source

 * Returns          pointer to string or NULL if the status is invalid
 */
```

Return a static string which describes the source of the location passed in loc. Source is available in the
location_source field of the Sky_location_t structure when sky_decode_response() returns `SKY_SUCCESS`.

The string descriptions are:

| Location Source | Description
| -------------------------------| ------------
| `SKY_LOCATION_SOURCE_HYBRID`   | "Hybrid"
| `SKY_LOCATION_SOURCE_CELL`     | "Cell"
| `SKY_LOCATION_SOURCE_WIFI`     | "Wi-Fi"
| `SKY_LOCATION_SOURCE_GNSS`     | "GNSS"
| `SKY_LOCATION_SOURCE_RFPM`     | "RFPM"
| `SKY_LOCATION_SOURCE_UNKNOWN`  | "????"

### sky_close() - frees any resources in use by the Skyhook library

```c
Sky_status_t sky_close(Sky_sctx_t *sctx,
    Sky_errno_t *sky_errno
)

/* Parameters
 * sctx             Skyhook session context
 * sky_errno        sky_errno is set to the error code

 * Returns          `SKY_SUCCESS` or `SKY_ERROR` and sets sky_errno with error code
 */
```

Returns `SKY_SUCCESS` on success, `SKY_ERROR` on error and sets sky_errno appropriately. After a successful close, the
session context may be copied to non-volatile storage. The size of the buffer may be determined by a call to
sky_sizeof_session_ctx().

sky_close() may report the following error conditions in sky_errno:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_NEVER_OPEN`                          | sky_open() must be called before the current operation can succeed

### Appendix

### API Return Codes

`Sky_status_t` indicates the status of a completed API request:

| Return Code                                      | Description
| ------------------------------------------------ | --------------------------------------------------------------
| `SKY_SUCCESS`                                    | Operation was successful
| `SKY_ERROR`                                      | Operation failed with sky_errno set

### API Error Codes

`Sky_errno_t` indicates the error condition:

| Error Code                                      | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_ERROR_NONE`                                | No error
| `SKY_ERROR_NEVER_OPEN`                          | sky_open() must be called before the current operation can succeed
| `SKY_ERROR_ALREADY_OPEN`                        | sky_open() called more than once with different parameters. Call sky_close()
| `SKY_ERROR_BAD_PARAMETERS`                      | The parameters to the current operation are illegal
| `SKY_ERROR_BAD_REQUEST_CTX`                     | The request context structure is corrupt
| `SKY_ERROR_BAD_SESSION_CTX`                     | The session context buffer is corrupt
| `SKY_ERROR_DECODE_ERROR`                        | Could not decode the the response buffer
| `SKY_ERROR_ENCODE_ERROR`                        | Could not encode the request
| `SKY_ERROR_RESOURCE_UNAVAILABLE`                | Could not acquire the necessary resourses
| `SKY_ERROR_NO_BEACONS`                          | Beacons must be added before a request can be finalized
| `SKY_ERROR_LOCATION_UNKNOWN`                    | Server response indicates no location could be determined
| `SKY_ERROR_SERVER_ERROR`                        | Server sent a response which indicated an error processing the request
| `SKY_ERROR_NO_PLUGIN`                           | At least one plugin must be registered
| `SKY_ERROR_INTERNAL`                            | An unexpected error occured
| `SKY_ERROR_SERVICE_DENIED`                      | LibEL temporarily blocked this service after repeated fails to authenticate (TBR only)
| `SKY_ERROR_AUTH`                                | server reported authentication error
| `SKY_AUTH_RETRY`                                | Server indicated that authentication was required and user must retry (TBR only)
| `SKY_AUTH_RETRY_8HR`                            | Server indicated that authentication failed. Service is blocked for 8 hours (TBR only)
| `SKY_AUTH_RETRY_16HR`                           | Server indicated that authentication failed. Service is blocked for 16 hours (TBR only)
| `SKY_AUTH_RETRY_24HR`                           | Server indicated that authentication failed. Service is blocked for 24 hours (TBR only)
| `SKY_AUTH_RETRY_30DAY`                          | Server indicated that authentication failed. Service is blocked for 30 Days (TBR only)

### API Location Result

The library returns location information as `Sky_location_t` from sky_decode_response():

```c
typedef struct sky_location {
    float lat, lon; /* GNSS info */
    uint16_t hpe;
    uint32_t time;
    Sky_loc_source_t location_source;
    Sky_loc_status_t location_status; /* reserved */
    uint8* dl_app_data;
    uint8 dl_data_len;
} Sky_location_t;
```

The location_source field can have one of the following values:

| Location Source                                 | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_LOCATION_SOURCE_HYBRID`                    | A combination of beacon types was used to derive the location
| `SKY_LOCATION_SOURCE_CELL`                      | Cell beacons were used to derive the location
| `SKY_LOCATION_SOURCE_WIFI`                      | Wi-Fi beacons were used to derive the location
| `SKY_LOCATION_SOURCE_GNSS`                      | GNSS info was used to return the location
| `SKY_LOCATION_SOURCE_RFPM`                      | Radio Frequency Pattern Matching was used to derive the location
| `SKY_LOCATION_SOURCE_UNKNOWN`                   | Location source unknown

### API sky_finalize_request() result

sky_finalize_request() returns a result as `Sky_finalize_t`:

| sky_finalize_request result                     | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_FINALIZE_ERROR`                            | Unable to process request context
| `SKY_FINALIZE_LOCATION`                         | The location is known e.g. stationary
| `SKY_FINALIZE_REQUEST`                          | Context contains a server request

### API Logging levels

The level at which the library generates log messages can be configured by passing `min_level` with one of these values
to `sky_open()`:

| Logging Level                                   | Description
| ----------------------------------------------- | --------------------------------------------------------------
| `SKY_LOG_LEVEL_CRITICAL`                        | Information about an error which caused library to immediately close
| `SKY_LOG_LEVEL_ERROR`                           | Information about an operation which failed
| `SKY_LOG_LEVEL_WARNING`                         | Information about an operation which is continuing with unexpected condition
| `SKY_LOG_LEVEL_DEBUG`                           | Verbose information about the operation being performed

**Note** the above table is in decending severity order

Setting min_level blocks less severe log messages. A level of `SKY_LOG_LEVEL_DEBUG` will include all
logging, `SKY_LOG_LEVEL_ERROR` will include error and critical log messages.

License
-------

This project is licensed under the MIT license.
See [LICENSE.txt](https://github.com/SkyhookWireless/embedded-client/blob/master/LICENSE.txt?raw=true).
