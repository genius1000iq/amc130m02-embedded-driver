/**
 * @file amc130m02_hal.h
 * @brief Hardware Abstraction Layer (HAL) for AMC130M02 driver.
 *
 * The user must implement these functions according to their MCU/platform.
 * Copy this file into the project and provide real implementations.
 */

#ifndef AMC130M02_HAL_H
#define AMC130M02_HAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize SPI peripheral for AMC130M02.
 *        Should configure SPI mode 1 (CPOL=0, CPHA=1), 1 MHz typical,
 *        16‑bit data frames, MSB first.
 */
void AMC130M02_HAL_SPI_Init(void);

/**
 * @brief Transmit one 16‑bit word and simultaneously receive one 16‑bit word.
 * @param txData Word to send.
 * @return Received word.
 */
uint16_t AMC130M02_HAL_SPI_Transceive16(uint16_t txData);

/**
 * @brief Assert chip select (active low).
 */
void AMC130M02_HAL_CS_Assert(void);

/**
 * @brief Deassert chip select.
 */
void AMC130M02_HAL_CS_Deassert(void);

/**
 * @brief Read the state of the DRDY pin (optional).
 * @return true if DRDY indicates new data ready, false otherwise.
 * @note If DRDY is not connected, always return true.
 */
bool AMC130M02_HAL_DRDY_Read(void);

/**
 * @brief Millisecond delay.
 * @param ms Delay in milliseconds.
 */
void AMC130M02_HAL_DelayMs(uint32_t ms);

/**
 * @brief Optional: re‑configure SPI settings (e.g., word length) if needed.
 *        This driver automatically configures word length based on MODE register,
 *        but HAL may need to reconfigure SPI hardware. Leave as empty if not needed.
 */
void AMC130M02_HAL_SPI_SetWordLength(uint8_t bytesPerWord);

#ifdef __cplusplus
}
#endif

#endif /* AMC130M02_HAL_H */