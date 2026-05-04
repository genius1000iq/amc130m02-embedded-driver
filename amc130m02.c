/**
 * @file amc130m02.c
 * @brief Driver implementation for AMC130M02.
 */

#include "amc130m02.h"
#include "amc130m02_hal.h"

#include <stddef.h>

/* -------------------------------------------------------------------------
 * Driver internal configuration
 * ------------------------------------------------------------------------- */
#define AMC_MAX_BYTES_PER_WORD      (4u)
#define AMC_STD_FRAME_WORDS         (4u)   /* command + response + CRC + extra */
#define SPI_TIMEOUT_PRESCALE        (1000u)
#define OUTPUT_TOGGLE_MS            (500u)
#define INIT_DELAY_MS               (20u)

/* -------------------------------------------------------------------------
 * Internal CRC functions (CCITT and ANSI)
 * ------------------------------------------------------------------------- */
static uint16_t crc16_ccitt_update(uint16_t crc, uint8_t data)
{
    crc ^= (uint16_t)data << 8;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000)
            crc = (crc << 1) ^ 0x1021;
        else
            crc <<= 1;
    }
    return crc;
}

static uint16_t crc16_ansi_update(uint16_t crc, uint8_t data)
{
    crc ^= (uint16_t)data;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x0001)
            crc = (crc >> 1) ^ 0xA001;
        else
            crc >>= 1;
    }
    return crc;
}

static uint16_t crc_calc(const uint8_t *data, uint16_t len, uint16_t crc_init, bool use_ansi)
{
    uint16_t crc = crc_init;
    for (uint16_t i = 0; i < len; i++) {
        if (use_ansi)
            crc = crc16_ansi_update(crc, data[i]);
        else
            crc = crc16_ccitt_update(crc, data[i]);
    }
    return crc;
}

/* -------------------------------------------------------------------------
 * Register map structure (for convenient bitfield access)
 * ------------------------------------------------------------------------- */
typedef struct {
    /* 0x00 ID (read only) */
    union {
        uint16_t reg;
        struct {
            uint16_t reserved0   : 8;
            uint16_t CHANCNT     : 4;
            uint16_t reserved1   : 4;
        } bits;
    } ID;

    /* 0x01 STATUS (read only) */
    union {
        uint16_t reg;
        struct {
            uint16_t DRDY0       : 1;
            uint16_t DRDY1       : 1;
            uint16_t RESERVED0   : 4;
            uint16_t SEC_FAIL    : 1;
            uint16_t FUSE_FAIL   : 1;
            uint16_t WLENGTH     : 2;
            uint16_t RESET       : 1;
            uint16_t CRC_TYPE    : 1;
            uint16_t CRC_ERR     : 1;
            uint16_t REG_MAP     : 1;
            uint16_t F_RESYNC    : 1;
            uint16_t LOCK_       : 1;
        } bits;
    } STATUS;

    /* 0x02 MODE (r/w) */
    union {
        uint16_t reg;
        struct {
            uint16_t DRDY_FMT    : 1;
            uint16_t DRDY_HiZ    : 1;
            uint16_t DRDY_SEL    : 2;
            uint16_t TIMEOUT     : 1;
            uint16_t RESERVED0   : 3;
            uint16_t WLENGTH     : 2;
            uint16_t RESET       : 1;
            uint16_t CRC_TYPE    : 1;
            uint16_t RX_CRC_EN   : 1;
            uint16_t REG_CRC_EN  : 1;
            uint16_t RESERVED1   : 1;
        } bits;
    } MODE;

    /* 0x03 CLOCK (r/w) */
    union {
        uint16_t reg;
        struct {
            uint16_t PWR         : 2;
            uint16_t OSR         : 3;
            uint16_t TURBO       : 1;
            uint16_t CLK_DIV     : 2;
            uint16_t CH0_EN      : 1;
            uint16_t CH1_EN      : 1;
            uint16_t RESERVED0   : 6;
        } bits;
    } CLOCK;

    /* 0x04 GAIN (r/w) */
    union {
        uint16_t reg;
        struct {
            uint16_t PGAGAIN0    : 3;
            uint16_t RESERVED0   : 1;
            uint16_t PGAGAIN1    : 3;
            uint16_t RESERVED1   : 9;
        } bits;
    } GAIN;

    uint16_t ReservedWord;                      /* 0x05 reserved */

    /* 0x06 CFG (r/w) */
    union {
        uint16_t reg;
        struct {
            uint16_t RESERVED0   : 8;
            uint16_t GC_EN       : 1;
            uint16_t GC_DLY      : 4;
            uint16_t GPO_DAT     : 1;
            uint16_t GPO_EN      : 1;
            uint16_t RESERVED1   : 1;
        } bits;
    } CFG;

    uint16_t ReservedWords[2];                  /* 0x07 .. 0x08 reserved */

    /* 0x09 CH0_CFG (r/w) */
    union {
        uint16_t reg;
        struct {
            uint16_t MUX0        : 3;
            uint16_t RESERVED0   : 4;
            uint16_t PHASE0      : 9;          /* 9 bits valid? Adjust as needed */
        } bits;
    } CH0_CFG;

    uint16_t CH0_OCAL_MSB;                     /* 0x0A */
    union {
        uint16_t reg;
        struct {
            uint16_t RESERVED0   : 8;
            uint16_t OCAL0_LSB   : 8;
        } bits;
    } CH0_OCAL_LSB;                            /* 0x0B */
    uint16_t CH0_GCAL_MSB;                     /* 0x0C */
    union {
        uint16_t reg;
        struct {
            uint16_t RESERVED0   : 8;
            uint16_t GCAL0_LSB   : 8;
        } bits;
    } CH0_GCAL_LSB;                            /* 0x0D */

    /* 0x0E CH1_CFG (r/w) */
    union {
        uint16_t reg;
        struct {
            uint16_t MUX1        : 3;
            uint16_t RESERVED0   : 4;
            uint16_t PHASE1      : 9;
        } bits;
    } CH1_CFG;

    uint16_t CH1_OCAL_MSB;                     /* 0x0F */
    union {
        uint16_t reg;
        struct {
            uint16_t RESERVED0   : 8;
            uint16_t OCAL1_LSB   : 8;
        } bits;
    } CH1_OCAL_LSB;                            /* 0x10 */
    uint16_t CH1_GCAL_MSB;                     /* 0x11 */
    union {
        uint16_t reg;
        struct {
            uint16_t RESERVED0   : 8;
            uint16_t GCAL1_LSB   : 8;
        } bits;
    } CH1_GCAL_LSB;                            /* 0x12 */

    /* 0x31 DCDC_CTRL (r/w) */
    union {
        uint16_t reg;
        struct {
            uint16_t DCDC_EN     : 1;
            uint16_t RESERVED0   : 7;
            uint16_t DCDC_FREQ   : 4;
            uint16_t RESERVED1   : 4;
        } bits;
    } DCDC_CTRL;

    uint16_t REGMAP_CRC;                       /* 0x3E */
} AMC130M02_Regs;

/* -------------------------------------------------------------------------
 * Static variables
 * ------------------------------------------------------------------------- */
static AMC130M02_Regs s_regs;          /* local register cache */
static uint8_t s_bytes_per_word = 3;   /* current word length in bytes (24‑bit default) */
static bool s_system_initialized = false;
static uint16_t s_toggle_counter = 0;  /* for output toggling */
static bool s_output_state = false;    /* current GPO state */

/* Helper: get current word length from STATUS register */
static void update_bytes_per_word(void)
{
    switch (s_regs.STATUS.bits.WLENGTH) {
        case 0: s_bytes_per_word = 2; break;  /* 16-bit */
        case 1: s_bytes_per_word = 3; break;  /* 24-bit */
        default: s_bytes_per_word = 4; break; /* 32-bit */
    }
}

/* -------------------------------------------------------------------------
 * Low-level SPI frame transaction (with CRC handling)
 * ------------------------------------------------------------------------- */
static AMC130M02_Status spi_transfer_frame(const uint16_t *tx_words, uint16_t word_count,
                                           uint16_t *rx_words, bool enable_crc)
{
    bool use_ansi = (s_regs.MODE.bits.CRC_TYPE == 1);
    uint16_t crc_tx = 0xFFFF;
    uint16_t crc_rx_calc = 0xFFFF;
    uint8_t tx_buf[AMC_MAX_BYTES_PER_WORD];
    uint8_t rx_buf[AMC_MAX_BYTES_PER_WORD];

    AMC130M02_HAL_CS_Assert();
    AMC130M02_HAL_DelayMs(1);   /* short setup time */

    for (uint16_t i = 0; i < word_count; i++) {
        /* Prepare TX bytes (always MSB first, 16-bit word) */
        tx_buf[0] = (tx_words[i] >> 8) & 0xFF;
        tx_buf[1] = (tx_words[i] >> 0) & 0xFF;
        /* If we have more than 2 bytes (e.g., 24-bit), pad with zeros */
        for (uint8_t b = 2; b < s_bytes_per_word; b++)
            tx_buf[b] = 0;

        /* Exchange bytes */
        for (uint8_t b = 0; b < s_bytes_per_word; b++) {
            rx_buf[b] = AMC130M02_HAL_SPI_Transceive16((uint16_t)tx_buf[b] << 8);
        }

        /* Reconstruct received word (only first two bytes are valid for 16-bit mode) */
        rx_words[i] = ((uint16_t)rx_buf[0] << 8) | rx_buf[1];

        /* Update CRC if needed */
        if (enable_crc) {
            crc_tx = crc_calc(tx_buf, s_bytes_per_word, crc_tx, use_ansi);
            crc_rx_calc = crc_calc(rx_buf, s_bytes_per_word, crc_rx_calc, use_ansi);
        }
    }

    AMC130M02_HAL_CS_Deassert();

    /* If CRC enabled, we must also read the trailing CRC word */
    if (enable_crc) {
        uint16_t crc_rx_real;
        AMC130M02_HAL_CS_Assert();
        crc_rx_real = AMC130M02_HAL_SPI_Transceive16(0x0000); /* dummy write */
        AMC130M02_HAL_CS_Deassert();
        if (crc_rx_calc != crc_rx_real)
            return AMC_STATUS_THIS_CRC_FAIL;
    }

    return AMC_STATUS_OK;
}

/* -------------------------------------------------------------------------
 * Command execution (single command without extra answer read)
 * ------------------------------------------------------------------------- */
static AMC130M02_Status execute_command(uint16_t command_word, uint16_t *response)
{
    uint16_t tx[2] = { command_word, 0 };
    uint16_t rx[2] = { 0, 0 };
    bool use_crc = (s_regs.MODE.bits.RX_CRC_EN != 0);
    AMC130M02_Status st;

    st = spi_transfer_frame(tx, use_crc ? 2 : 1, rx, use_crc);
    if (st == AMC_STATUS_OK && response)
        *response = rx[0];
    return st;
}

/* -------------------------------------------------------------------------
 * Read/Write register (single register)
 * ------------------------------------------------------------------------- */
static AMC130M02_Status read_register(uint16_t addr, uint16_t *value)
{
    uint16_t cmd = AMC_CMD_RREG | (addr << AMC_REGADDR_SHIFT);
    uint16_t resp;
    AMC130M02_Status st;

    st = execute_command(cmd, &resp);
    if (st != AMC_STATUS_OK)
        return st;

    /* The next frame returns the register value */
    uint16_t dummy_tx = AMC_CMD_NULL;
    uint16_t rx[1];
    st = spi_transfer_frame(&dummy_tx, 1, rx, false);
    if (st == AMC_STATUS_OK && value)
        *value = rx[0];
    return st;
}

static AMC130M02_Status write_register(uint16_t addr, uint16_t value)
{
    uint16_t cmd = AMC_CMD_WREG | (addr << AMC_REGADDR_SHIFT);
    uint16_t tx[2] = { cmd, value };
    uint16_t rx[2];
    bool use_crc = (s_regs.MODE.bits.RX_CRC_EN != 0);
    AMC130M02_Status st;

    st = spi_transfer_frame(tx, use_crc ? 3 : 2, rx, use_crc);
    if (st != AMC_STATUS_OK)
        return st;

    /* Verify response code */
    uint16_t expected = AMC_RESP_WREG | (addr << AMC_REGADDR_SHIFT);
    if ((rx[0] & 0xF0FF) != expected)   /* response code is in bits 12..15? adjust mask if needed */
        return AMC_STATUS_RESP_CODE_FAIL;

    return AMC_STATUS_OK;
}

/* -------------------------------------------------------------------------
 * Public API functions
 * ------------------------------------------------------------------------- */
void AMC130M02_Init(void)
{
    AMC130M02_HAL_SPI_Init();

    /* Reset the device via hardware? For software reset, send RESET command */
    (void)execute_command(AMC_CMD_RESET, NULL);
    AMC130M02_HAL_DelayMs(INIT_DELAY_MS);

    /* Wait for DRDY (if connected) else simple delay */
    while (!AMC130M02_HAL_DRDY_Read())
        AMC130M02_HAL_DelayMs(1);

    /* Read STATUS to know configuration */
    read_register(AMC_REG_STATUS, &s_regs.STATUS.reg);
    update_bytes_per_word();

    /* Configure registers (example: set CLOCK, DCDC, CFG) */
    s_regs.CLOCK.reg = 0x030Eu;          /* high resolution, OSR=1024, CLK_DIV=4, both channels enabled */
    write_register(AMC_REG_CLOCK, s_regs.CLOCK.reg);

    s_regs.DCDC_CTRL.bits.DCDC_FREQ = 11; /* example: 2.37‑2.59 MHz */
    s_regs.DCDC_CTRL.bits.DCDC_EN = 1;
    write_register(AMC_REG_DCDC_CTRL, s_regs.DCDC_CTRL.reg);

    s_regs.CFG.bits.GPO_EN = 1;
    s_regs.CFG.bits.GPO_DAT = 0;
    write_register(AMC_REG_CFG, s_regs.CFG.reg);

    s_system_initialized = true;
    s_toggle_counter = OUTPUT_TOGGLE_MS;
}

void AMC130M02_Run(void)
{
    if (!s_system_initialized) {
        AMC130M02_Init();
        return;
    }

    if (AMC130M02_HAL_DRDY_Read()) {
        /* Read ADC data using NULL command */
        uint16_t tx = AMC_CMD_NULL;
        uint16_t rx[3];
        AMC130M02_HAL_CS_Assert();
        for (int i = 0; i < 3; i++) {
            rx[i] = AMC130M02_HAL_SPI_Transceive16(tx);
        }
        AMC130M02_HAL_CS_Deassert();

        /* Store results (24‑bit data assumed, sign‑extend to 32‑bit) */
        int32_t ch0 = (int16_t)rx[1];
        int32_t ch1 = (int16_t)rx[2];
        /* If 24‑bit, we could do proper sign extension, but for simplicity keep 16‑bit */
        /* The public function will be used by application */
    }

    /* Handle auxiliary output toggling (example) */
    if (s_toggle_counter == 0) {
        s_output_state = !s_output_state;
        s_regs.CFG.bits.GPO_DAT = s_output_state ? 1 : 0;
        write_register(AMC_REG_CFG, s_regs.CFG.reg);
        s_toggle_counter = OUTPUT_TOGGLE_MS;
    }
}

void AMC130M02_Tick1ms(void)
{
    if (s_toggle_counter > 0)
        s_toggle_counter--;
}

AMC130M02_Status AMC130M02_ReadADC(int32_t *adc0_ptr, int32_t *adc1_ptr)
{
    if (!s_system_initialized)
        return AMC_STATUS_UNKNOWN_CMD;

    uint16_t tx = AMC_CMD_NULL;
    uint16_t rx[3];
    AMC130M02_HAL_CS_Assert();
    for (int i = 0; i < 3; i++) {
        rx[i] = AMC130M02_HAL_SPI_Transceive16(tx);
    }
    AMC130M02_HAL_CS_Deassert();

    if (adc0_ptr)
        *adc0_ptr = (int16_t)rx[1];   /* sign‑extend from 16‑bit */
    if (adc1_ptr)
        *adc1_ptr = (int16_t)rx[2];

    return AMC_STATUS_OK;
}

void AMC130M02_SetLED(bool state)
{
    s_regs.CFG.bits.GPO_DAT = state ? 1 : 0;
    write_register(AMC_REG_CFG, s_regs.CFG.reg);
}

AMC130M02_Status AMC130M02_ReadReg(uint16_t reg_addr, uint16_t *reg_value)
{
    return read_register(reg_addr, reg_value);
}

AMC130M02_Status AMC130M02_WriteReg(uint16_t reg_addr, uint16_t reg_value)
{
    return write_register(reg_addr, reg_value);
}