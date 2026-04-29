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

struct adc_priv {
    char dev_path[64];
    float scale;
    int fd;
};

static int adc_linux_init(struct pm_dev *dev)
{
    struct adc_priv *priv = dev->priv_data;

    printf("[ADC-Linux] init: %s, dev=%s, scale=%.2f\n",
        dev->name, priv->dev_path, priv->scale);

    /* TODO: open Linux ADC device (e.g., /sys/bus/iio/devices/iio:device0) */
    priv->fd = -1;

    return 0;
}

static int adc_linux_read(struct pm_dev *dev, struct pm_state *state)
{
    struct adc_priv *priv = dev->priv_data;
    float raw_value = 0.0f;

    /* TODO: read from Linux ADC */
    state->voltage = raw_value * priv->scale;
    state->current = 0.0f;
    state->power = state->voltage * state->current;
    state->percentage = (state->voltage - dev->config.min_voltage) /
                (dev->config.max_voltage - dev->config.min_voltage) * 100.0f;
    state->status = PM_STATUS_DISCHARGING;
    state->timestamp_us = 0; /* TODO: get timestamp */

    return 0;
}

static int adc_linux_switch_set(struct pm_dev *dev, const char *ch, bool enable)
{
    (void)dev;
    (void)ch;
    (void)enable;
    /* TODO: control power switch via GPIO */
    return -ENOSYS;
}

static bool adc_linux_switch_get(struct pm_dev *dev, const char *ch)
{
    (void)dev;
    (void)ch;
    /* TODO: read power switch state via GPIO */
    return false;
}

static void adc_linux_free(struct pm_dev *dev)
{
    if (!dev) return;
    printf("[ADC-Linux] free: %s\n", dev->name);
    if (dev->priv_data) free(dev->priv_data);
    if (dev->name) free((void *)dev->name);
    free(dev);
}

static const struct pm_ops adc_linux_ops2 = {
    .init = adc_linux_init,
    .read = adc_linux_read,
    .switch_set = adc_linux_switch_set,
    .switch_get = adc_linux_switch_get,
    .free = adc_linux_free,
};

static struct pm_dev *adc_linux_create(void *args)
{
    struct pm_args_adc *a = (struct pm_args_adc *)args;
    struct pm_dev *dev;
    struct adc_priv *priv;

    if (!a || !a->instance || !a->dev_path)
        return NULL;

    dev = pm_dev_alloc(a->instance, sizeof(*priv));
    if (!dev)
        return NULL;

    priv = dev->priv_data;
    dev->ops = &adc_linux_ops2;

    strncpy(priv->dev_path, a->dev_path, sizeof(priv->dev_path) - 1);
    priv->dev_path[sizeof(priv->dev_path) - 1] = '\0';
    priv->scale = a->scale;
    priv->fd = -1;

    return dev;
}

REGISTER_PM_DRIVER("ADC-Linux", PM_DRV_ADC, adc_linux_create);
