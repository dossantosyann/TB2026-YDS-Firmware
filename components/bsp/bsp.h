/**
 * @file bsp.h
 * @brief Board support package umbrella header: pulls in every bus driver.
 *
 * @defgroup bsp Board Support Package
 * @brief Low-level board drivers: SPI, I2C, I2S buses and the volume ADC.
 */
#pragma once

#include "spi_bus.h"
#include "i2c_bus.h"
#include "i2s_bus.h"
#include "adc.h"
