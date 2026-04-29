/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "../include/pm.h"

static void on_power_event(struct pm_dev *dev, const struct pm_state *state, void *ctx)
{
    (void)dev;
    (void)ctx;

    if (state->percentage < 10.0f) {
        printf("[WARNING] Low Battery: %.1f%%! Please charge.\n",
            state->percentage);
    }

    if (state->status == PM_STATUS_CHARGING) {
        printf("[INFO] Charging... Current: %.2f A\n", state->current);
    }
}

int main(void)
{
    struct pm_dev *batt;
    struct pm_config cfg;
    int ret;

    printf("=== PM ADC Test ===\n\n");

    /* 1. Create ADC-based battery instance */
    /* Assume 3S LiPo (12.6V) with 11:1 voltage divider */
    printf("[1] Creating ADC PM device...\n");
    batt = pm_alloc_adc("main_batt", "/dev/adc1", 11.0f);
    if (!batt) {
        fprintf(stderr, "Failed to create PM device\n");
        return -1;
    }
    printf("   PM device created\n\n");

    /* 2. Configure parameters */
    printf("[2] Configuring PM device...\n");
    cfg = (struct pm_config){
        .max_voltage = 12.6f,
        .min_voltage = 10.5f,
        .capacity_mah = 5000.0f,
        .warn_voltage = 11.0f,
        .crit_voltage = 10.0f,
        .max_temp = 60.0f,
    };

    ret = pm_init(batt, &cfg);
    if (ret < 0) {
        fprintf(stderr, "Failed to init PM device: %d\n", ret);
        goto cleanup;
    }
    printf("   PM device initialized\n\n");

    /* 3. Register callback */
    printf("[3] Registering callback...\n");
    pm_set_callback(batt, on_power_event, NULL);
    printf("   Callback registered\n\n");

    /* 4. Start monitoring */
    printf("[4] Starting PM monitoring (1Hz)...\n");
    ret = pm_start(batt, 1);
    if (ret < 0) {
        fprintf(stderr, "Failed to start PM: %d\n", ret);
        goto cleanup;
    }
    printf("   Monitoring started\n\n");

    /* 5. Optional: Control power switch (if supported) */
    printf("[5] Setting power switch...\n");
    ret = pm_switch_set(batt, "aux_12v", true);
    if (ret < 0) {
        printf("   Power switch control not supported\n");
    } else {
        printf("   Power switch enabled\n");
    }
    printf("\n");

    printf("=== Test running (demo: 3 cycles) ===\n\n");

    /* Main loop (demo) */
    for (int i = 0; i < 3; i++) {
        struct pm_state st;
        if (pm_get_state(batt, &st) == 0) {
            printf("[state] V=%.2fV I=%.2fA SOC=%.1f%% status=%d\n",
                st.voltage, st.current, st.percentage, st.status);
        }
        sleep(1);
    }

cleanup:
    pm_free(batt);
    return ret;
}
