/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pm_core.h"

struct mock_pm_priv {
    int initialized;
    bool switch_enabled;
    int callback_set_count;
};

static int g_failures;
static int g_free_count;
static int g_callback_count;
static struct pm_state g_last_callback_state;

#define CHECK_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL:%s:%d: expected true: %s\n", __FILE__, __LINE__, #expr); \
        g_failures++; \
    } \
} while (0)

#define CHECK_INT_EQ(actual, expected) do { \
    int _actual = (int)(actual); \
    int _expected = (int)(expected); \
    if (_actual != _expected) { \
        printf("FAIL:%s:%d: expected %s == %d, got %d\n", \
            __FILE__, __LINE__, #actual, _expected, _actual); \
        g_failures++; \
    } \
} while (0)

#define CHECK_FLOAT_EQ(actual, expected) do { \
    float _actual = (actual); \
    float _expected = (expected); \
    if (_actual < _expected - 0.001f || _actual > _expected + 0.001f) { \
        printf("FAIL:%s:%d: expected %s == %.3f, got %.3f\n", \
            __FILE__, __LINE__, #actual, _expected, _actual); \
        g_failures++; \
    } \
} while (0)

static void reset_test_state(void)
{
    g_failures = 0;
    g_free_count = 0;
    g_callback_count = 0;
    memset(&g_last_callback_state, 0, sizeof(g_last_callback_state));
}

static void pm_cb(struct pm_dev *dev, const struct pm_state *state, void *ctx)
{
    int *seen = ctx;

    CHECK_TRUE(dev != NULL);
    CHECK_TRUE(state != NULL);
    CHECK_TRUE(seen != NULL);

    if (seen)
        (*seen)++;
    if (state) {
        g_callback_count++;
        g_last_callback_state = *state;
    }
}

static int mock_init(struct pm_dev *dev)
{
    struct mock_pm_priv *priv;

    if (!dev || !dev->priv_data)
        return -EINVAL;

    priv = dev->priv_data;
    priv->initialized = 1;
    return 0;
}

static int mock_read(struct pm_dev *dev, struct pm_state *state)
{
    struct mock_pm_priv *priv;

    if (!dev || !dev->priv_data || !state)
        return -EINVAL;

    priv = dev->priv_data;
    if (!priv->initialized)
        return -EAGAIN;

    state->voltage = 11.8f;
    state->current = 1.5f;
    state->power = state->voltage * state->current;
    state->percentage = 66.0f;
    state->temperature = 32.0f;
    state->health = 98.0f;
    state->status = priv->switch_enabled ? PM_STATUS_CHARGING : PM_STATUS_DISCHARGING;
    state->cell_count = 2;
    state->cell_voltages[0] = 3.9f;
    state->cell_voltages[1] = 3.8f;

    if (dev->cb)
        dev->cb(dev, state, dev->cb_ctx);

    return 0;
}

static int mock_switch_set(struct pm_dev *dev, const char *ch, bool enable)
{
    struct mock_pm_priv *priv;

    if (!dev || !dev->priv_data || !ch)
        return -EINVAL;
    if (strcmp(ch, "main") != 0)
        return -ENOENT;

    priv = dev->priv_data;
    priv->switch_enabled = enable;
    return 0;
}

static bool mock_switch_get(struct pm_dev *dev, const char *ch)
{
    struct mock_pm_priv *priv;

    if (!dev || !dev->priv_data || !ch)
        return false;
    if (strcmp(ch, "main") != 0)
        return false;

    priv = dev->priv_data;
    return priv->switch_enabled;
}

static void mock_set_callback(struct pm_dev *dev, pm_callback_t cb, void *ctx)
{
    struct mock_pm_priv *priv;

    (void)cb;
    (void)ctx;

    if (!dev || !dev->priv_data)
        return;

    priv = dev->priv_data;
    priv->callback_set_count++;
}

static void mock_free(struct pm_dev *dev)
{
    if (!dev)
        return;

    g_free_count++;
    free(dev->priv_data);
    free((void *)dev->name);
    free(dev);
}

static const struct pm_ops mock_ops = {
    .init = mock_init,
    .read = mock_read,
    .switch_set = mock_switch_set,
    .switch_get = mock_switch_get,
    .free = mock_free,
    .set_callback = mock_set_callback,
};

static struct pm_dev *mock_factory(void *args)
{
    struct pm_args_generic *pm_args = args;
    struct pm_dev *dev;

    if (!pm_args || !pm_args->instance || !pm_args->charger_node ||
        !pm_args->capacity_node)
        return NULL;

    dev = pm_dev_alloc(pm_args->instance, sizeof(struct mock_pm_priv));
    if (!dev)
        return NULL;

    dev->ops = &mock_ops;
    return dev;
}

REGISTER_PM_DRIVER("MOCK", PM_DRV_GENERIC, mock_factory);

static void test_error_paths(void)
{
    struct pm_state state;
    struct pm_dev *dev;

    CHECK_INT_EQ(pm_init(NULL, NULL), -EINVAL);
    CHECK_INT_EQ(pm_get_state(NULL, &state), -EINVAL);
    CHECK_INT_EQ(pm_get_state(NULL, NULL), -EINVAL);
    CHECK_INT_EQ(pm_switch_set(NULL, "main", true), -EINVAL);
    CHECK_TRUE(!pm_switch_get(NULL, "main"));
    pm_set_callback(NULL, pm_cb, NULL);
    pm_free(NULL);

    CHECK_TRUE(pm_alloc_generic(NULL, "charger", "battery", NULL) == NULL);
    CHECK_TRUE(pm_alloc_generic("MOCK:pm0", NULL, "battery", NULL) == NULL);
    CHECK_TRUE(pm_alloc_generic("MOCK:pm0", "charger", NULL, NULL) == NULL);
    CHECK_TRUE(pm_alloc_generic("MOCK:", "charger", "battery", NULL) == NULL);
    CHECK_TRUE(pm_alloc_generic(":pm0", "charger", "battery", NULL) == NULL);
    CHECK_TRUE(pm_alloc_generic("MISSING:pm0", "charger", "battery", NULL) == NULL);
    CHECK_TRUE(pm_alloc_digital("MOCK:pm0", NULL, "/dev/i2c-0", 0x40) == NULL);

    dev = pm_alloc_generic("MOCK:not-ready", "charger", "battery", NULL);
    CHECK_TRUE(dev != NULL);
    if (!dev)
        return;

    CHECK_INT_EQ(pm_get_state(dev, &state), -EAGAIN);
    CHECK_INT_EQ(pm_get_state(dev, NULL), -EINVAL);
    CHECK_INT_EQ(pm_switch_set(dev, NULL, true), -EINVAL);
    CHECK_INT_EQ(pm_init(dev, NULL), 0);
    CHECK_INT_EQ(pm_switch_set(dev, "missing", true), -ENOENT);
    CHECK_TRUE(!pm_switch_get(dev, "missing"));
    pm_free(dev);
    CHECK_INT_EQ(g_free_count, 1);
}

static void test_functional(void)
{
    struct pm_dev *dev;
    struct pm_state state;
    struct pm_config cfg = {
        .capacity_mah = 6000.0f,
        .max_voltage = 12.6f,
        .min_voltage = 9.0f,
        .warn_voltage = 10.0f,
        .crit_voltage = 9.5f,
        .max_temp = 55.0f,
    };
    int local_callbacks = 0;

    dev = pm_alloc_generic("MOCK:pm0", "charger", "battery", NULL);
    CHECK_TRUE(dev != NULL);
    if (!dev)
        return;

    CHECK_INT_EQ(pm_init(dev, &cfg), 0);
    CHECK_FLOAT_EQ(dev->config.capacity_mah, 6000.0f);
    pm_set_callback(dev, pm_cb, &local_callbacks);
    CHECK_INT_EQ(pm_start(dev, 10), 0);
    CHECK_INT_EQ(pm_switch_set(dev, "main", true), 0);
    CHECK_TRUE(pm_switch_get(dev, "main"));
    CHECK_INT_EQ(pm_get_state(dev, &state), 0);
    CHECK_FLOAT_EQ(state.voltage, 11.8f);
    CHECK_FLOAT_EQ(state.percentage, 66.0f);
    CHECK_INT_EQ(state.status, PM_STATUS_CHARGING);
    CHECK_INT_EQ(state.cell_count, 2);
    CHECK_INT_EQ(local_callbacks, 1);
    CHECK_INT_EQ(g_callback_count, 1);
    CHECK_FLOAT_EQ(g_last_callback_state.power, 17.7f);
    CHECK_INT_EQ(pm_switch_set(dev, "main", false), 0);
    CHECK_TRUE(!pm_switch_get(dev, "main"));

    pm_free(dev);
    CHECK_INT_EQ(g_free_count, 1);
}

static int finish_test(const char *name)
{
    if (g_failures != 0) {
        printf("%s FAILED: %d failure(s)\n", name, g_failures);
        return 1;
    }
    printf("%s PASSED\n", name);
    return 0;
}

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "all";

    if (strcmp(mode, "functional") == 0) {
        reset_test_state();
        test_functional();
        return finish_test("pm api functional test");
    }
    if (strcmp(mode, "error-paths") == 0) {
        reset_test_state();
        test_error_paths();
        return finish_test("pm api error paths test");
    }
    if (strcmp(mode, "all") == 0) {
        reset_test_state();
        test_functional();
        if (finish_test("pm api functional test") != 0)
            return 1;
        reset_test_state();
        test_error_paths();
        if (finish_test("pm api error paths test") != 0)
            return 1;
        printf("pm api contract test PASSED\n");
        return 0;
    }

    fprintf(stderr, "usage: %s [all|functional|error-paths]\n", argv[0]);
    return 2;
}
