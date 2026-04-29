/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "../pm_core.h"

struct generic_priv {
    char online_path[128];
    char capacity_path[128];
    pthread_t cb_thread;
    pthread_mutex_t cb_lock;
    bool cb_lock_init;
    bool cb_thread_running;
    bool cb_thread_stop;
    enum pm_status last_status;
    bool last_valid;
};

static int generic_read(struct pm_dev *dev, struct pm_state *state);
static void generic_set_callback(struct pm_dev *dev, pm_callback_t cb, void *ctx);

static void *generic_cb_thread_fn(void *arg)
{
    struct pm_dev *dev = (struct pm_dev *)arg;
    struct generic_priv *priv = dev->priv_data;
    struct pm_state state;

    for (;;) {
        bool stop;

        pthread_mutex_lock(&priv->cb_lock);
        stop = priv->cb_thread_stop;
        pthread_mutex_unlock(&priv->cb_lock);

        if (stop)
            break;

        if (dev->cb && generic_read(dev, &state) == 0) {
            if (priv->last_valid && state.status != priv->last_status) {
                pm_callback_t cb = dev->cb;
                void *ctx = dev->cb_ctx;

                if (cb)
                    cb(dev, &state, ctx);
            }
            priv->last_status = state.status;
            priv->last_valid = true;
        }

        usleep(1000U * 1000U);
    }

    pthread_mutex_lock(&priv->cb_lock);
    priv->cb_thread_running = false;
    pthread_mutex_unlock(&priv->cb_lock);

    return NULL;
}

static int generic_cb_thread_start(struct pm_dev *dev)
{
    struct generic_priv *priv = dev->priv_data;
    int ret;

    if (!priv->cb_lock_init)
        return -EINVAL;

    pthread_mutex_lock(&priv->cb_lock);
    if (priv->cb_thread_running) {
        pthread_mutex_unlock(&priv->cb_lock);
        return 0;
    }
    priv->cb_thread_stop = false;
    priv->cb_thread_running = true;
    pthread_mutex_unlock(&priv->cb_lock);

    ret = pthread_create(&priv->cb_thread, NULL, generic_cb_thread_fn, dev);
    if (ret != 0) {
        pthread_mutex_lock(&priv->cb_lock);
        priv->cb_thread_running = false;
        pthread_mutex_unlock(&priv->cb_lock);
        return -ret;
    }

    return 0;
}

static void generic_cb_thread_stop(struct pm_dev *dev)
{
    struct generic_priv *priv = dev->priv_data;
    bool running = false;

    if (!priv->cb_lock_init)
        return;

    pthread_mutex_lock(&priv->cb_lock);
    if (priv->cb_thread_running) {
        priv->cb_thread_stop = true;
        running = true;
    }
    pthread_mutex_unlock(&priv->cb_lock);

    if (!running)
        return;

    if (pthread_equal(pthread_self(), priv->cb_thread))
        return;

    pthread_join(priv->cb_thread, NULL);
}

static int read_int_from_file(const char *path, int *out_value)
{
    char buf[32];
    char *end = NULL;
    FILE *fp;
    int64_t val;

    if (!path || !out_value)
        return -EINVAL;

    fp = fopen(path, "r");
    if (!fp)
        return -errno;

    if (!fgets(buf, sizeof(buf), fp)) {
        int err = ferror(fp) ? errno : EIO;
        fclose(fp);
        return -err;
    }

    fclose(fp);

    val = strtoll(buf, &end, 10);
    if (end == buf)
        return -EINVAL;

    *out_value = (int)val;
    return 0;
}

static int generic_init(struct pm_dev *dev)
{
    struct generic_priv *priv = dev->priv_data;

    printf("[GENERIC] init: %s\n", dev->name);
    printf("[GENERIC] online=%s capacity=%s\n",
        priv->online_path, priv->capacity_path);

    return 0;
}

static int generic_read(struct pm_dev *dev, struct pm_state *state)
{
    struct generic_priv *priv = dev->priv_data;
    int online = 0;
    int capacity = 0;
    int ret;

    ret = read_int_from_file(priv->online_path, &online);
    if (ret < 0) {
        state->status = PM_STATUS_UNKNOWN;
        return ret;
    }

    ret = read_int_from_file(priv->capacity_path, &capacity);
    if (ret < 0) {
        state->status = PM_STATUS_UNKNOWN;
        return ret;
    }

    if (capacity < 0)
        capacity = 0;
    if (capacity > 100)
        capacity = 100;

    state->percentage = (float)capacity;
    state->status = (online > 0) ? PM_STATUS_CHARGING : PM_STATUS_DISCHARGING;
    state->timestamp_us = 0;

    return 0;
}

static int generic_switch_set(struct pm_dev *dev, const char *ch, bool enable)
{
    (void)dev;
    (void)ch;
    (void)enable;
    return -ENOSYS;
}

static bool generic_switch_get(struct pm_dev *dev, const char *ch)
{
    (void)dev;
    (void)ch;
    return false;
}

static void generic_free(struct pm_dev *dev)
{
    if (!dev)
        return;
    if (dev->priv_data) {
        struct generic_priv *priv = dev->priv_data;
        generic_cb_thread_stop(dev);
        if (priv->cb_lock_init) {
            pthread_mutex_destroy(&priv->cb_lock);
            priv->cb_lock_init = false;
        }
    }
    printf("[GENERIC] free: %s\n", dev->name);
    if (dev->priv_data)
        free(dev->priv_data);
    if (dev->name)
        free((void *)dev->name);
    free(dev);
}

static const struct pm_ops generic_ops = {
    .init = generic_init,
    .read = generic_read,
    .switch_set = generic_switch_set,
    .switch_get = generic_switch_get,
    .free = generic_free,
    .set_callback = generic_set_callback,
};

static void generic_set_callback(struct pm_dev *dev, pm_callback_t cb, void *ctx)
{
    struct generic_priv *priv;

    (void)ctx;

    if (!dev)
        return;

    priv = dev->priv_data;

    if (cb) {
        priv->last_valid = false;
        priv->last_status = PM_STATUS_UNKNOWN;
        generic_cb_thread_start(dev);
    } else {
        generic_cb_thread_stop(dev);
    }
}

static struct pm_dev *generic_create(void *args)
{
    struct pm_args_generic *a = (struct pm_args_generic *)args;
    struct pm_dev *dev;
    struct generic_priv *priv;
    int n;

    if (!a || !a->instance || !a->charger_node || !a->capacity_node)
        return NULL;

    dev = pm_dev_alloc(a->instance, sizeof(*priv));
    if (!dev)
        return NULL;

    priv = dev->priv_data;
    dev->ops = &generic_ops;
    priv->cb_lock_init = false;
    priv->cb_thread_running = false;
    priv->cb_thread_stop = false;
    priv->last_status = PM_STATUS_UNKNOWN;
    priv->last_valid = false;

    if (pthread_mutex_init(&priv->cb_lock, NULL) != 0) {
        if (dev->priv_data)
            free(dev->priv_data);
        if (dev->name)
            free((void *)dev->name);
        free(dev);
        return NULL;
    }
    priv->cb_lock_init = true;

    n = snprintf(priv->online_path, sizeof(priv->online_path),
        "/sys/class/power_supply/%s/online", a->charger_node);
    if (n < 0 || (size_t)n >= sizeof(priv->online_path)) {
        generic_free(dev);
        return NULL;
    }

    n = snprintf(priv->capacity_path, sizeof(priv->capacity_path),
        "/sys/class/power_supply/%s/capacity", a->capacity_node);
    if (n < 0 || (size_t)n >= sizeof(priv->capacity_path)) {
        generic_free(dev);
        return NULL;
    }

    return dev;
}

REGISTER_PM_DRIVER("GENERIC", PM_DRV_GENERIC, generic_create);
