/*
 * Copyright 2026 NXP
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_mcux_eqdc

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <fsl_eqdc.h>
#include <fsl_inputmux.h>
#include <fsl_inputmux_connections.h>

#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/sensor/eqdc_mcux.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(eqdc_mcux, CONFIG_SENSOR_LOG_LEVEL);

struct eqdc_mcux_inputmux_entry {
	INPUTMUX_Type *base;
	uint16_t channel;
	uint32_t connection;
};

struct eqdc_mcux_config {
	EQDC_Type *base;
	const struct pinctrl_dev_config *pincfg;
	const struct eqdc_mcux_inputmux_entry *inputmux_entries;
    uint8_t inputmux_entries_count;
	const struct device *clock_dev;
    clock_control_subsys_t clock_subsys;
	uint8_t filter_count;
	uint8_t filter_sample_period;
	bool single_phase_mode;
};

struct eqdc_mcux_data {
	eqdc_config_t eqdc_config;
	int32_t position;
	uint32_t counts_per_revolution;
};

static eqdc_operate_mode_t int_to_work_mode(int32_t val)
{
	return val == 0 ? kEQDC_QuadratureDecodeOperationMode :
					kEQDC_SinglePhaseDecodeOperationMode;
}

static int eqdc_mcux_attr_set(const struct device *dev, enum sensor_channel ch,
			      enum sensor_attribute attr, const struct sensor_value *val)
{
	const struct eqdc_mcux_config *config = dev->config;
	struct eqdc_mcux_data *data = dev->data;

	if (ch != SENSOR_CHAN_ROTATION) {
		return -ENOTSUP;
	}

	switch ((enum sensor_attribute_eqdc_mcux)attr) {
	case SENSOR_ATTR_EQDC_MOD_VAL:
		if (!IN_RANGE(val->val1, 1, UINT32_MAX)) {
			LOG_ERR("SENSOR_ATTR_EQDC_MOD_VAL value invalid");
			return -EINVAL;
		}
		data->counts_per_revolution = val->val1;
		data->eqdc_config.positionModulusValue = data->counts_per_revolution - 1U;
		EQDC_SetPositionModulusValue(config->base, data->eqdc_config.positionModulusValue);
		return 0;

	case SENSOR_ATTR_EQDC_ENABLE_SINGLE_PHASE:
		data->eqdc_config.operateMode = int_to_work_mode(val->val1);
		EQDC_SetOperateMode(config->base, data->eqdc_config.operateMode);
		return 0;
	default:
		return -ENOTSUP;
	}
}

static int eqdc_mcux_attr_get(const struct device *dev, enum sensor_channel ch,
			      enum sensor_attribute attr, struct sensor_value *val)
{
	struct eqdc_mcux_data *data = dev->data;

	if (ch != SENSOR_CHAN_ROTATION) {
		return -ENOTSUP;
	}

	switch ((enum sensor_attribute_eqdc_mcux)attr) {
	case SENSOR_ATTR_EQDC_MOD_VAL:
		val->val1 = data->counts_per_revolution;
		return 0;
	case SENSOR_ATTR_EQDC_ENABLE_SINGLE_PHASE:
		val->val1 = data->eqdc_config.operateMode ==
				    kEQDC_QuadratureDecodeOperationMode ?
			    0 :
			    1;
		return 0;
	default:
		return -ENOTSUP;
	}
}

static int eqdc_mcux_fetch(const struct device *dev, enum sensor_channel ch)
{
	const struct eqdc_mcux_config *config = dev->config;
	struct eqdc_mcux_data *data = dev->data;

	if (ch != SENSOR_CHAN_ALL) {
		return -ENOTSUP;
	}

	data->position = (int32_t)EQDC_GetPosition(config->base);

	LOG_DBG("pos %d", data->position);

	return 0;
}

static int eqdc_mcux_ch_get(const struct device *dev, enum sensor_channel ch,
			   struct sensor_value *val)
{
	struct eqdc_mcux_data *data = dev->data;

	if (ch != SENSOR_CHAN_ROTATION) {
		return -ENOTSUP;
	}

	sensor_value_from_float(val, (data->position * 360.0f) /
						 data->counts_per_revolution);

	return 0;
}

static DEVICE_API(sensor, eqdc_mcux_api) = {
	.attr_set = &eqdc_mcux_attr_set,
	.attr_get = &eqdc_mcux_attr_get,
	.sample_fetch = &eqdc_mcux_fetch,
	.channel_get = &eqdc_mcux_ch_get,
};

static int eqdc_mcux_init(const struct device *dev)
{
	const struct eqdc_mcux_config *config = dev->config;
	struct eqdc_mcux_data *data = dev->data;
	int err;

	if (!device_is_ready(config->clock_dev)) {
        LOG_ERR("Clock control device not ready");
        return -ENODEV;
    }
	clock_control_on(config->clock_dev, config->clock_subsys);

	err = pinctrl_apply_state(config->pincfg, PINCTRL_STATE_DEFAULT);
	if (err != 0 && err != -ENOENT) {
		return err;
	}

	const struct eqdc_mcux_inputmux_entry *entry = &config->inputmux_entries[i];

	INPUTMUX_Init(entry->base);
	for (uint8_t i = 0; i < config->inputmux_entries_count; i++) {
		INPUTMUX_AttachSignal(entry->base, entry->channel, (inputmux_connection_t)entry->connection);
	}
	INPUTMUX_Deinit(entry->base);

	EQDC_GetDefaultConfig(&data->eqdc_config);
	data->eqdc_config.operateMode = int_to_work_mode(config->single_phase_mode);
	data->eqdc_config.filterSampleCount = config->filter_count;
	data->eqdc_config.filterSamplePeriod = config->filter_sample_period;
	data->eqdc_config.positionModulusValue = data->counts_per_revolution - 1U;
	EQDC_Init(config->base, &data->eqdc_config);
	EQDC_DoSoftwareLoadInitialPositionValue(config->base);

	return 0;
}
#define EQDC_CHECK_COND(n, p, min, max)                     \
    COND_CODE_1(DT_INST_NODE_HAS_PROP(n, p), (              \
            BUILD_ASSERT(IN_RANGE(DT_INST_PROP(n, p), min, max),    \
                 STRINGIFY(p) " value is out of range")), ())

#define EQDC_INPUTMUX_ENTRY(node_id, prop, idx)              \
    {                                                        \
     .base = (INPUTMUX_Type *)DT_REG_ADDR(DT_PHANDLE_BY_IDX(node_id, prop, idx)), \
     .channel = (uint16_t)DT_PHA_BY_IDX(node_id, prop, idx, mux), \
     .connection = (uint32_t)DT_PHA_BY_IDX(node_id, prop, idx, val), \
    }

#define EQDC_INPUTMUX_DEFINE(n)                                 \
    static const struct eqdc_inputmux_entry eqdc_mcux_inputmux_entries_##n[] = { \
        DT_INST_FOREACH_PROP_ELEM_SEP(n, inputmux_connections, EQDC_INPUTMUX_ENTRY, (,)) \
    };

#define EQDC_MCUX_INIT(n)                            \
    EQDC_INPUTMUX_DEFINE(n)                                  \
    EQDC_CHECK_COND(n, filter_count, 0, 7);      \
    BUILD_ASSERT(DT_INST_NODE_HAS_PROP(n, inputmux_connections), \
    "inputmux-connections must be defined in the Devicetree"); \
                                     \
    static struct eqdc_mcux_data eqdc_mcux_##n##_data = {            \
        .counts_per_revolution = DT_INST_PROP(n, counts_per_revolution), \
    };                                   \
                                     \
    PINCTRL_DT_INST_DEFINE(n);                       \
                                     \
    static const struct eqdc_mcux_config eqdc_mcux_##n##_config = {      \
        .base = (EQDC_Type *)DT_INST_REG_ADDR(n),            \
        .pincfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),             \
        .clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(n)),         \     \
        .clock_subsys = (clock_control_subsys_t)DT_INST_CLOCKS_CELL(n, name), \
        .filter_count = DT_INST_PROP_OR(n, filter_count, 0),         \
        .filter_sample_period = DT_INST_PROP_OR(n, filter_sample_period, 0), \
        .single_phase_mode = DT_INST_PROP(n, single_phase_mode),         \
        .inputmux_entries = eqdc_mcux_inputmux_entries_##n,               \
        .inputmux_entries_count = ARRAY_SIZE(eqdc_mcux_inputmux_entries_##n), \
    };                                   \
                                     \
    SENSOR_DEVICE_DT_INST_DEFINE(n, eqdc_mcux_init, NULL,            \
                     &eqdc_mcux_##n##_data,          \
                     &eqdc_mcux_##n##_config, POST_KERNEL,   \
                     CONFIG_SENSOR_INIT_PRIORITY, &eqdc_mcux_api);

DT_INST_FOREACH_STATUS_OKAY(EQDC_MCUX_INIT)