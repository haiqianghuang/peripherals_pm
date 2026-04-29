## 项目简介
本组件提供电源/电池状态读取与监控能力，适用于从 Linux `power_supply` 节点获取充电状态与容量信息，并在充电状态变化时触发回调，便于上层应用感知电源事件。

## 功能特性
- 支持读取电池容量（SOC）与充电状态（Charging/Discharging）。
- 支持 `pm_set_callback` 注册回调，在充电状态变化时触发。
- 支持通过 `pm_alloc_generic` 绑定 `power_supply` 节点路径。
- 不支持电源开关控制（`pm_switch_set/get` 在 GENERIC 驱动中返回不支持）。

## 快速开始
### 环境准备
- Linux 系统，存在 `power_supply` 节点（例如 `/sys/class/power_supply/<charger>/online` 与 `/sys/class/power_supply/<battery>/capacity`）。
- CMake 3.10+，C 编译器（GCC/Clang），pthread（系统默认提供）。

### 构建编译
仅描述脱离 SDK 的独立编译方式：
```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build
```

### 运行示例
默认节点名来自 `test/test_pm_generic.c`：
```bash
./build/test_pm_generic
```
自定义节点名：
```bash
./build/test_pm_generic ip2317-charger cw-bat
```
程序会周期性打印 SOC 与充电状态，并在状态变化时触发回调输出。

## 详细使用
保留，引用到后续的官方文档。

## 常见问题
- 读取失败：确认 `power_supply` 节点名称正确，且进程有读取 `/sys/class/power_supply/*` 的权限。
- 回调未触发：回调仅在充电状态变化时触发，且需先调用 `pm_set_callback` 注册。
- 构建失败：确认已安装 CMake 与系统线程库（pthread）。

## 版本与发布
版本以本目录 `package.xml` 中的 `<version>` 为准。

| 版本   | 日期       | 说明 |
| ------ | ---------- | ---- |
| 0.1.0  | 2026-02-28 | 初始版本，支持通用电池电量、充电状态读取 |

## 贡献方式
欢迎参与贡献：提交 Issue 反馈问题，或通过 Pull Request 提交代码。

- **编码规范**：本组件 C 代码遵循 [Google C++ 风格指南](https://google.github.io/styleguide/cppguide.html)（C 相关部分），请按该规范编写与修改代码。
- **提交前检查**：请在提交前运行本仓库的 lint 脚本，确保通过风格检查：
  ```bash
  # 在仓库根目录执行（检查全仓库）
  bash scripts/lint/lint_cpp.sh

  # 仅检查本组件
  bash scripts/lint/lint_cpp.sh components/peripherals/pm
  ```
  脚本路径：`scripts/lint/lint_cpp.sh`。若未安装 `cpplint`，可先执行：`pip install cpplint` 或 `pipx install cpplint`。
- **提交说明**：提交 Issue 或 PR 前请描述硬件型号、GPIO 编号与复现步骤。

## License
本组件源码文件头声明为 Apache-2.0，最终以本目录 `LICENSE` 文件为准。
