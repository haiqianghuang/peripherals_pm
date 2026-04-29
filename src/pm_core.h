/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PM_CORE_H
#define PM_CORE_H

/*
 * Private header for PM component (motor-like minimal style).
 */

#include "../include/pm.h"
#include <stddef.h>

/* 1. 参数适配包 */
struct pm_args_adc {
    const char *instance;
    const char *dev_path;
    float scale;
};

struct pm_args_i2c {
    const char *instance;
    const char *dev_path;
    uint8_t addr;
};

struct pm_args_generic {
    const char *instance;
    const char *charger_node;
    const char *capacity_node;
    void *args;
};

/* 2. 驱动类型枚举 */
enum pm_driver_type {
    PM_DRV_ADC = 0,
    PM_DRV_I2C,
    PM_DRV_GENERIC,
};

/* 3. 虚函数表（驱动实现） */
struct pm_ops {
    int (*init)(struct pm_dev *dev);
    int (*read)(struct pm_dev *dev, struct pm_state *state);
    int (*switch_set)(struct pm_dev *dev, const char *ch, bool enable);
    bool (*switch_get)(struct pm_dev *dev, const char *ch);
    void (*free)(struct pm_dev *dev);
    void (*set_callback)(struct pm_dev *dev, pm_callback_t cb, void *ctx);
};

/* 4. 设备对象（私有实现） */
struct pm_dev {
    const char *name; /* instance name */
    struct pm_config config;
    const struct pm_ops *ops;
    void *priv_data;
    pm_callback_t cb;
    void *cb_ctx;
};

/* 5. 通用工厂函数类型 */
typedef struct pm_dev *(*pm_factory_t)(void *args);

/* 6. 注册节点结构 */
struct driver_info {
    const char *name;
    enum pm_driver_type type;
    pm_factory_t factory;
    struct driver_info *next;
};

void pm_driver_register(struct driver_info *info);

#define REGISTER_PM_DRIVER(_name, _type, _factory) \
    static struct driver_info __drv_info_##_factory = { \
        .name = _name, \
        .type = _type, \
        .factory = _factory, \
        .next = 0 \
    }; \
    __attribute__((constructor)) \
    static void __auto_reg_##_factory(void) { \
        pm_driver_register(&__drv_info_##_factory); \
    }

struct pm_dev *pm_dev_alloc(const char *instance, size_t priv_size);

#endif /* PM_CORE_H */
