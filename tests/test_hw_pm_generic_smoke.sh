#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
module_root="$(cd "$script_dir/.." && pwd)"
artifact_dir="${SROBOTIS_TEST_ARTIFACT_DIR:-${SROBOTIS_OUTPUT_ROOT:-$PWD/output}/test-artifacts/components/peripherals/pm/${SROBOTIS_TEST_NAME:-pm-generic-hardware-smoke}}"
log_dir="$artifact_dir/logs"
build_dir="$artifact_dir/build"
log_file="$log_dir/pm_generic_hardware_smoke.log"

charger_node="${PM_CHARGER_NODE:-ip2317-charger}"
capacity_node="${PM_CAPACITY_NODE:-cw-bat}"
timeout_s="${PM_HW_SMOKE_TIMEOUT_S:-15}"

mkdir -p "$log_dir" "$build_dir"

{
    echo "[info] module_root=$module_root"
    echo "[info] build_dir=$build_dir"
    echo "[info] charger_node=$charger_node"
    echo "[info] capacity_node=$capacity_node"
    echo "[info] timeout_s=$timeout_s"

    cmake -S "$module_root" -B "$build_dir" \
        -DBUILD_TESTS=ON \
        -DSROBOTIS_PERIPHERALS_PM_ENABLED_DRIVERS=drv_generic
    cmake --build "$build_dir" --target test_pm_generic -j"$(nproc)"
    echo "[info] waiting for a PM state sample"
    set +e
    LD_LIBRARY_PATH="$build_dir:${LD_LIBRARY_PATH:-}" \
        stdbuf -oL -eL "$build_dir/test_pm_generic" "$charger_node" "$capacity_node" &
    test_pid=$!
    seen=0
    for _ in $(seq 1 "$timeout_s"); do
        if grep -q "\[state\] SOC=" "$log_file"; then
            seen=1
            break
        fi
        if ! kill -0 "$test_pid" 2>/dev/null; then
            break
        fi
        sleep 1
    done
    if kill -0 "$test_pid" 2>/dev/null; then
        kill "$test_pid" 2>/dev/null
        wait "$test_pid" 2>/dev/null
    else
        wait "$test_pid" 2>/dev/null
    fi
    set -e
    if [ "$seen" -ne 1 ]; then
        echo "[error] no PM state sample observed within ${timeout_s}s"
        exit 1
    fi
} 2>&1 | tee "$log_file"

grep -q "\\[state\\] SOC=" "$log_file"
