/**
 * @file gpio_expander.c
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 */
#include "gpio_expander.h"
#include "i2c_bus.h"
#include "board_pins.h"

#define EXPANDER_ADDR    0x38
#define EXP_I2C_TIMEOUT  100   /* ms; fail loud rather than block forever */

/* PCA9554A command bytes. */
#define REG_INPUT        0x00  /* read-only */
#define REG_OUTPUT       0x01
#define REG_CONFIG       0x03  /* direction: 1 = input, 0 = output */

/* IO0 (DAC mute) + IO1 (amp shutdown) are outputs; the rest are inputs. */
#define EXP_OUTPUTS      ((1u << EXP_DAC_MUTE) | (1u << EXP_AMP_SHDN))
#define CONFIG_DIR       ((uint8_t)~EXP_OUTPUTS)

static i2c_master_dev_handle_t s_dev;
static uint8_t s_out;   /* shadow of the output port register */

static esp_err_t write_reg(uint8_t reg, uint8_t val)
{
    const uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_dev, buf, sizeof buf, EXP_I2C_TIMEOUT);
}

static esp_err_t read_reg(uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, EXP_I2C_TIMEOUT);
}

esp_err_t gpio_expander_init(i2c_master_bus_handle_t bus)
{
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = EXPANDER_ADDR,
        .scl_speed_hz    = I2C_BUS_FREQ_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &s_dev);
    if (err != ESP_OK) return err;

    s_out = 0x00;
    err = write_reg(REG_OUTPUT, s_out);   /* drive outputs low before enabling them */
    if (err != ESP_OK) return err;
    return write_reg(REG_CONFIG, CONFIG_DIR);
}

esp_err_t gpio_expander_set(uint8_t channel, bool level)
{
    if (level) s_out |= (uint8_t)(1u << channel);
    else       s_out &= (uint8_t)~(1u << channel);
    return write_reg(REG_OUTPUT, s_out);
}

esp_err_t gpio_expander_get(uint8_t channel, bool *level)
{
    uint8_t in;
    esp_err_t err = read_reg(REG_INPUT, &in);
    if (err != ESP_OK) return err;
    *level = (in >> channel) & 1u;
    return ESP_OK;
}

esp_err_t gpio_expander_read_all(uint8_t *inputs)
{
    return read_reg(REG_INPUT, inputs);
}
