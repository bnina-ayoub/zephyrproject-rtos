/*
 * SPDX-FileCopyrightText: 2026 NXP
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Header file for extended sensor API of NXP MCUX EQDC sensor
 * @ingroup sensor_mcux_eqdc
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_SENSOR_MCUX_EQDC_H_
#define ZEPHYR_INCLUDE_DRIVERS_SENSOR_MCUX_EQDC_H_

/**
 * @brief NXP MCUX EQDC sensor
 * @defgroup sensor_mcux_eqdc EQDC MCUX
 * @ingroup sensor_interface_ext
 * @{
 */

#include <zephyr/drivers/sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Extended sensor attributes for NXP MCUX EQDC
 */
enum sensor_attribute_mcux_eqdc {
	/** Counts per revolution - uint32_t */
	SENSOR_ATTR_EQDC_COUNTS_PER_REVOLUTION = SENSOR_ATTR_PRIV_START,

	/** Clock frequency of the EQDC module after prescaled - uint32_t */
	SENSOR_ATTR_EQDC_PRESCALED_FREQUENCY,
};

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_DRIVERS_SENSOR_MCUX_EQDC_H_ */
