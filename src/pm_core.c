/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pm_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>

int pm_init(struct pm_dev *dev, const struct pm_config *cfg)
{
    if (!dev || !dev->ops || !dev->ops->init)
        return -EINVAL;

    if (cfg) {
        dev->config = *cfg;
    } else {
        /* default config */
        memset(&dev->config, 0, sizeof(struct pm_config));
        dev->config.capacity_mah = 5000.0f;
        dev->config.max_voltage = 12.6f;
        dev->config.min_voltage = 9.0f;
        dev->config.warn_voltage = 10.0f;
        dev->config.crit_voltage = 9.5f;
        dev->config.max_temp = 60.0f;
    }

    return dev->ops->init(dev);
}

void pm_set_callback(struct pm_dev *dev, pm_callback_t cb, void *ctx)
{
    if (!dev)
        return;

    dev->cb = cb;
    dev->cb_ctx = ctx;

    if (dev->ops && dev->ops->set_callback)
        dev->ops->set_callback(dev, cb, ctx);
}

int pm_start(struct pm_dev *dev, uint32_t freq_hz)
{
    (void)dev;
    (void)freq_hz;
    /* TODO: start periodic polling if needed */
    return 0;
}

int pm_get_state(struct pm_dev *dev, struct pm_state *out_state)
{
    if (!dev || !dev->ops || !dev->ops->read || !out_state)
        return -EINVAL;

    memset(out_state, 0, sizeof(*out_state));
    return dev->ops->read(dev, out_state);
}

void pm_free(struct pm_dev *dev)
{
    if (!dev)
        return;

    if (dev->ops && dev->ops->free) {
        dev->ops->free(dev);
        return;
    }

    if (dev->priv_data) free(dev->priv_data);
    if (dev->name) free((void *)dev->name);
    free(dev);
}

int pm_switch_set(struct pm_dev *dev, const char *channel_name, bool enable)
{
    if (!dev || !dev->ops || !dev->ops->switch_set || !channel_name)
        return -EINVAL;

    return dev->ops->switch_set(dev, channel_name, enable);
}

bool pm_switch_get(struct pm_dev *dev, const char *channel_name)
{
    if (!dev || !dev->ops || !dev->ops->switch_get || !channel_name)
        return false;

    return dev->ops->switch_get(dev, channel_name);
}

struct pm_dev *pm_dev_alloc(const char *name, size_t priv_size)
{
    struct pm_dev *dev;
    void *priv = NULL;
    char *name_copy = NULL;

    dev = calloc(1, sizeof(*dev));
    if (!dev)
        return NULL;

    if (priv_size) {
        priv = calloc(1, priv_size);
        if (!priv) {
            free(dev);
            return NULL;
        }
        dev->priv_data = priv;
    }

    if (name) {
        size_t n = strlen(name);
        name_copy = calloc(1, n + 1);
        if (!name_copy) {
            free(priv);
            free(dev);
            return NULL;
        }
        memcpy(name_copy, name, n);
        name_copy[n] = '\0';
        dev->name = name_copy;
    }

    return dev;
}

/* --- driver registry (minimal, motor-like) --- */

static struct driver_info *g_driver_list = NULL;

void pm_driver_register(struct driver_info *info)
{
    if (!info)
        return;
    info->next = g_driver_list;
    g_driver_list = info;
}

static struct driver_info *find_driver(const char *name, enum pm_driver_type type)
{
    struct driver_info *curr = g_driver_list;
    while (curr) {
        if (curr->name && name && strcmp(curr->name, name) == 0) {
            if (curr->type == type)
                return curr;
            printf("[PM] driver '%s' type mismatch (expected %d got %d)\n",
                    name, (int)type, (int)curr->type);
            return NULL;
        }
        curr = curr->next;
    }
    printf("[PM] driver '%s' not found\n", name ? name : "(null)");
    return NULL;
}

static int split_driver_instance(const char *name,
        char *driver, size_t driver_sz,
        const char **instance)
{
    const char *sep;
    size_t len;

    if (!name || !driver || !driver_sz || !instance)
        return -EINVAL;

    sep = strchr(name, ':');
    if (!sep)
        return 0;

    len = (size_t)(sep - name);
    if (len == 0 || len + 1 > driver_sz || !*(sep + 1))
        return -EINVAL;

    memcpy(driver, name, len);
    driver[len] = '\0';
    *instance = sep + 1;
    return 1;
}

/* --- factory functions (public API) --- */

struct pm_dev *pm_alloc_adc(const char *name, const char *adc_dev, float scale)
{
    struct driver_info *drv;
    struct pm_args_adc args;
    char driver[64];
    const char *instance = NULL;
    int r;

    if (!name || !adc_dev)
        return NULL;

    /* default: name is instance, driver is ADC-Linux */
    strncpy(driver, "ADC-Linux", sizeof(driver) - 1);
    driver[sizeof(driver) - 1] = '\0';
    instance = name;

    r = split_driver_instance(name, driver, sizeof(driver), &instance);
    if (r < 0)
        return NULL;

    drv = find_driver(driver, PM_DRV_ADC);
    if (!drv || !drv->factory)
        return NULL;

    args.instance = instance;
    args.dev_path = adc_dev;
    args.scale = scale;
    return drv->factory(&args);
}

struct pm_dev *pm_alloc_digital(const char *name, const char *protocol,
                const char *dev_path, uint32_t addr)
{
    struct driver_info *drv;
    struct pm_args_i2c args;
    char driver[64];
    const char *instance = NULL;
    int r;

    if (!name || !dev_path)
        return NULL;

    /* default: protocol selects driver; fallback to INA219 */
    if (protocol && *protocol) {
        strncpy(driver, protocol, sizeof(driver) - 1);
        driver[sizeof(driver) - 1] = '\0';
        instance = name;
    } else {
        strncpy(driver, "INA219", sizeof(driver) - 1);
        driver[sizeof(driver) - 1] = '\0';
        instance = name;

        r = split_driver_instance(name, driver, sizeof(driver), &instance);
        if (r < 0)
            return NULL;
    }

    drv = find_driver(driver, PM_DRV_I2C);
    if (!drv || !drv->factory)
        return NULL;

    args.instance = instance;
    args.dev_path = dev_path;
    args.addr = (uint8_t)addr;
    return drv->factory(&args);
}

struct pm_dev *pm_alloc_generic(const char *name, const char *charger_node,
                const char *capacity_node, void *_args)
{
    struct driver_info *drv;
    struct pm_args_generic args;
    char driver[64];
    const char *instance = NULL;
    int r;

    if (!name || !charger_node || !capacity_node)
        return NULL;

    /* default: name is instance, driver is GENERIC */
    strncpy(driver, "GENERIC", sizeof(driver) - 1);
    driver[sizeof(driver) - 1] = '\0';
    instance = name;

    r = split_driver_instance(name, driver, sizeof(driver), &instance);
    if (r < 0)
        return NULL;

    drv = find_driver(driver, PM_DRV_GENERIC);
    if (!drv || !drv->factory)
        return NULL;

    args.instance = instance;
    args.charger_node = charger_node;
    args.capacity_node = capacity_node;
    args.args = _args;
    return drv->factory(&args);
}
