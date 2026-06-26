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
#include "nvs_flash.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "bluetooth";

#define BT_DEVICE_NAME   "TB2026-YDS"
#define BT_INQ_LEN       10              /* inquiry window: 10 * 1.28 s ~= 12.8 s */

static bool               s_inited;
static bool               s_seeking;     /* an inquiry is running for connect_by_name */
static char               s_target[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
static esp_bd_addr_t      s_peer_bda;
static bool               s_connected;
static esp_a2d_conn_hdl_t s_conn_hdl;    /* valid while s_connected; consumed by the BT sink */
static uint16_t           s_audio_mtu;   /* negotiated audio MTU; caps the encoded send size */

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

static void gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_DISC_RES_EVT: {
        if (!s_seeking) break;
        char name[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
        if (!name_from_disc(&param->disc_res, name, sizeof name)) break;
        if (strstr(name, s_target)) {
            ESP_LOGI(TAG, "found '%s', connecting", name);
            memcpy(s_peer_bda, param->disc_res.bda, ESP_BD_ADDR_LEN);
            s_seeking = false;
            esp_bt_gap_cancel_discovery();
            esp_a2d_source_connect(s_peer_bda);
        }
        break;
    }
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
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

static void a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
        if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            s_connected = true;
            s_conn_hdl = param->conn_stat.conn_hdl;
            s_audio_mtu = param->conn_stat.audio_mtu;
            ESP_LOGI(TAG, "A2DP connected (audio MTU %u)", s_audio_mtu);
        } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            s_connected = false;
            ESP_LOGW(TAG, "A2DP disconnected (reason %d)", param->conn_stat.disc_rsn);
        } else {
            ESP_LOGI(TAG, "A2DP connection state %d", param->conn_stat.state);
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

esp_err_t bluetooth_init(void)
{
    if (s_inited) return ESP_OK;

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
    if (!s_connected) return ESP_OK;
    return esp_a2d_source_disconnect(s_peer_bda);
}

bool bluetooth_is_connected(void)
{
    return s_connected;
}
