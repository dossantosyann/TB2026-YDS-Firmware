#include "audio_dac.h"
#include "i2c_bus.h"

#define PCM5242_ADDR     0x4C
#define DAC_I2C_TIMEOUT  100  /* ms; fail loud rather than block forever */

/* Page 0 registers (the device powers up on page 0). */
#define REG_PAGE         0x00
#define REG_STANDBY      0x02   /* RQST=D4 (standby), RQPD=D0 (power-down) */
#define REG_MUTE         0x03   /* RQML=D4 (left), RQMR=D0 (right); 0 = un-muted */
#define REG_VOL_CTRL     0x3C   /* PCTL[1:0] volume-link mode */
#define REG_VOL_LEFT     0x3D

#define STANDBY_REQ      0x10   /* RQST */
#define MUTE_BOTH        0x11   /* RQML | RQMR */
#define VOL_R_FOLLOWS_L  0x01   /* PCTL=01: right channel tracks the left register */

static i2c_master_dev_handle_t s_dev;

static esp_err_t write_reg(uint8_t reg, uint8_t val)
{
    const uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_dev, buf, sizeof buf, DAC_I2C_TIMEOUT);
}

static esp_err_t read_reg(uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, DAC_I2C_TIMEOUT);
}

esp_err_t audio_dac_init(i2c_master_bus_handle_t bus)
{
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = PCM5242_ADDR,
        .scl_speed_hz    = I2C_BUS_FREQ_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &s_dev);
    if (err != ESP_OK) return err;

    err = write_reg(REG_PAGE, 0x00);    /* all our registers live on page 0 */
    if (err != ESP_OK) return err;
    return write_reg(REG_VOL_CTRL, VOL_R_FOLLOWS_L);  /* one write drives both channels */
}

esp_err_t audio_dac_set_volume(uint8_t level)
{
    return write_reg(REG_VOL_LEFT, level);   /* right channel follows, see audio_dac_init() */
}

esp_err_t audio_dac_get_volume(uint8_t *level)
{
    return read_reg(REG_VOL_LEFT, level);
}

esp_err_t audio_dac_mute(bool mute)
{
    return write_reg(REG_MUTE, mute ? MUTE_BOTH : 0x00);
}

esp_err_t audio_dac_standby(bool standby)
{
    return write_reg(REG_STANDBY, standby ? STANDBY_REQ : 0x00);
}
