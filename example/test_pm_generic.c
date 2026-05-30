/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/pm.h"

static void on_power_event(struct pm_dev *dev, const struct pm_state *state, void *ctx)
{
    (void)dev;
    (void)ctx;

    if (state->status == PM_STATUS_CHARGING) {
        printf("[callback] Charging, SOC=%.1f%%\n", state->percentage);
    } else if (state->status == PM_STATUS_DISCHARGING) {
        printf("[callback] Discharging, SOC=%.1f%%\n", state->percentage);
    } else {
        printf("[callback] Status=%d, SOC=%.1f%%\n", state->status, state->percentage);
    }
}

static void print_usage(const char *prog)
{
    printf("Usage: %s [charger_node] [capacity_node]\n", prog);
    printf("Default charger_node=ip2317-charger capacity_node=cw-bat\n");
}

int main(int argc, char **argv)
{
    const char *charger_node = "ip2317-charger";
    const char *capacity_node = "cw-bat";
    struct pm_dev *batt;
    int ret;

    if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        print_usage(argv[0]);
        return 0;
    }

    if (argc > 1)
        charger_node = argv[1];
    if (argc > 2)
        capacity_node = argv[2];

    printf("=== PM GENERIC Test ===\n\n");
    printf("[1] Creating GENERIC PM device...\n");
    batt = pm_alloc_generic("main_batt", charger_node, capacity_node, NULL);
    if (!batt) {
        fprintf(stderr, "Failed to create PM device\n");
        return -1;
    }
    printf("   PM device created\n\n");

    printf("[2] Initializing PM device...\n");
    ret = pm_init(batt, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to init PM device: %d\n", ret);
        pm_free(batt);
        return ret;
    }
    printf("   PM device initialized\n\n");

    printf("[3] Registering callback...\n");
    pm_set_callback(batt, on_power_event, NULL);
    printf("   Callback registered\n\n");

    printf("=== Test running (demo: 3 cycles) ===\n\n");
    for (int i = 0; i < 20; i++) {
        struct pm_state st;
        if (pm_get_state(batt, &st) == 0) {
            printf("[state] SOC=%.1f%% status=%d, ",
                st.percentage, st.status);

            switch (st.status)
            {
            case PM_STATUS_CHARGING:
                printf("   Status: Charging\n");
                break;
            case PM_STATUS_DISCHARGING:
                printf("   Status: Discharging\n");
                break;
            case PM_STATUS_FULL:
                printf("   Status: Full\n");
                break;
            default:
                printf("   Status: Unknown (%d)\n", st.status);
                break;
            }

        } else {
            printf("[state] read failed\n");
        }
        sleep(1);
    }

    /* keep running for callback test demo trigger */
    while (true) {
        sleep(1);
    }

    pm_free(batt);
    return 0;
}
