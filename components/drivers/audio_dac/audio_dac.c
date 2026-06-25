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
#define REG_CLOCK_STATUS 0x5E   /* read-only clock/PLL validity flags (0 = valid) */

#define STANDBY_REQ      0x10   /* RQST */
#define MUTE_BOTH        0x11   /* RQML | RQMR */
#define VOL_R_FOLLOWS_L  0x01   /* PCTL=01: right channel tracks the left register */

/* ----- Manual clock tree for 3-wire I2S, software mode -----
   SCK is tied to GND on our board, so the DAC has no master clock and its clock
   auto-set does not work in software mode (datasheet 8.8.3). We reference the PLL
   off BCK (= 64*fS, the I2S runs 32-bit stereo slots) and set every divider by hand.

   The PLL input must satisfy 1 MHz <= BCK <= 20 MHz, so fS < 16 kHz (BCK < 1 MHz)
   cannot lock and is rejected; the supported range is 16 kHz .. 192 kHz (see README).

   Clock tree (datasheet Fig. 68, verified against the 44.1 kHz row):
     VCO = BCK*J*R/P,  DSPCLK = VCO/NMAC,  DACCLK = VCO/NDAC,
     OSRCK = DACCLK/DOSR (must be 16*fS),  CPCK = DACCLK/NCP,  IDAC = DSPCLK/fS.
   Each rate holds the VCO at 90.3168 MHz (44.1k family) or 98.304 MHz (48k family)
   and keeps NDAC=16, NCP=4 fixed; J/R follow from BCK and NMAC/DOSR/IDAC from fS.
   Register encodings: dividers and PLL P/R store (value-1); J and IDAC are direct. */
#define REG_CLK_FLAGS    0x25   /* IDSK D4 | IDCH D3 | DCAS D1 (ignore SCK + autoset off) */
#define REG_PLL_SREF     0x0D   /* SREF[2:0]=D6:4: PLL reference clock select */
#define REG_PLL_P        0x14
#define REG_PLL_J        0x15
#define REG_PLL_D_MSB    0x16
#define REG_PLL_D_LSB    0x17
#define REG_PLL_R        0x18
#define REG_DIV_DSP      0x1B   /* DDSP: mini-DSP clock divider (NMAC) */
#define REG_DIV_DAC      0x1C   /* DDAC: DAC clock divider (NDAC) */
#define REG_DIV_NCP      0x1D   /* DNCP: charge-pump clock divider (NCP) */
#define REG_DIV_OSR      0x1E   /* DOSR: OSR clock divider (gives OSRCK = 16*fS) */
#define REG_IDAC_MSB     0x23   /* DSP clock cycles per audio frame (= DSPCLK/fS) */
#define REG_IDAC_LSB     0x24

#define CLK_FLAGS_MANUAL 0x1A   /* IDSK|IDCH|DCAS: ignore the grounded SCK, drop autoset */
#define SREF_BCK         0x10   /* SREF[2:0]=001: PLL reference = BCK */

#define NDAC_FIXED       16     /* DAC clock divider, constant across the supported rates */
#define NCP_FIXED        4      /* charge-pump clock divider, constant */

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

/* Per-sample-rate PLL/divider coefficients (P = 1 and D = 0 for every supported rate).
   Rows marked "Table 50" are TI's recommended VREF values (RSCK=64 column, which is our
   BCK = 64*fS reference). The 24/88.2/176.4 kHz rows are not in Table 50; they are derived
   within the same VCO family by mirroring the tabulated sibling (22.05 / 96 / 192 kHz).
   TI keeps NMAC so DSPCLK stays <= 49.152 MHz, so IDAC drops to 512/256 above 48 kHz. */
struct clock_cfg {
    uint32_t rate_hz;
    uint8_t  j;        /* PLL J (K = J, since D = 0) */
    uint8_t  r;        /* PLL R */
    uint8_t  nmac;     /* mini-DSP clock divider */
    uint8_t  dosr;     /* OSR clock divider */
    uint16_t idac;     /* DSP clock cycles per audio frame */
};

static const struct clock_cfg k_clock_cfgs[] = {
    /* rate    J   R  NMAC DOSR  IDAC*/
    {  16000, 48,  2,  6,  24,  1024 },
    {  22050, 32,  2,  4,  16,  1024 },
    {  24000, 32,  2,  4,  16,  1024 },
    {  32000, 24,  2,  3,  12,  1024 },
    {  44100, 16,  2,  2,   8,  1024 },
    {  48000, 16,  2,  2,   8,  1024 },
    {  88200,  8,  2,  2,   4,   512 },
    {  96000,  8,  2,  2,   4,   512 },
    { 176400,  4,  2,  2,   2,   256 },
    { 192000,  4,  2,  2,   2,   256 },
};

/* Program the BCK-referenced PLL and clock dividers for rate_hz.
   DCAS is written first so clock auto-set can't fight the manual dividers.

   Rewriting the clock tree drops the PLL out of lock, and the device auto-enters standby
   on the "clock changing" flag anyway. So we hold it in standby across the rewrite and
   issue the power-up command (RQST=0, datasheet 10.5.3) when done. The caller still
   soft-mutes around this for a pop-free retune (XSMT also gates the amp). */
static esp_err_t configure_clock(uint32_t rate_hz)
{
    const struct clock_cfg *c = NULL;
    for (size_t i = 0; i < sizeof k_clock_cfgs / sizeof k_clock_cfgs[0]; i++) {
        if (k_clock_cfgs[i].rate_hz == rate_hz) { c = &k_clock_cfgs[i]; break; }
    }
    if (c == NULL) return ESP_ERR_NOT_SUPPORTED;   /* fS < 16 kHz or a non-standard rate */

    esp_err_t err = write_reg(REG_STANDBY, STANDBY_REQ);   /* freeze clocks before retune */
    if (err != ESP_OK) return err;

    const uint8_t seq[][2] = {
        { REG_CLK_FLAGS, CLK_FLAGS_MANUAL },     /* disable autoset, ignore grounded SCK */
        { REG_PLL_SREF,  SREF_BCK },             /* PLL reference = BCK */
        { REG_PLL_P,     0x00 },                 /* P = 1 */
        { REG_PLL_J,     c->j },
        { REG_PLL_D_MSB, 0x00 },                 /* D = 0 */
        { REG_PLL_D_LSB, 0x00 },
        { REG_PLL_R,     (uint8_t)(c->r - 1) },
        { REG_DIV_DSP,   (uint8_t)(c->nmac - 1) },
        { REG_DIV_DAC,   NDAC_FIXED - 1 },
        { REG_DIV_NCP,   NCP_FIXED - 1 },
        { REG_DIV_OSR,   (uint8_t)(c->dosr - 1) },
        { REG_IDAC_MSB,  (uint8_t)(c->idac >> 8) },
        { REG_IDAC_LSB,  (uint8_t)(c->idac & 0xFF) },
    };
    for (size_t i = 0; i < sizeof seq / sizeof seq[0]; i++) {
        err = write_reg(seq[i][0], seq[i][1]);
        if (err != ESP_OK) return err;   /* leaves the DAC in standby (muted), not half-tuned */
    }
    return write_reg(REG_STANDBY, 0x00);   /* power-up sequence command (RQST=0) */
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

    err = configure_clock(44100);       /* default rate; retuned per track via _set_sample_rate */
    if (err != ESP_OK) return err;

    return write_reg(REG_VOL_CTRL, VOL_R_FOLLOWS_L);  /* one write drives both channels */
}

esp_err_t audio_dac_set_sample_rate(uint32_t rate_hz)
{
    return configure_clock(rate_hz);
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

esp_err_t audio_dac_get_clock_status(uint8_t *status)
{
    return read_reg(REG_CLOCK_STATUS, status);
}
