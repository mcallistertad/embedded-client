/* Automatically generated nanopb header */
/* Generated by nanopb-0.4.0-dev */

#ifndef PB_EL_PB_H_INCLUDED
#define PB_EL_PB_H_INCLUDED
#include <pb.h>

/* @@protoc_insertion_point(includes) */
#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Enum definitions */
typedef enum _RsHeader_Status {
    RsHeader_Status_SUCCESS = 0,
    RsHeader_Status_UNSPECIFIED_ERROR = 1,
    RsHeader_Status_BAD_PARTNER_ID_ERROR = 2,
    RsHeader_Status_DECODE_FAILURE_ERROR = 3,
    RsHeader_Status_API_SERVER_ERROR = 4,
    RsHeader_Status_AUTH_ERROR = 5,
    RsHeader_Status_UNABLE_TO_LOCATE = 6
} RsHeader_Status;
#define _RsHeader_Status_MIN RsHeader_Status_SUCCESS
#define _RsHeader_Status_MAX RsHeader_Status_UNABLE_TO_LOCATE
#define _RsHeader_Status_ARRAYSIZE ((RsHeader_Status)(RsHeader_Status_UNABLE_TO_LOCATE+1))

typedef enum _Cell_Type {
    Cell_Type_UNKNOWN = 0,
    Cell_Type_CDMA = 3,
    Cell_Type_GSM = 4,
    Cell_Type_LTE = 5,
    Cell_Type_NBIOT = 6,
    Cell_Type_UMTS = 7,
    Cell_Type_NR = 8
} Cell_Type;
#define _Cell_Type_MIN Cell_Type_UNKNOWN
#define _Cell_Type_MAX Cell_Type_NR
#define _Cell_Type_ARRAYSIZE ((Cell_Type)(Cell_Type_NR+1))

typedef enum _Rs_Source {
    Rs_Source_UNKNOWN = 0,
    Rs_Source_HYBRID = 1,
    Rs_Source_CELL = 2,
    Rs_Source_WIFI = 3,
    Rs_Source_GNSS = 4
} Rs_Source;
#define _Rs_Source_MIN Rs_Source_UNKNOWN
#define _Rs_Source_MAX Rs_Source_GNSS
#define _Rs_Source_ARRAYSIZE ((Rs_Source)(Rs_Source_GNSS+1))

/* Struct definitions */
typedef struct _Gnss {
    pb_callback_t lat;
    pb_callback_t lon;
    pb_callback_t hpe;
    pb_callback_t alt;
    pb_callback_t vpe;
    pb_callback_t speed;
    pb_callback_t bearing;
    pb_callback_t nsat;
    pb_callback_t age;
/* @@protoc_insertion_point(struct:Gnss) */
} Gnss;


typedef struct _Aps {
    uint32_t connected_idx_plus_1;
    uint32_t common_freq_plus_1;
    uint32_t common_age_plus_1;
    pb_callback_t mac;
    pb_callback_t frequency;
    pb_callback_t neg_rssi;
    pb_callback_t age;
/* @@protoc_insertion_point(struct:Aps) */
} Aps;


typedef struct _CdmaCells {
    uint32_t connected_idx_plus_1;
    uint32_t common_age_plus_1;
    pb_callback_t sid;
    pb_callback_t nid;
    pb_callback_t bsid;
    pb_callback_t neg_rssi;
    pb_callback_t age;
/* @@protoc_insertion_point(struct:CdmaCells) */
} CdmaCells;


typedef struct _Cell {
    Cell_Type type;
    uint32_t id1_plus_1;
    uint32_t id2_plus_1;
    uint32_t id3_plus_1;
    uint64_t id4_plus_1;
    uint32_t id5_plus_1;
    uint32_t id6_plus_1;
    bool connected;
    uint32_t neg_rssi;
    uint32_t age;
    uint32_t ta_plus_1;
/* @@protoc_insertion_point(struct:Cell) */
} Cell;


typedef struct _ClientConfig {
    uint32_t total_beacons;
    uint32_t max_ap_beacons;
    uint32_t cache_match_threshold;
    uint32_t cache_age_threshold;
    uint32_t cache_beacon_threshold;
    uint32_t cache_neg_rssi_threshold;
    uint32_t cache_match_all_threshold;
    uint32_t cache_match_used_threshold;
    uint32_t max_vap_per_ap;
    uint32_t max_vap_per_rq;
/* @@protoc_insertion_point(struct:ClientConfig) */
} ClientConfig;


typedef PB_BYTES_ARRAY_T(16) CryptoInfo_iv_t;
typedef struct _CryptoInfo {
    CryptoInfo_iv_t iv;
    int32_t aes_padding_length;
/* @@protoc_insertion_point(struct:CryptoInfo) */
} CryptoInfo;


typedef struct _GsmCells {
    uint32_t connected_idx_plus_1;
    uint32_t common_age_plus_1;
    pb_callback_t mcc;
    pb_callback_t mnc;
    pb_callback_t lac;
    pb_callback_t ci;
    pb_callback_t neg_rssi;
    pb_callback_t age;
/* @@protoc_insertion_point(struct:GsmCells) */
} GsmCells;


typedef struct _LteCells {
    uint32_t connected_idx_plus_1;
    uint32_t common_age_plus_1;
    pb_callback_t mcc;
    pb_callback_t mnc;
    pb_callback_t tac;
    pb_callback_t eucid;
    pb_callback_t neg_rssi;
    pb_callback_t age;
/* @@protoc_insertion_point(struct:LteCells) */
} LteCells;


typedef struct _NbiotCells {
    uint32_t connected_idx_plus_1;
    uint32_t common_age_plus_1;
    pb_callback_t mcc;
    pb_callback_t mnc;
    pb_callback_t tac;
    pb_callback_t e_cellid;
    pb_callback_t neg_rssi;
    pb_callback_t age;
/* @@protoc_insertion_point(struct:NbiotCells) */
} NbiotCells;


typedef struct _RqHeader {
    int32_t partner_id;
    int32_t crypto_info_length;
    int32_t rq_length;
    uint32_t sw_version;
    bool request_client_conf;
/* @@protoc_insertion_point(struct:RqHeader) */
} RqHeader;


typedef struct _RsHeader {
    int32_t crypto_info_length;
    int32_t rs_length;
    RsHeader_Status status;
/* @@protoc_insertion_point(struct:RsHeader) */
} RsHeader;


typedef struct _TbrRegistration {
    char sku[33];
    int32_t cc;
/* @@protoc_insertion_point(struct:TbrRegistration) */
} TbrRegistration;


typedef struct _UmtsCells {
    uint32_t connected_idx_plus_1;
    uint32_t common_age_plus_1;
    pb_callback_t mcc;
    pb_callback_t mnc;
    pb_callback_t lac;
    pb_callback_t ucid;
    pb_callback_t neg_rssi;
    pb_callback_t age;
/* @@protoc_insertion_point(struct:UmtsCells) */
} UmtsCells;


typedef PB_BYTES_ARRAY_T(16) Rq_device_id_t;
typedef PB_BYTES_ARRAY_T(100) Rq_ul_app_data_t;
typedef struct _Rq {
    Rq_device_id_t device_id;
    uint64_t timestamp;
    void* aps;
    GsmCells gsm_cells;
    UmtsCells umts_cells;
    LteCells lte_cells;
    CdmaCells cdma_cells;
    NbiotCells nbiot_cells;
    void* gnss;
    void* cells;
    void* vaps;
    uint32_t cache_hits;
    uint32_t score;
    uint32_t score_threshold;
    pb_callback_t ctx;
    pb_callback_t cache;
    int32_t token_id;
    TbrRegistration tbr;
    int32_t max_dl_app_data;
    Rq_ul_app_data_t ul_app_data;
/* @@protoc_insertion_point(struct:Rq) */
} Rq;


typedef PB_BYTES_ARRAY_T(8) Rs_used_aps_t;
typedef PB_BYTES_ARRAY_T(100) Rs_dl_app_data_t;
typedef struct _Rs {
    double lat;
    double lon;
    uint32_t hpe;
    Rs_Source source;
    ClientConfig config;
    Rs_used_aps_t used_aps;
    int32_t token_id;
    Rs_dl_app_data_t dl_app_data;
/* @@protoc_insertion_point(struct:Rs) */
} Rs;


/* Initializer values for message structs */
#define RqHeader_init_default                    {0, 0, 0, 0, 0}
#define RsHeader_init_default                    {0, 0, _RsHeader_Status_MIN}
#define CryptoInfo_init_default                  {{0, {0}}, 0}
#define Rq_init_default                          {{0, {0}}, 0, {{NULL}, NULL}, GsmCells_init_default, UmtsCells_init_default, LteCells_init_default, CdmaCells_init_default, NbiotCells_init_default, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, 0, 0, 0, {{NULL}, NULL}, {{NULL}, NULL}, 0, TbrRegistration_init_default, 0, {0, {0}}}
#define TbrRegistration_init_default             {"", 0}
#define Aps_init_default                         {0, 0, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define Cell_init_default                        {_Cell_Type_MIN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
#define GsmCells_init_default                    {0, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define UmtsCells_init_default                   {0, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define CdmaCells_init_default                   {0, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define LteCells_init_default                    {0, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define NbiotCells_init_default                  {0, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define Gnss_init_default                        {{{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define Rs_init_default                          {0, 0, 0, _Rs_Source_MIN, ClientConfig_init_default, {0, {0}}, 0, {0, {0}}}
#define ClientConfig_init_default                {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
#define RqHeader_init_zero                       {0, 0, 0, 0, 0}
#define RsHeader_init_zero                       {0, 0, _RsHeader_Status_MIN}
#define CryptoInfo_init_zero                     {{0, {0}}, 0}
#define Rq_init_zero                             {{0, {0}}, 0, {{NULL}, NULL}, GsmCells_init_zero, UmtsCells_init_zero, LteCells_init_zero, CdmaCells_init_zero, NbiotCells_init_zero, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, 0, 0, 0, {{NULL}, NULL}, {{NULL}, NULL}, 0, TbrRegistration_init_zero, 0, {0, {0}}}
#define TbrRegistration_init_zero                {"", 0}
#define Aps_init_zero                            {0, 0, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define Cell_init_zero                           {_Cell_Type_MIN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
#define GsmCells_init_zero                       {0, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define UmtsCells_init_zero                      {0, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define CdmaCells_init_zero                      {0, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define LteCells_init_zero                       {0, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define NbiotCells_init_zero                     {0, 0, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define Gnss_init_zero                           {{{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}, {{NULL}, NULL}}
#define Rs_init_zero                             {0, 0, 0, _Rs_Source_MIN, ClientConfig_init_zero, {0, {0}}, 0, {0, {0}}}
#define ClientConfig_init_zero                   {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

/* Field tags (for use in manual encoding/decoding) */
#define Gnss_lat_tag                             1
#define Gnss_lon_tag                             2
#define Gnss_hpe_tag                             3
#define Gnss_alt_tag                             4
#define Gnss_vpe_tag                             5
#define Gnss_speed_tag                           6
#define Gnss_bearing_tag                         7
#define Gnss_nsat_tag                            8
#define Gnss_age_tag                             9
#define Aps_connected_idx_plus_1_tag             1
#define Aps_common_freq_plus_1_tag               2
#define Aps_common_age_plus_1_tag                3
#define Aps_mac_tag                              4
#define Aps_frequency_tag                        5
#define Aps_neg_rssi_tag                         6
#define Aps_age_tag                              7
#define CdmaCells_connected_idx_plus_1_tag       1
#define CdmaCells_common_age_plus_1_tag          2
#define CdmaCells_sid_tag                        3
#define CdmaCells_nid_tag                        4
#define CdmaCells_bsid_tag                       5
#define CdmaCells_neg_rssi_tag                   6
#define CdmaCells_age_tag                        7
#define Cell_type_tag                            1
#define Cell_id1_plus_1_tag                      2
#define Cell_id2_plus_1_tag                      3
#define Cell_id3_plus_1_tag                      4
#define Cell_id4_plus_1_tag                      5
#define Cell_id5_plus_1_tag                      6
#define Cell_id6_plus_1_tag                      7
#define Cell_connected_tag                       8
#define Cell_neg_rssi_tag                        9
#define Cell_age_tag                             10
#define Cell_ta_plus_1_tag                       11
#define ClientConfig_total_beacons_tag           1
#define ClientConfig_max_ap_beacons_tag          2
#define ClientConfig_cache_match_threshold_tag   3
#define ClientConfig_cache_age_threshold_tag     4
#define ClientConfig_cache_beacon_threshold_tag  5
#define ClientConfig_cache_neg_rssi_threshold_tag 6
#define ClientConfig_cache_match_all_threshold_tag 7
#define ClientConfig_cache_match_used_threshold_tag 8
#define ClientConfig_max_vap_per_ap_tag          9
#define ClientConfig_max_vap_per_rq_tag          10
#define CryptoInfo_iv_tag                        1
#define CryptoInfo_aes_padding_length_tag        2
#define GsmCells_connected_idx_plus_1_tag        1
#define GsmCells_common_age_plus_1_tag           2
#define GsmCells_mcc_tag                         3
#define GsmCells_mnc_tag                         4
#define GsmCells_lac_tag                         5
#define GsmCells_ci_tag                          6
#define GsmCells_neg_rssi_tag                    7
#define GsmCells_age_tag                         8
#define LteCells_connected_idx_plus_1_tag        1
#define LteCells_common_age_plus_1_tag           2
#define LteCells_mcc_tag                         3
#define LteCells_mnc_tag                         4
#define LteCells_tac_tag                         5
#define LteCells_eucid_tag                       6
#define LteCells_neg_rssi_tag                    7
#define LteCells_age_tag                         8
#define NbiotCells_connected_idx_plus_1_tag      1
#define NbiotCells_common_age_plus_1_tag         2
#define NbiotCells_mcc_tag                       3
#define NbiotCells_mnc_tag                       4
#define NbiotCells_tac_tag                       5
#define NbiotCells_e_cellid_tag                  6
#define NbiotCells_neg_rssi_tag                  7
#define NbiotCells_age_tag                       8
#define RqHeader_partner_id_tag                  1
#define RqHeader_crypto_info_length_tag          2
#define RqHeader_rq_length_tag                   3
#define RqHeader_sw_version_tag                  4
#define RqHeader_request_client_conf_tag         5
#define RsHeader_crypto_info_length_tag          1
#define RsHeader_rs_length_tag                   2
#define RsHeader_status_tag                      3
#define TbrRegistration_sku_tag                  3
#define TbrRegistration_cc_tag                   4
#define UmtsCells_connected_idx_plus_1_tag       1
#define UmtsCells_common_age_plus_1_tag          2
#define UmtsCells_mcc_tag                        3
#define UmtsCells_mnc_tag                        4
#define UmtsCells_lac_tag                        5
#define UmtsCells_ucid_tag                       6
#define UmtsCells_neg_rssi_tag                   7
#define UmtsCells_age_tag                        8
#define Rq_device_id_tag                         1
#define Rq_timestamp_tag                         2
#define Rq_aps_tag                               3
#define Rq_gsm_cells_tag                         4
#define Rq_umts_cells_tag                        5
#define Rq_lte_cells_tag                         6
#define Rq_cdma_cells_tag                        7
#define Rq_nbiot_cells_tag                       8
#define Rq_gnss_tag                              9
#define Rq_cells_tag                             10
#define Rq_vaps_tag                              11
#define Rq_cache_hits_tag                        12
#define Rq_score_tag                             13
#define Rq_score_threshold_tag                   14
#define Rq_ctx_tag                               15
#define Rq_cache_tag                             16
#define Rq_token_id_tag                          17
#define Rq_tbr_tag                               18
#define Rq_max_dl_app_data_tag                   19
#define Rq_ul_app_data_tag                       20
#define Rs_lat_tag                               1
#define Rs_lon_tag                               2
#define Rs_hpe_tag                               3
#define Rs_source_tag                            4
#define Rs_config_tag                            5
#define Rs_used_aps_tag                          6
#define Rs_token_id_tag                          7
#define Rs_dl_app_data_tag                       8

/* Struct field encoding specification for nanopb */
#define RqHeader_FIELDLIST(X, a) \
X(a, STATIC, SINGULAR, INT32, partner_id, 1) \
X(a, STATIC, SINGULAR, INT32, crypto_info_length, 2) \
X(a, STATIC, SINGULAR, INT32, rq_length, 3) \
X(a, STATIC, SINGULAR, UINT32, sw_version, 4) \
X(a, STATIC, SINGULAR, BOOL, request_client_conf, 5)
#define RqHeader_CALLBACK NULL
#define RqHeader_DEFAULT NULL

#define RsHeader_FIELDLIST(X, a) \
X(a, STATIC, SINGULAR, INT32, crypto_info_length, 1) \
X(a, STATIC, SINGULAR, INT32, rs_length, 2) \
X(a, STATIC, SINGULAR, UENUM, status, 3)
#define RsHeader_CALLBACK NULL
#define RsHeader_DEFAULT NULL

#define CryptoInfo_FIELDLIST(X, a) \
X(a, STATIC, SINGULAR, BYTES, iv, 1) \
X(a, STATIC, SINGULAR, INT32, aes_padding_length, 2)
#define CryptoInfo_CALLBACK NULL
#define CryptoInfo_DEFAULT NULL

#define Rq_FIELDLIST(X, a) \
X(a, STATIC, SINGULAR, BYTES, device_id, 1) \
X(a, STATIC, SINGULAR, UINT64, timestamp, 2) \
X(a, CALLBACK, SINGULAR, MESSAGE, aps, 3) \
X(a, STATIC, SINGULAR, MESSAGE, gsm_cells, 4) \
X(a, STATIC, SINGULAR, MESSAGE, umts_cells, 5) \
X(a, STATIC, SINGULAR, MESSAGE, lte_cells, 6) \
X(a, STATIC, SINGULAR, MESSAGE, cdma_cells, 7) \
X(a, STATIC, SINGULAR, MESSAGE, nbiot_cells, 8) \
X(a, CALLBACK, SINGULAR, MESSAGE, gnss, 9) \
X(a, CALLBACK, REPEATED, MESSAGE, cells, 10) \
X(a, CALLBACK, SINGULAR, BYTES, vaps, 11) \
X(a, STATIC, SINGULAR, UINT32, cache_hits, 12) \
X(a, STATIC, SINGULAR, UINT32, score, 13) \
X(a, STATIC, SINGULAR, UINT32, score_threshold, 14) \
X(a, CALLBACK, SINGULAR, BYTES, ctx, 15) \
X(a, CALLBACK, SINGULAR, BYTES, cache, 16) \
X(a, STATIC, SINGULAR, INT32, token_id, 17) \
X(a, STATIC, SINGULAR, MESSAGE, tbr, 18) \
X(a, STATIC, SINGULAR, INT32, max_dl_app_data, 19) \
X(a, STATIC, SINGULAR, BYTES, ul_app_data, 20)
extern bool Rq_callback(pb_istream_t *istream, pb_ostream_t *ostream, const pb_field_t *field);
#define Rq_CALLBACK Rq_callback
#define Rq_DEFAULT NULL
#define Rq_aps_MSGTYPE Aps
#define Rq_gsm_cells_MSGTYPE GsmCells
#define Rq_umts_cells_MSGTYPE UmtsCells
#define Rq_lte_cells_MSGTYPE LteCells
#define Rq_cdma_cells_MSGTYPE CdmaCells
#define Rq_nbiot_cells_MSGTYPE NbiotCells
#define Rq_gnss_MSGTYPE Gnss
#define Rq_cells_MSGTYPE Cell
#define Rq_tbr_MSGTYPE TbrRegistration

#define TbrRegistration_FIELDLIST(X, a) \
X(a, STATIC, SINGULAR, STRING, sku, 3) \
X(a, STATIC, SINGULAR, INT32, cc, 4)
#define TbrRegistration_CALLBACK NULL
#define TbrRegistration_DEFAULT NULL

#define Aps_FIELDLIST(X, a) \
X(a, STATIC, SINGULAR, UINT32, connected_idx_plus_1, 1) \
X(a, STATIC, SINGULAR, UINT32, common_freq_plus_1, 2) \
X(a, STATIC, SINGULAR, UINT32, common_age_plus_1, 3) \
X(a, CALLBACK, REPEATED, INT64, mac, 4) \
X(a, CALLBACK, REPEATED, INT32, frequency, 5) \
X(a, CALLBACK, REPEATED, UINT32, neg_rssi, 6) \
X(a, CALLBACK, REPEATED, UINT32, age, 7)
#define Aps_CALLBACK pb_default_field_callback
#define Aps_DEFAULT NULL

#define Cell_FIELDLIST(X, a) \
X(a, STATIC, SINGULAR, UENUM, type, 1) \
X(a, STATIC, SINGULAR, UINT32, id1_plus_1, 2) \
X(a, STATIC, SINGULAR, UINT32, id2_plus_1, 3) \
X(a, STATIC, SINGULAR, UINT32, id3_plus_1, 4) \
X(a, STATIC, SINGULAR, UINT64, id4_plus_1, 5) \
X(a, STATIC, SINGULAR, UINT32, id5_plus_1, 6) \
X(a, STATIC, SINGULAR, UINT32, id6_plus_1, 7) \
X(a, STATIC, SINGULAR, BOOL, connected, 8) \
X(a, STATIC, SINGULAR, UINT32, neg_rssi, 9) \
X(a, STATIC, SINGULAR, UINT32, age, 10) \
X(a, STATIC, SINGULAR, UINT32, ta_plus_1, 11)
#define Cell_CALLBACK NULL
#define Cell_DEFAULT NULL

#define GsmCells_FIELDLIST(X, a) \
X(a, STATIC, SINGULAR, UINT32, connected_idx_plus_1, 1) \
X(a, STATIC, SINGULAR, UINT32, common_age_plus_1, 2) \
X(a, CALLBACK, REPEATED, UINT32, mcc, 3) \
X(a, CALLBACK, REPEATED, UINT32, mnc, 4) \
X(a, CALLBACK, REPEATED, UINT32, lac, 5) \
X(a, CALLBACK, REPEATED, UINT32, ci, 6) \
X(a, CALLBACK, REPEATED, UINT32, neg_rssi, 7) \
X(a, CALLBACK, REPEATED, UINT32, age, 8)
#define GsmCells_CALLBACK pb_default_field_callback
#define GsmCells_DEFAULT NULL

#define UmtsCells_FIELDLIST(X, a) \
X(a, STATIC, SINGULAR, UINT32, connected_idx_plus_1, 1) \
X(a, STATIC, SINGULAR, UINT32, common_age_plus_1, 2) \
X(a, CALLBACK, REPEATED, UINT32, mcc, 3) \
X(a, CALLBACK, REPEATED, UINT32, mnc, 4) \
X(a, CALLBACK, REPEATED, UINT32, lac, 5) \
X(a, CALLBACK, REPEATED, UINT32, ucid, 6) \
X(a, CALLBACK, REPEATED, UINT32, neg_rssi, 7) \
X(a, CALLBACK, REPEATED, UINT32, age, 8)
#define UmtsCells_CALLBACK pb_default_field_callback
#define UmtsCells_DEFAULT NULL

#define CdmaCells_FIELDLIST(X, a) \
X(a, STATIC, SINGULAR, UINT32, connected_idx_plus_1, 1) \
X(a, STATIC, SINGULAR, UINT32, common_age_plus_1, 2) \
X(a, CALLBACK, REPEATED, UINT32, sid, 3) \
X(a, CALLBACK, REPEATED, UINT32, nid, 4) \
X(a, CALLBACK, REPEATED, UINT32, bsid, 5) \
X(a, CALLBACK, REPEATED, UINT32, neg_rssi, 6) \
X(a, CALLBACK, REPEATED, UINT32, age, 7)
#define CdmaCells_CALLBACK pb_default_field_callback
#define CdmaCells_DEFAULT NULL

#define LteCells_FIELDLIST(X, a) \
X(a, STATIC, SINGULAR, UINT32, connected_idx_plus_1, 1) \
X(a, STATIC, SINGULAR, UINT32, common_age_plus_1, 2) \
X(a, CALLBACK, REPEATED, UINT32, mcc, 3) \
X(a, CALLBACK, REPEATED, UINT32, mnc, 4) \
X(a, CALLBACK, REPEATED, UINT32, tac, 5) \
X(a, CALLBACK, REPEATED, UINT32, eucid, 6) \
X(a, CALLBACK, REPEATED, UINT32, neg_rssi, 7) \
X(a, CALLBACK, REPEATED, UINT32, age, 8)
#define LteCells_CALLBACK pb_default_field_callback
#define LteCells_DEFAULT NULL

#define NbiotCells_FIELDLIST(X, a) \
X(a, STATIC, SINGULAR, UINT32, connected_idx_plus_1, 1) \
X(a, STATIC, SINGULAR, UINT32, common_age_plus_1, 2) \
X(a, CALLBACK, REPEATED, UINT32, mcc, 3) \
X(a, CALLBACK, REPEATED, UINT32, mnc, 4) \
X(a, CALLBACK, REPEATED, UINT32, tac, 5) \
X(a, CALLBACK, REPEATED, UINT32, e_cellid, 6) \
X(a, CALLBACK, REPEATED, UINT32, neg_rssi, 7) \
X(a, CALLBACK, REPEATED, UINT32, age, 8)
#define NbiotCells_CALLBACK pb_default_field_callback
#define NbiotCells_DEFAULT NULL

#define Gnss_FIELDLIST(X, a) \
X(a, CALLBACK, REPEATED, INT32, lat, 1) \
X(a, CALLBACK, REPEATED, INT32, lon, 2) \
X(a, CALLBACK, REPEATED, UINT32, hpe, 3) \
X(a, CALLBACK, REPEATED, INT32, alt, 4) \
X(a, CALLBACK, REPEATED, UINT32, vpe, 5) \
X(a, CALLBACK, REPEATED, UINT32, speed, 6) \
X(a, CALLBACK, REPEATED, UINT32, bearing, 7) \
X(a, CALLBACK, REPEATED, UINT32, nsat, 8) \
X(a, CALLBACK, REPEATED, UINT32, age, 9)
#define Gnss_CALLBACK pb_default_field_callback
#define Gnss_DEFAULT NULL

#define Rs_FIELDLIST(X, a) \
X(a, STATIC, SINGULAR, DOUBLE, lat, 1) \
X(a, STATIC, SINGULAR, DOUBLE, lon, 2) \
X(a, STATIC, SINGULAR, UINT32, hpe, 3) \
X(a, STATIC, SINGULAR, UENUM, source, 4) \
X(a, STATIC, SINGULAR, MESSAGE, config, 5) \
X(a, STATIC, SINGULAR, BYTES, used_aps, 6) \
X(a, STATIC, SINGULAR, INT32, token_id, 7) \
X(a, STATIC, SINGULAR, BYTES, dl_app_data, 8)
#define Rs_CALLBACK NULL
#define Rs_DEFAULT NULL
#define Rs_config_MSGTYPE ClientConfig

#define ClientConfig_FIELDLIST(X, a) \
X(a, STATIC, SINGULAR, UINT32, total_beacons, 1) \
X(a, STATIC, SINGULAR, UINT32, max_ap_beacons, 2) \
X(a, STATIC, SINGULAR, UINT32, cache_match_threshold, 3) \
X(a, STATIC, SINGULAR, UINT32, cache_age_threshold, 4) \
X(a, STATIC, SINGULAR, UINT32, cache_beacon_threshold, 5) \
X(a, STATIC, SINGULAR, UINT32, cache_neg_rssi_threshold, 6) \
X(a, STATIC, SINGULAR, UINT32, cache_match_all_threshold, 7) \
X(a, STATIC, SINGULAR, UINT32, cache_match_used_threshold, 8) \
X(a, STATIC, SINGULAR, UINT32, max_vap_per_ap, 9) \
X(a, STATIC, SINGULAR, UINT32, max_vap_per_rq, 10)
#define ClientConfig_CALLBACK NULL
#define ClientConfig_DEFAULT NULL

extern const pb_msgdesc_t RqHeader_msg;
extern const pb_msgdesc_t RsHeader_msg;
extern const pb_msgdesc_t CryptoInfo_msg;
extern const pb_msgdesc_t Rq_msg;
extern const pb_msgdesc_t TbrRegistration_msg;
extern const pb_msgdesc_t Aps_msg;
extern const pb_msgdesc_t Cell_msg;
extern const pb_msgdesc_t GsmCells_msg;
extern const pb_msgdesc_t UmtsCells_msg;
extern const pb_msgdesc_t CdmaCells_msg;
extern const pb_msgdesc_t LteCells_msg;
extern const pb_msgdesc_t NbiotCells_msg;
extern const pb_msgdesc_t Gnss_msg;
extern const pb_msgdesc_t Rs_msg;
extern const pb_msgdesc_t ClientConfig_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define RqHeader_fields &RqHeader_msg
#define RsHeader_fields &RsHeader_msg
#define CryptoInfo_fields &CryptoInfo_msg
#define Rq_fields &Rq_msg
#define TbrRegistration_fields &TbrRegistration_msg
#define Aps_fields &Aps_msg
#define Cell_fields &Cell_msg
#define GsmCells_fields &GsmCells_msg
#define UmtsCells_fields &UmtsCells_msg
#define CdmaCells_fields &CdmaCells_msg
#define LteCells_fields &LteCells_msg
#define NbiotCells_fields &NbiotCells_msg
#define Gnss_fields &Gnss_msg
#define Rs_fields &Rs_msg
#define ClientConfig_fields &ClientConfig_msg

/* Maximum encoded size of messages (where known) */
#define RqHeader_size                            41
#define RsHeader_size                            24
#define CryptoInfo_size                          29
/* Rq_size depends on runtime parameters */
#define TbrRegistration_size                     45
/* Aps_size depends on runtime parameters */
#define Cell_size                                63
/* GsmCells_size depends on runtime parameters */
/* UmtsCells_size depends on runtime parameters */
/* CdmaCells_size depends on runtime parameters */
/* LteCells_size depends on runtime parameters */
/* NbiotCells_size depends on runtime parameters */
/* Gnss_size depends on runtime parameters */
#define Rs_size                                  211
#define ClientConfig_size                        60

#ifdef __cplusplus
} /* extern "C" */
#endif
/* @@protoc_insertion_point(eof) */

#endif
