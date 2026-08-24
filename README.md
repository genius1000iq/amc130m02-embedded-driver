# AMC130M02 Driver

Platform-independent C driver for **Texas Instruments AMC130M02** isolated delta-sigma modulator (2 channels). Provides full register access, ADC data reading, and auxiliary GPO control.

## Features

- Fully hardware‑agnostic – only 7 simple HAL functions need to be implemented.
- Built‑in CRC‑16 (CCITT and ANSI).
- Supports all commands: NULL, RESET, STANDBY, WAKEUP, LOCK, UNLOCK, RREG, WREG.
- Handles variable word length (16, 24, 32 bits).
- Automatic chip select management and timing delays.
- Ready for RTOS or superloop – requires a 1 ms tick.

## Repository Contents

| File              | Description |
|-------------------|-------------|
| `amc130m02.h`     | Public header: register definitions, commands, status codes, API. |
| `amc130m02.c`     | Driver implementation (platform‑independent). |
| `amc130m02_hal.h` | HAL template – user must implement these functions for the target MCU. |
| `README.md`       | This file. |
| `LICENSE`         | MIT license (recommended). |

## Requirements

- C99 (or later) compiler.
- Standard headers: `stdint.h`, `stdbool.h`, `stddef.h`.
- SPI peripheral supporting mode 1 (CPOL=0, CPHA=1), MSB first, 16‑bit frames (the driver manages word length internally).

## Porting to Your Platform

1. Copy `amc130m02.h` and `amc130m02.c` to your project.
2. Create `amc130m02_hal.h` using the provided template and **implement all declared functions** according to your MCU:
   - `AMC130M02_HAL_SPI_Init()` – initialise SPI (mode 1, MSB first, ~1 MHz).
   - `AMC130M02_HAL_SPI_Transceive16(uint16_t tx)` – synchronous 16‑bit SPI transfer (full duplex).
   - `AMC130M02_HAL_CS_Assert()` / `Deassert()` – drive the CS pin (active low).
   - `AMC130M02_HAL_DRDY_Read()` – read the DRDY pin state (return `true` if not connected).
   - `AMC130M02_HAL_DelayMs(uint32_t ms)` – millisecond delay.
   - `AMC130M02_HAL_SPI_SetWordLength(uint8_t bytes_per_word)` – optional; reconfigure SPI hardware if word length changes (otherwise keep empty).
3. Ensure HAL functions are declared as `extern "C"` when used with C++.

## Usage Example

```c
#include "amc130m02.h"

int main(void) {
    // 1. User‑provided hardware initialisation
    HAL_Init();
    // 2. Driver initialisation
    AMC130M02_Init();

    // Start a 1 ms timer that calls AMC130M02_Tick1ms()
    start_1ms_timer(AMC130M02_Tick1ms);

    while (1) {
        AMC130M02_Run();   // main loop: reads ADC data, toggles GPO

        // Or read ADC on demand:
        int32_t adc0, adc1;
        if (AMC130M02_ReadADC(&adc0, &adc1) == AMC_STATUS_OK) {
            // process adc0, adc1
        }

        // Control external LED via GPO
        AMC130M02_SetLED(true);
    }
}
```

## Design Notes

- The driver owns protocol framing, command encoding, register access, CRC calculation, and conversion of raw ADC words.
- Platform-specific SPI, chip-select, data-ready, and timing operations stay behind the HAL interface.
- No dynamic allocation is used, which makes the driver suitable for small bare-metal systems.
- The public repository contains a reusable standalone driver; product-specific integration code remains outside the project.

## Project Status

The driver API and register map are implemented. Hardware integration still requires a target-specific HAL and validation against the final board configuration.

## License

Released under the [MIT License](LICENSE).
