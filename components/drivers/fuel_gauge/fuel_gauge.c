#include "fuel_gauge.h"
#include "i2c_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define FUEL_GAUGE_ADDR  0x36
#define FG_I2C_TIMEOUT   100   /* ms; fail loud rather than block forever */

/* ----- Board / cell parameters (change these with the battery) ----- */
#define FG_RSENSE_MOHM     5.0f   /* sense resistor, mOhm */
#define FG_DESIGN_CAP_MAH  950    /* cell nominal capacity */
#define FG_ICHG_TERM_MA    75     /* charger termination current (MAX77757) */

/* ModelGauge m5 standard LSBs scaled by Rsense (datasheet Table 2). */
#define CAP_LSB_MAH      (5.0f / FG_RSENSE_MOHM)       /* 5.0uVh/Rsense -> mAh  (1.0 at 5mOhm)   */
#define CURRENT_LSB_MA   (1.5625f / FG_RSENSE_MOHM)    /* 1.5625uV/Rsense -> mA (0.3125 at 5mOhm) */
#define VOLT_LSB_V       78.125e-6f                    /* 78.125uV per LSB */
#define TIME_LSB_S       5.625f                        /* 5.625s per LSB */
#define PCT_LSB          (1.0f / 256.0f)               /* 1/256 % per LSB */
#define TEMP_LSB_C       (1.0f / 256.0f)               /* 1/256 degC per LSB */
#define CYCLES_LSB       (1.0f / 100.0f)               /* 1% per LSB -> full cycles */

/* EZ-config register values (see MAX1726x ModelGauge m5 EZ user guide). */
#define DESIGN_CAP_RAW   ((uint16_t)(FG_DESIGN_CAP_MAH / CAP_LSB_MAH))
#define ICHG_TERM_RAW    ((uint16_t)(FG_ICHG_TERM_MA / CURRENT_LSB_MA))
#define VEMPTY_DEFAULT   0xA561   /* VE = 3.3V, VR = 3.88V (POR default, recommended) */
#define MODEL_CFG        0x8000   /* Refresh=1, ModelID=0 (LiCoO2), VChg=0 (4.2V charge) */

/* Output registers. */
#define REG_REP_CAP      0x05
#define REG_REP_SOC      0x06
#define REG_AGE          0x07
#define REG_TEMP         0x08
#define REG_VCELL        0x09
#define REG_CURRENT      0x0A
#define REG_TTE          0x11
#define REG_CYCLES       0x17
#define REG_TTF          0x20

/* Config / control registers. */
#define REG_STATUS       0x00
#define REG_DESIGN_CAP   0x18
#define REG_ICHG_TERM    0x1E
#define REG_VEMPTY       0x3A
#define REG_FSTAT        0x3D
#define REG_CMD          0x60
#define REG_HIBCFG       0xBA
#define REG_MODEL_CFG    0xDB

#define STATUS_POR       0x0002   /* power-on reset flag */
#define FSTAT_DNR        0x0001   /* data not ready */
#define MODELCFG_REFRESH 0x8000   /* model reload in progress */
#define CMD_SOFT_WAKEUP  0x0090
#define CMD_CLEAR        0x0000

#define REFRESH_TIMEOUT_MS  800   /* DNR clear / model refresh upper bound (~710ms typ) */

static i2c_master_dev_handle_t s_dev;

static esp_err_t read_reg(uint8_t reg, uint16_t *val)
{
    uint8_t rx[2];
    esp_err_t err = i2c_master_transmit_receive(s_dev, &reg, 1, rx, sizeof rx, FG_I2C_TIMEOUT);
    if (err != ESP_OK) return err;
    *val = (uint16_t)rx[0] | ((uint16_t)rx[1] << 8);   /* little-endian */
    return ESP_OK;
}

static esp_err_t write_reg(uint8_t reg, uint16_t val)
{
    const uint8_t buf[3] = { reg, (uint8_t)(val & 0xFF), (uint8_t)(val >> 8) };
    return i2c_master_transmit(s_dev, buf, sizeof buf, FG_I2C_TIMEOUT);
}

/* Config registers live in shadow RAM and can silently drop a write, so the user
   guide requires reading them back; retry a few times before giving up. */
static esp_err_t write_verify(uint8_t reg, uint16_t val)
{
    for (int attempt = 0; attempt < 3; attempt++) {
        esp_err_t err = write_reg(reg, val);
        if (err != ESP_OK) return err;
        vTaskDelay(pdMS_TO_TICKS(1));
        uint16_t readback;
        err = read_reg(reg, &readback);
        if (err != ESP_OK) return err;
        if (readback == val) return ESP_OK;
    }
    return ESP_ERR_INVALID_RESPONSE;
}

/* Poll a register until all bits in mask read 0, or time out. */
static esp_err_t wait_clear(uint8_t reg, uint16_t mask)
{
    for (int waited_ms = 0; waited_ms <= REFRESH_TIMEOUT_MS; waited_ms += 10) {
        uint16_t v;
        esp_err_t err = read_reg(reg, &v);
        if (err != ESP_OK) return err;
        if ((v & mask) == 0) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_ERR_TIMEOUT;
}

/* EZ model load: soft-wakeup, write the cell parameters, refresh, restore HibCfg. */
static esp_err_t load_ez_config(void)
{
    uint16_t hibcfg;
    esp_err_t err = read_reg(REG_HIBCFG, &hibcfg);
    if (err != ESP_OK) return err;

    /* Exit hibernate so config changes take effect immediately. */
    if ((err = write_reg(REG_CMD, CMD_SOFT_WAKEUP)) != ESP_OK) return err;
    if ((err = write_reg(REG_HIBCFG, 0x0000)) != ESP_OK) return err;
    if ((err = write_reg(REG_CMD, CMD_CLEAR)) != ESP_OK) return err;

    if ((err = write_verify(REG_DESIGN_CAP, DESIGN_CAP_RAW)) != ESP_OK) return err;
    if ((err = write_verify(REG_ICHG_TERM, ICHG_TERM_RAW)) != ESP_OK) return err;
    if ((err = write_verify(REG_VEMPTY, VEMPTY_DEFAULT)) != ESP_OK) return err;
    if ((err = write_reg(REG_MODEL_CFG, MODEL_CFG)) != ESP_OK) return err;

    err = wait_clear(REG_MODEL_CFG, MODELCFG_REFRESH);   /* gauge clears Refresh when done */
    if (err != ESP_OK) return err;

    return write_reg(REG_HIBCFG, hibcfg);   /* re-enable automatic hibernate */
}

esp_err_t fuel_gauge_init(i2c_master_bus_handle_t bus)
{
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = FUEL_GAUGE_ADDR,
        .scl_speed_hz    = I2C_BUS_FREQ_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &s_dev);
    if (err != ESP_OK) return err;

    uint16_t status;
    err = read_reg(REG_STATUS, &status);
    if (err != ESP_OK) return err;
    if ((status & STATUS_POR) == 0) return ESP_OK;   /* already configured: keep learned state */

    err = wait_clear(REG_FSTAT, FSTAT_DNR);          /* wait for first measurements */
    if (err != ESP_OK) return err;

    err = load_ez_config();
    if (err != ESP_OK) return err;

    err = read_reg(REG_STATUS, &status);             /* re-read: POR persists until cleared */
    if (err != ESP_OK) return err;
    return write_reg(REG_STATUS, status & ~STATUS_POR);
}

esp_err_t fuel_gauge_read(fuel_gauge_data_t *out)
{
    uint16_t raw;
    esp_err_t err;

    if ((err = read_reg(REG_REP_SOC, &raw)) != ESP_OK) return err;
    out->soc_pct = raw * PCT_LSB;

    if ((err = read_reg(REG_VCELL, &raw)) != ESP_OK) return err;
    out->voltage_v = raw * VOLT_LSB_V;

    if ((err = read_reg(REG_CURRENT, &raw)) != ESP_OK) return err;
    out->current_ma = (int16_t)raw * CURRENT_LSB_MA;   /* signed two's complement */

    if ((err = read_reg(REG_REP_CAP, &raw)) != ESP_OK) return err;
    out->rep_cap_mah = raw * CAP_LSB_MAH;

    if ((err = read_reg(REG_TTE, &raw)) != ESP_OK) return err;
    out->tte_s = raw * TIME_LSB_S;

    if ((err = read_reg(REG_TTF, &raw)) != ESP_OK) return err;
    out->ttf_s = raw * TIME_LSB_S;

    if ((err = read_reg(REG_AGE, &raw)) != ESP_OK) return err;
    out->age_pct = raw * PCT_LSB;

    if ((err = read_reg(REG_TEMP, &raw)) != ESP_OK) return err;
    out->die_temp_c = (int16_t)raw * TEMP_LSB_C;       /* signed two's complement */

    if ((err = read_reg(REG_CYCLES, &raw)) != ESP_OK) return err;
    out->cycles = raw * CYCLES_LSB;

    return ESP_OK;
}
