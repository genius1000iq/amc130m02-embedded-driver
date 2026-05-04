/**
 * @file amc130m02.h
 * @brief Driver for Texas Instruments AMC130M02 isolated delta‑sigma modulator.
 *
 * Provides initialisation, register access, and ADC data reading.
 * Requires the user to implement hardware abstraction layer (amc130m02_hal.h).
 *
 * @note Based on original code for TMS320F281x, made portable.
 * @version 2.0
 */

#ifndef AMC130M02_H
#define AMC130M02_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Register addresses (AMC130M02)
 * ------------------------------------------------------------------------- */
#define AMC_REG_ID                  (0x00)
#define AMC_REG_STATUS              (0x01)
#define AMC_REG_MODE                (0x02)
#define AMC_REG_CLOCK               (0x03)
#define AMC_REG_GAIN                (0x04)
#define AMC_REG_CFG                 (0x06)
#define AMC_REG_CH0_CFG             (0x09)
#define AMC_REG_CH0_OCAL_MSB        (0x0A)
#define AMC_REG_CH0_OCAL_LSB        (0x0B)
#define AMC_REG_CH0_GCAL_MSB        (0x0C)
#define AMC_REG_CH0_GCAL_LSB        (0x0D)
#define AMC_REG_CH1_CFG             (0x0E)
#define AMC_REG_CH1_OCAL_MSB        (0x0F)
#define AMC_REG_CH1_OCAL_LSB        (0x10)
#define AMC_REG_CH1_GCAL_MSB        (0x11)
#define AMC_REG_CH1_GCAL_LSB        (0x12)
#define AMC_REG_DCDC_CTRL           (0x31)
#define AMC_REG_REGMAP_CRC          (0x3E)

/* -------------------------------------------------------------------------
 * Commands
 * ------------------------------------------------------------------------- */
#define AMC_CMD_NULL                (0x0000)
#define AMC_CMD_RESET               (0x0011)
#define AMC_CMD_STANDBY             (0x0022)
#define AMC_CMD_WAKEUP              (0x0033)
#define AMC_CMD_LOCK                (0x0555)
#define AMC_CMD_UNLOCK              (0x0655)
#define AMC_CMD_RREG                (0xA000)
#define AMC_CMD_WREG                (0x6000)

#define AMC_RESP_NULL               (0x0000)
#define AMC_RESP_RESET              (0xFF23)
#define AMC_RESP_STANDBY            (0x0022)
#define AMC_RESP_WAKEUP             (0x0033)
#define AMC_RESP_LOCK               (0x0555)
#define AMC_RESP_UNLOCK             (0x0655)
#define AMC_RESP_RREG               (0xE000)
#define AMC_RESP_WREG               (0x4000)

#define AMC_REGADDR_SHIFT           (7u)
#define AMC_REGCOUNT_SHIFT          (0u)

/* -------------------------------------------------------------------------
 * Return status codes
 * ------------------------------------------------------------------------- */
typedef enum {
    AMC_STATUS_OK,
    AMC_STATUS_PREV_CRC_FAIL,
    AMC_STATUS_THIS_CRC_FAIL,
    AMC_STATUS_RESP_CODE_FAIL,
    AMC_STATUS_SPI_REQUEST_FAIL,
    AMC_STATUS_SPI_ANSWER_FAIL,
    AMC_STATUS_UNKNOWN_CMD
} AMC130M02_Status;

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

/**
 * @brief Initialise the AMC130M02 device.
 * Must be called once before using any other function.
 */
void AMC130M02_Init(void);

/**
 * @brief Periodic main handler – call this frequently (e.g., in main loop).
 * It will read ADC data when DRDY is ready and manage auxiliary output.
 */
void AMC130M02_Run(void);

/**
 * @brief 1ms timer tick – must be called from an interrupt or timer every 1 ms.
 */
void AMC130M02_Tick1ms(void);

/**
 * @brief Read both ADC channels.
 * @param[out] adc0_ptr Pointer to store channel 0 value (signed 24‑bit
 *            in the two lower bytes, sign‑extended to int32_t but returned as int32_t).
 * @param[out] adc1_ptr Pointer to store channel 1 value.
 * @return Status code.
 */
AMC130M02_Status AMC130M02_ReadADC(int32_t *adc0_ptr, int32_t *adc1_ptr);

/**
 * @brief Control the auxiliary GPO pin (can drive an LED).
 * @param state true = high, false = low.
 */
void AMC130M02_SetLED(bool state);

/**
 * @brief Read a single register.
 * @param reg_addr Register address.
 * @param[out] reg_value Pointer to store the register value.
 * @return Status code.
 */
AMC130M02_Status AMC130M02_ReadReg(uint16_t reg_addr, uint16_t *reg_value);

/**
 * @brief Write a single register.
 * @param reg_addr Register address.
 * @param reg_value Value to write.
 * @return Status code.
 */
AMC130M02_Status AMC130M02_WriteReg(uint16_t reg_addr, uint16_t reg_value);

#ifdef __cplusplus
}
#endif

#endif /* AMC130M02_H */