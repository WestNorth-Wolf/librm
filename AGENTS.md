# librm — AI Agent Guide

跨平台 Robomaster 嵌入式软件开发框架，为 STM32、Linux/Raspberry Pi/Jetson 提供统一的外设 HAL、设备驱动与算法库。

详细说明见 [README.md](README.md) 与 [文档站](https://librm.xduirobot.cc/)，文档站源码位于 [docs/](docs/) 目录。对代码进行修改后应该同步检查文档站相关内容是否需要更新。

## 注意事项

1. 若无特殊说明，实现任何功能时应该避免动态分配内存（new/malloc），以保证在 STM32 等资源受限平台上的行为确定性。如果需要使用STL容器或算法，考虑 ETL（Embedded Template Library）提供的无动态内存版本。

2. 代码风格在遵循 Google C++ Style Guide 的前提下尽量和现有代码保持一致，使用 clang-format 进行格式化。规则见 [.clang-format](.clang-format)。

3. librm 整合有 ETL（Embedded Template Library）作为轻量级的 STL 替代方案，提供了常用的数据结构（vector/map/string 等）和算法支持。请注意实现任何功能时都应该先检查 ETL 是否已有对应的实现，以避免重复造轮子。

## 快速定位

| 需要了解的内容                          | 文件                                                                                 |
| --------------------------------------- | ------------------------------------------------------------------------------------ |
| 所有公开 API 的汇总入口                 | [src/librm.hpp](src/librm.hpp)                                                       |
| 类型别名（u8/u16/f32…）                 | [src/librm/core/typedefs.hpp](src/librm/core/typedefs.hpp)                           |
| 平台检测（STM32 / Linux）               | [cmake/detect_platform.cmake](cmake/detect_platform.cmake)                           |
| HAL 抽象接口（CAN/UART/SPI/GPIO/Timer） | `src/librm/hal/*_interface.hpp`                                                      |
| STM32 HAL 实现                          | `src/librm/hal/stm32/`                                                               |
| Linux/Raspi HAL 实现                    | `src/librm/hal/linux/`, `src/librm/hal/raspi/`                                       |
| 设备基类                                | [src/librm/device/device.hpp](src/librm/device/device.hpp)                           |
| CAN 设备基类                            | [src/librm/device/can_device.hpp](src/librm/device/can_device.hpp)                   |
| DJI 电机（典型设备实现）                | `src/librm/device/actuator/dji_motor.hpp`                                            |
| PID 控制器                              | [src/librm/modules/pid.hpp](src/librm/modules/pid.hpp)                               |
| 轨迹限制器                              | [src/librm/modules/trajectory_limiter.hpp](src/librm/modules/trajectory_limiter.hpp) |
| 底盘运动学（FKIK）                      | [src/librm/modules/chassis_fkik.hpp](src/librm/modules/chassis_fkik.hpp)             |
| 异常/断言策略                           | [src/librm/core/exception.hpp](src/librm/core/exception.hpp)                         |
| 测试说明                                | [test/README.md](test/README.md)                                                     |

## 架构与命名空间

```
rm::core::    — 类型别名、Trait、异常、时间（平台无关）
rm::hal::     — 外设抽象接口 + 各平台实现
rm::device::  — 设备驱动（电机、传感器、遥控、裁判系统）
rm::modules:: — 算法模块（PID、AHRS、运动学、滤波器…）
```

平台在 CMake 配置期自动检测，通过 `LIBRM_PLATFORM_STM32` / `LIBRM_PLATFORM_LINUX_*` 宏区分。
