/**
 * @file bluetooth.c
 * @brief Classic Bluetooth A2DP source: stack bring-up, name-based discovery and connection.
 * @ingroup services_bluetooth
 */
#include "bluetooth.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>
#include <stdint.h>

static const char *TAG = "bluetooth";

#define BT_DEVICE_NAME   "TB2026-YDS"
#define BT_INQ_LEN       10              /* inquiry window: 10 * 1.28 s ~= 12.8 s */

static bool               s_inited;
static bool               s_scanning;    /* an inquiry is running, collecting into s_devices */
static bool               s_seeking;     /* TEST ONLY: an inquiry is running for connect_by_name */
static char               s_target[ESP_BT_GAP_MAX_BDNAME_LEN + 1];

/* Scan results, filled by the GAP callback (BT task) and read by the UI task; s_lock guards
   both s_devices and s_device_count. */
static SemaphoreHandle_t  s_lock;
static bluetooth_device_t s_devices[BLUETOOTH_MAX_DEVICES];
static size_t             s_device_count;
static esp_bd_addr_t      s_peer_bda;
static volatile bluetooth_conn_state_t s_conn_state;  /* coarse state for the UI; see bluetooth.h */
static bt_known_device_t  s_last;        /* last connected device; guarded by s_lock */
static bool               s_last_valid;  /* a device has been remembered (this session or seeded) */
static esp_a2d_conn_hdl_t s_conn_hdl;    /* valid while connected; consumed by the BT sink */
static uint16_t           s_audio_mtu;   /* negotiated audio MTU; caps the encoded send size */
static bool               s_avrc_connected; /* AVRCP control channel up (separate from A2DP) */
static uint8_t            s_avrc_vol;       /* last requested absolute volume, 0..0x7F */
static bool              s_avrc_have_vol;   /* a volume was requested at least once */
static volatile bool     s_vol_acked;       /* the most recent set was confirmed by the sink */

/* Rolling AVRCP transaction label (0..15); consecutive commands should use different values. */
static uint8_t next_tl(void)
{
    static uint8_t tl;
    tl = (tl + 1) & ESP_AVRC_TRANS_LABEL_MAX;
    return tl;
}

/* Pull the advertised device name out of an inquiry result: the explicit BDNAME property if
   present, otherwise the complete/short local name carried in the EIR. */
static bool name_from_disc(struct disc_res_param *res, char *out, size_t cap)
{
    uint8_t *eir = NULL;
    for (int i = 0; i < res->num_prop; i++) {
        esp_bt_gap_dev_prop_t *p = &res->prop[i];
        if (p->type == ESP_BT_GAP_DEV_PROP_BDNAME) {
            int len = p->len < (int)cap - 1 ? p->len : (int)cap - 1;
            memcpy(out, p->val, len);
            out[len] = '\0';
            return true;
        }
        if (p->type == ESP_BT_GAP_DEV_PROP_EIR) {
            eir = (uint8_t *)p->val;
        }
    }
    if (eir) {
        uint8_t len = 0;
        uint8_t *name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);
        if (!name) {
            name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &len);
        }
        if (name) {
            if (len > cap - 1) len = cap - 1;
            memcpy(out, name, len);
            out[len] = '\0';
            return true;
        }
    }
    return false;
}

/* Insert or update a scan result, keyed by address: the inquiry reports a device several times
   and its name often arrives after the first (address-only) hit, so refresh an existing entry
   rather than duplicate it. Runs in the BT task; takes s_lock around the shared list. */
static void scan_add(struct disc_res_param *res)
{
    char name[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
    bool have_name = name_from_disc(res, name, sizeof name);
    int8_t rssi = INT8_MIN;
    for (int i = 0; i < res->num_prop; i++) {
        if (res->prop[i].type == ESP_BT_GAP_DEV_PROP_RSSI) {
            rssi = *(int8_t *)res->prop[i].val;
        }
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    bluetooth_device_t *d = NULL;
    for (size_t i = 0; i < s_device_count; i++) {
        if (memcmp(s_devices[i].bda, res->bda, ESP_BD_ADDR_LEN) == 0) {
            d = &s_devices[i];
            break;
        }
    }
    if (!d && s_device_count < BLUETOOTH_MAX_DEVICES) {
        d = &s_devices[s_device_count++];
        memcpy(d->bda, res->bda, ESP_BD_ADDR_LEN);
        d->name[0] = '\0';
        d->rssi = INT8_MIN;
    }
    if (d) {
        if (have_name) strlcpy(d->name, name, sizeof d->name);
        if (rssi != INT8_MIN) d->rssi = rssi;
    }
    xSemaphoreGive(s_lock);
}

static void gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT:
        if (s_seeking) {
            /* TEST ONLY (connect_by_name): connect to the first name match. */
            char name[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
            if (name_from_disc(&param->disc_res, name, sizeof name) && strstr(name, s_target)) {
                ESP_LOGI(TAG, "found '%s', connecting", name);
                memcpy(s_peer_bda, param->disc_res.bda, ESP_BD_ADDR_LEN);
                s_seeking = false;
                esp_bt_gap_cancel_discovery();
                if (esp_a2d_source_connect(s_peer_bda) == ESP_OK) {
                    s_conn_state = BLUETOOTH_CONN_CONNECTING;
                }
            }
        } else if (s_scanning) {
            scan_add(&param->disc_res);
        }
        break;
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
        /* Track the live inquiry state so bluetooth_is_scanning() stays accurate when the
           inquiry window ends on its own. */
        s_scanning = (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED);
        ESP_LOGI(TAG, "discovery %s",
                 param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED ? "stopped" : "started");
        break;
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "paired with %s", param->auth_cmpl.device_name);
        } else {
            ESP_LOGE(TAG, "pairing failed (status %d)", param->auth_cmpl.stat);
        }
        break;
    case ESP_BT_GAP_CFM_REQ_EVT:
        /* Just-works pairing: accept the numeric comparison automatically. */
        ESP_LOGI(TAG, "SSP confirm %lu -> accept", (unsigned long)param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    case ESP_BT_GAP_PIN_REQ_EVT: {
        /* Legacy pairing fallback: reply with the usual default PIN. */
        esp_bt_pin_code_t pin = { '0', '0', '0', '0' };
        esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin);
        break;
    }
    default:
        break;
    }
}

/* Record the peer we just connected to as the device to offer for quick reconnect. Resolve its
   name from the scan list when present; keep the previously known name on a reconnect to the same
   address (a scan-less reconnect has an empty list). Runs in the BT task; takes s_lock. */
static void remember_peer(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool same = s_last_valid && memcmp(s_last.bda, s_peer_bda, ESP_BD_ADDR_LEN) == 0;
    memcpy(s_last.bda, s_peer_bda, ESP_BD_ADDR_LEN);
    if (!same) s_last.name[0] = '\0';
    for (size_t i = 0; i < s_device_count; i++) {
        if (memcmp(s_devices[i].bda, s_peer_bda, ESP_BD_ADDR_LEN) == 0) {
            strlcpy(s_last.name, s_devices[i].name, sizeof s_last.name);
            break;
        }
    }
    s_last_valid = true;
    xSemaphoreGive(s_lock);
}

static void a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        switch (param->conn_stat.state) {
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            s_conn_state = BLUETOOTH_CONN_CONNECTED;
            s_conn_hdl = param->conn_stat.conn_hdl;
            s_audio_mtu = param->conn_stat.audio_mtu;
            remember_peer();
            ESP_LOGI(TAG, "A2DP connected (audio MTU %u)", s_audio_mtu);
            break;
        case ESP_A2D_CONNECTION_STATE_CONNECTING:
            s_conn_state = BLUETOOTH_CONN_CONNECTING;
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            /* An abnormal reason means the attempt failed or the link dropped unexpectedly: keep
               it visible as FAILED. A normal close (user disconnect) returns to IDLE. */
            s_conn_state = (param->conn_stat.disc_rsn == ESP_A2D_DISC_RSN_ABNORMAL)
                               ? BLUETOOTH_CONN_FAILED : BLUETOOTH_CONN_IDLE;
            ESP_LOGW(TAG, "A2DP disconnected (reason %d)", param->conn_stat.disc_rsn);
            break;
        default:  /* DISCONNECTING: keep current state until DISCONNECTED arrives. */
            ESP_LOGI(TAG, "A2DP connection state %d", param->conn_stat.state);
            break;
        }
        break;
    case ESP_A2D_AUDIO_STATE_EVT:
        ESP_LOGI(TAG, "A2DP audio state %d", param->audio_stat.state);
        break;
    case ESP_A2D_AUDIO_CFG_EVT:
        ESP_LOGI(TAG, "A2DP codec configured (type %d)", param->audio_cfg.mcc.type);
        break;
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        ESP_LOGI(TAG, "media ctrl %d ack %d",
                 param->media_ctrl_stat.cmd, param->media_ctrl_stat.status);
        /* Two-step source start: once the stack confirms it is ready, open the media channel. */
        if (param->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY &&
            param->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
        }
        break;
    case ESP_A2D_PROF_STATE_EVT:
        ESP_LOGI(TAG, "A2DP profile %s",
                 param->a2d_prof_stat.init_state == ESP_A2D_INIT_SUCCESS ? "init" : "deinit");
        break;
    case ESP_A2D_SEP_REG_STATE_EVT:
        ESP_LOGI(TAG, "SBC endpoint %d register state %d",
                 param->a2d_sep_reg_stat.seid, param->a2d_sep_reg_stat.reg_state);
        break;
    default:
        break;
    }
}

/* AVRCP controller role: we send commands to the speaker (set its absolute volume). We do not
   subscribe to its volume-change notifications on purpose: the volume potentiometer is the
   master, so mirroring the speaker's local changes would fight the knob. */
static void avrc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
    switch (event) {
    case ESP_AVRC_CT_CONNECTION_STATE_EVT:
        s_avrc_connected = param->conn_stat.connected;
        ESP_LOGI(TAG, "AVRCP %s", s_avrc_connected ? "connected" : "disconnected");
        /* AVRCP comes up a few seconds after A2DP, so a volume set before then was deferred.
           Apply it now, otherwise the speaker plays at its own (often loud) default. */
        if (s_avrc_connected && s_avrc_have_vol) {
            esp_avrc_ct_send_set_absolute_volume_cmd(next_tl(), s_avrc_vol);
        }
        break;
    case ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT:
        s_vol_acked = true;
        ESP_LOGI(TAG, "AVRCP volume acked: %u/127", param->set_volume_rsp.volume);
        break;
    case ESP_AVRC_CT_PROF_STATE_EVT:
        ESP_LOGI(TAG, "AVRCP CT %s",
                 param->avrc_ct_init_stat.state == ESP_AVRC_INIT_SUCCESS ? "init" : "deinit");
        break;
    default:
        break;
    }
}

esp_err_t bluetooth_init(void)
{
    if (s_inited) return ESP_OK;

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;

    /* Classic-only: hand the BLE controller RAM back to the heap. */
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((err = esp_bt_controller_init(&cfg)) != ESP_OK) return err;
    if ((err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) return err;
    if ((err = esp_bluedroid_init()) != ESP_OK) return err;
    if ((err = esp_bluedroid_enable()) != ESP_OK) return err;

    esp_bt_gap_set_device_name(BT_DEVICE_NAME);
    esp_bt_gap_register_callback(gap_cb);

    /* No display, no keyboard: just-works Secure Simple Pairing. */
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof iocap);

    /* AVRCP must be initialised before A2DP (Bluedroid requirement, see esp_avrc_ct_init docs). */
    if ((err = esp_avrc_ct_init()) != ESP_OK) return err;
    esp_avrc_ct_register_callback(avrc_ct_cb);

    esp_a2d_register_callback(a2d_cb);
    if ((err = esp_a2d_source_init()) != ESP_OK) return err;

    /* Advertise the full SBC capability range on one endpoint; the sink picks the config. */
    esp_a2d_mcc_t sep = { .type = ESP_A2D_MCT_SBC };
    sep.cie.sbc_info.samp_freq    = ESP_A2D_SBC_CIE_SF_44K | ESP_A2D_SBC_CIE_SF_48K;
    sep.cie.sbc_info.ch_mode      = ESP_A2D_SBC_CIE_CH_MODE_MONO | ESP_A2D_SBC_CIE_CH_MODE_DUAL_CHANNEL |
                                    ESP_A2D_SBC_CIE_CH_MODE_STEREO | ESP_A2D_SBC_CIE_CH_MODE_JOINT_STEREO;
    sep.cie.sbc_info.block_len    = ESP_A2D_SBC_CIE_BLOCK_LEN_4 | ESP_A2D_SBC_CIE_BLOCK_LEN_8 |
                                    ESP_A2D_SBC_CIE_BLOCK_LEN_12 | ESP_A2D_SBC_CIE_BLOCK_LEN_16;
    sep.cie.sbc_info.num_subbands = ESP_A2D_SBC_CIE_NUM_SUBBANDS_4 | ESP_A2D_SBC_CIE_NUM_SUBBANDS_8;
    sep.cie.sbc_info.alloc_mthd   = ESP_A2D_SBC_CIE_ALLOC_MTHD_SNR | ESP_A2D_SBC_CIE_ALLOC_MTHD_LOUDNESS;
    sep.cie.sbc_info.min_bitpool  = 2;
    sep.cie.sbc_info.max_bitpool  = 53;
    if ((err = esp_a2d_source_register_stream_endpoint(0, &sep)) != ESP_OK) return err;

    /* We initiate connections, so stay reachable but not discoverable. */
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);

    s_inited = true;
    ESP_LOGI(TAG, "A2DP source ready as \"%s\"", BT_DEVICE_NAME);
    return ESP_OK;
}

esp_err_t bluetooth_scan_start(void)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_device_count = 0;
    xSemaphoreGive(s_lock);

    s_scanning = true;
    ESP_LOGI(TAG, "scanning for nearby devices");
    return esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, BT_INQ_LEN, 0);
}

esp_err_t bluetooth_scan_stop(void)
{
    if (!s_scanning) return ESP_OK;
    s_scanning = false;
    return esp_bt_gap_cancel_discovery();
}

bool bluetooth_is_scanning(void)
{
    return s_scanning;
}

size_t bluetooth_get_devices(bluetooth_device_t *out, size_t cap)
{
    if (!out || cap == 0) return 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    size_t n = s_device_count < cap ? s_device_count : cap;
    memcpy(out, s_devices, n * sizeof *out);
    xSemaphoreGive(s_lock);
    return n;
}

esp_err_t bluetooth_connect(const esp_bd_addr_t bda)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    if (!bda) return ESP_ERR_INVALID_ARG;

    if (s_scanning) {
        s_scanning = false;
        esp_bt_gap_cancel_discovery();
    }
    memcpy(s_peer_bda, bda, ESP_BD_ADDR_LEN);
    esp_err_t err = esp_a2d_source_connect(s_peer_bda);
    if (err == ESP_OK) s_conn_state = BLUETOOTH_CONN_CONNECTING;
    return err;
}

esp_err_t bluetooth_connect_by_name(const char *name_substr)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    if (!name_substr || !name_substr[0]) return ESP_ERR_INVALID_ARG;

    strlcpy(s_target, name_substr, sizeof s_target);
    s_seeking = true;
    ESP_LOGI(TAG, "scanning for a device named \"*%s*\"", s_target);
    return esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, BT_INQ_LEN, 0);
}

esp_err_t bluetooth_disconnect(void)
{
    if (s_conn_state != BLUETOOTH_CONN_CONNECTED) return ESP_OK;
    return esp_a2d_source_disconnect(s_peer_bda);
}

bool bluetooth_is_connected(void)
{
    return s_conn_state == BLUETOOTH_CONN_CONNECTED;
}

bluetooth_conn_state_t bluetooth_get_conn_state(void)
{
    return s_conn_state;
}

bool bluetooth_get_last_device(bt_known_device_t *out)
{
    if (!out) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool valid = s_last_valid;
    if (valid) *out = s_last;
    xSemaphoreGive(s_lock);
    return valid;
}

void bluetooth_set_last_device(const bt_known_device_t *dev)
{
    if (!dev) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_last = *dev;
    s_last_valid = true;
    xSemaphoreGive(s_lock);
}

esp_err_t bluetooth_reconnect_last(void)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool valid = s_last_valid;
    esp_bd_addr_t bda;
    if (valid) memcpy(bda, s_last.bda, ESP_BD_ADDR_LEN);
    xSemaphoreGive(s_lock);
    if (!valid) return ESP_ERR_NOT_FOUND;
    return bluetooth_connect(bda);
}

esp_err_t bluetooth_forget_device(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool valid = s_last_valid;
    esp_bd_addr_t bda;
    if (valid) memcpy(bda, s_last.bda, ESP_BD_ADDR_LEN);
    s_last_valid = false;
    memset(&s_last, 0, sizeof s_last);
    xSemaphoreGive(s_lock);
    if (!valid) return ESP_OK;
    return esp_bt_gap_remove_bond_device(bda);
}

esp_err_t bluetooth_audio_start(esp_a2d_source_data_cb_t pcm_cb)
{
    if (s_conn_state != BLUETOOTH_CONN_CONNECTED) return ESP_ERR_INVALID_STATE;
    if (!pcm_cb) return ESP_ERR_INVALID_ARG;

    esp_err_t err = esp_a2d_source_register_data_callback(pcm_cb);
    if (err != ESP_OK) return err;
    /* CHECK_SRC_RDY is acked in a2d_cb, which then issues START. */
    return esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
}

esp_err_t bluetooth_audio_stop(void)
{
    return esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
}

esp_err_t bluetooth_set_absolute_volume(uint8_t volume)
{
    if (volume > 0x7F) volume = 0x7F;
    s_avrc_vol = volume;
    s_avrc_have_vol = true;
    /* Defer if AVRCP is not up yet; avrc_ct_cb re-applies s_avrc_vol on connection. */
    if (!s_avrc_connected) return ESP_OK;
    s_vol_acked = false;   /* cleared until the sink confirms this set (see bluetooth_volume_acked) */
    return esp_avrc_ct_send_set_absolute_volume_cmd(next_tl(), volume);
}

bool bluetooth_is_avrc_connected(void)
{
    return s_avrc_connected;
}

bool bluetooth_volume_acked(void)
{
    return s_vol_acked;
}
