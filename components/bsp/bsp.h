/**
 * @file bsp.h
 * @brief Board support package umbrella header: pulls in every bus driver.
 *
 * @note Développé avec l'assistance de Claude Opus 4.8 (Anthropic), sous la
 *       direction de Y. Dos Santos : spécification, revue et validation sur
 *       cible par l'auteur.
 *
 * @defgroup bsp Board Support Package
 * @brief Low-level board drivers: SPI, I2C, I2S buses and the volume ADC.
 */
#pragma once

#include "spi_bus.h"
#include "i2c_bus.h"
#include "i2s_bus.h"
#include "adc.h"
