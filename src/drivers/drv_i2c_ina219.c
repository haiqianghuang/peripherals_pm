/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../pm_core.h"

struct ina219_priv {
    char dev_path[64];
    uint8_t addr;
    int fd;
};

static int ina219_init(struct pm_dev *dev)
{
    struct ina219_priv *priv = dev->priv_data;

    printf("[INA219] init: %s, dev=%s, addr=0x%02X\n",
        dev->name, priv->dev_path, priv->addr);

    /* TODO: configure INA219 registers */
    priv->fd = -1;

    return 0;
}

static int ina219_read(struct pm_dev *dev, struct pm_state *state)
{
    struct ina219_priv *priv = dev->priv_data;
    uint16_t voltage_reg = 0, current_reg = 0;

    /* TODO: read INA219 voltage and current registers */
    (void)priv;

    state->voltage = (float)voltage_reg * 0.001f; /* LSB = 1mV */
    state->current = (float)current_reg * 0.001f; /* LSB = 1mA */
    state->power = state->voltage * state->current;
    state->percentage = (state->voltage - dev->config.min_voltage) /
                (dev->config.max_voltage - dev->config.min_voltage) * 100.0f;
    state->status = PM_STATUS_DISCHARGING;
    state->timestamp_us = 0; /* TODO: get timestamp */

    return 0;
}

static int ina219_switch_set(struct pm_dev *dev, const char *ch, bool enable)
{
    (void)dev;
    (void)ch;
    (void)enable;
    /* INA219 doesn't have switch control */
    return -ENOSYS;
}

static bool ina219_switch_get(struct pm_dev *dev, const char *ch)
{
    (void)dev;
    (void)ch;
    return false;
}

static void ina219_free(struct pm_dev *dev)
{
    if (!dev) return;
    printf("[INA219] free: %s\n", dev->name);
    if (dev->priv_data) free(dev->priv_data);
    if (dev->name) free((void *)dev->name);
    free(dev);
}

static const struct pm_ops ina219_ops = {
    .init = ina219_init,
    .read = ina219_read,
    .switch_set = ina219_switch_set,
    .switch_get = ina219_switch_get,
    .free = ina219_free,
};

static struct pm_dev *ina219_create(void *args)
{
    struct pm_args_i2c *a = (struct pm_args_i2c *)args;
    struct pm_dev *dev;
    struct ina219_priv *priv;

    if (!a || !a->instance || !a->dev_path)
        return NULL;

    dev = pm_dev_alloc(a->instance, sizeof(*priv));
    if (!dev)
        return NULL;

    priv = dev->priv_data;
    dev->ops = &ina219_ops;

    strncpy(priv->dev_path, a->dev_path, sizeof(priv->dev_path) - 1);
    priv->dev_path[sizeof(priv->dev_path) - 1] = '\0';
    priv->addr = a->addr;
    priv->fd = -1;

    return dev;
}

REGISTER_PM_DRIVER("INA219", PM_DRV_I2C, ina219_create);
