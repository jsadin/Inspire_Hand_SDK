# 灵巧手控制系统（Inspire / ROS2）

[![CI](https://github.com/jsadin/Inspire_Hand_SDK/actions/workflows/ci.yml/badge.svg)](https://github.com/jsadin/Inspire_Hand_SDK/actions/workflows/ci.yml)

基于 C++ 与 ROS2 的多设备灵巧手控制系统，底层通过 RS485 / CANFD 等与多台 Inspire 系列灵巧手通信。节点包名为 **`inspire_control_ros2`**。

## 项目简介

本项目是一个模块化的灵巧手控制系统，支持：
- ✅ **多设备支持**：同时控制多个灵巧手设备（如左手、右手）
- ✅ **多协议支持**：通过工厂模式支持多种通信协议（**RH524J1_485**、RH56F1_485、**RH56H1_485** / **RH56H1_canfd**、**RH56DFX_serial_can**、RH5DG2_485、**EG5CD1**、**EG2_4C2_serial_can** 等）
- ✅ **动态配置**：通过 YAML 配置设备协议与 ROS2 话题/服务
- ✅ **双通信模式**：支持话题（实时控制）和服务（按需调用）两种方式
- ✅ **异步串口通信**：基于Boost.Asio的异步串口通信，支持超时和错误处理
- ✅ **统一日志系统**：全局日志管理器，支持文件轮转和级别控制
- ✅ **持续集成（CI）**：GitHub Actions 自动编译、跑单元测试与静态检查

### 开发与规范

加新产品、写提交、开 PR 请先看这两份短文（不要只改默认 yaml）：

- **[docs/开发与Git约定.md](docs/开发与Git约定.md)**：分支命名、提交信息、CI、禁止入库、配置文件怎么起名（机型示例一律 `*_example.yaml`，启动时按自己的机型选用）
- **[docs/新增机型清单.md](docs/新增机型清单.md)**：协议 → 接口包 → 适配器 → 示例 yaml → 文档的固定步骤和机型矩阵

架构与线程模型见 [docs/项目架构说明.md](docs/项目架构说明.md)；给 AI 协作者的**新增产品通用规则**见 [docs/项目提示词.md](docs/项目提示词.md)。

## 目录

- [项目结构](#项目结构)
- [机型说明](#机型说明)
- [快速开始](#快速开始)
- [文档说明](#文档说明)
- [架构要点](#架构要点)
- [常见问题](#常见问题)
- [扩展开发](#扩展开发)

## 项目结构

本仓库即一个 **colcon 工作区根目录**，`src/` 下为各平级包（单层 `src`，无嵌套工作区）：

```
serial_control/                        # = git 根 = colcon 工作区根
├── src/                               # colcon 包目录（唯一一层）
│   ├── inspire_serial_core/           # ① 裸库（纯 CMake 包，可脱离 ROS 独立构建）
│   │   ├── package.xml                #    <build_type>cmake</build_type>，供 colcon 排序
│   │   ├── CMakeLists.txt             #    构建 SHARED 库 + 安装/导出 find_package 配置
│   │   ├── cmake/inspire_serial_coreConfig.cmake.in   # 导出配置模板（源文件，需入库）
│   │   ├── include/                   #    protocol.hpp、serial_port.hpp；机型协议在 protocol/hand|gripper/
│   │   ├── src/                       #    串口 / 配置 / 日志；机型协议在 protocol/hand|gripper/
│   │   ├── examples/                  #    main.cpp 多设备并行控制示例（serial_hand_control_node）
│   │   ├── config/                    #    device_protocol_config.yaml、device_protocol_rh56f1_example.yaml、device_protocol_rh5dg2_example.yaml ...
│   │   └── tests/                     #    gtest 单元测试（RingBuffer / DeviceWorker / 三款 485 协议，不依赖硬件）
│   ├── driver/                        # ② 功能包 inspire_control_ros2（find_package(inspire_serial_core)）
│   │   ├── src/                       #    节点、RegisterController、机型适配器
│   │   ├── include/
│   │   ├── config/                    #    device_protocol_*.yaml、ros2_controller_*.yaml
│   │   └── launch/                    #    inspire_control_*.launch.py
│   └── interfaces/                    # ③ 接口包
│       ├── RH524J1/                   #    rh524j1_interfaces（24 自由度腱绳手）
│       ├── RH5DG2/                    #    rh5dg2_interfaces（13 自由度）
│       ├── RH56F1/                    #    rh56f1_interfaces（6 自由度）
│       ├── RH56H1/                    #    rh56h1_interfaces（6 自由度，触觉 version2 压阻式）
│       ├── RH56DFX/                   #    rh56dfx_interfaces（Serial-CAN 灵巧手）
│       ├── EG5CD1/                    #    eg5cd1_interfaces（EG-5CD1 夹爪）
│       └── EG2_4C2/                   #    eg2_4c2_interfaces（EG2-4C2 Serial-CAN 夹爪）
├── docs/                              # 架构 / Git 约定 / 加机型清单 / 协议规则（手册 PDF 不强制入库）
├── scripts/                           # CI 辅助脚本（clang-format / clang-tidy 检查）
├── .github/workflows/                 # GitHub Actions CI 配置
├── install_dependencies.sh           # 依赖安装脚本（一键安装）
├── .gitignore
└── README.md                         # 本文件
```

> **依赖关系**：`driver` 通过 `find_package(inspire_serial_core)` 链接裸库导出的 `inspire_serial_core::inspire_serial_core` 目标；colcon 依 `package.xml` 的 `<depend>inspire_serial_core</depend>` 自动保证先构建裸库。裸库采用 **SHARED** 库，使各协议 `REGISTER_PROTOCOL` 自注册对象随 `.so` 加载执行（避免 STATIC 归档丢符号）。

### ROS2 接口说明（重构后）

| 包名 | 作用 |
|------|------|
| **inspire_control_ros2** | 节点与驱动逻辑：`inspire_control_node`、`RegisterController`、**`RH524J1InterfaceAdapter`** / `RH5DG2InterfaceAdapter` / `RH56F1InterfaceAdapter` / **`RH56H1InterfaceAdapter`** / **`RH56DFXInterfaceAdapter`** / **`EG5CD1InterfaceAdapter`** / **`EG2_4C2InterfaceAdapter`**，配置文件安装在 `share/inspire_control_ros2/config`。 |
| **rh524j1_interfaces** | RH524J1（065 腱绳手，24 自由度）专用 `msg`/`srv`，`joint_values` 长度 24，顺序为电缸 ID1～ID24。话题含角度/力/速度/电流；服务含设置角度力速度、读温度/故障码/状态/电流/力/角度等。 |
| **rh5dg2_interfaces** | RH5DG2（13 自由度）专用 `msg`/`srv`，例如 `SetAngle1`、`GetAngleAct1`、`Setforce`、`Geterror` 等。 |
| **rh56f1_interfaces** | RH56F1（6 自由度）专用 `msg`/`srv`。 |
| **rh56h1_interfaces** | RH56H1（6 自由度）专用 `msg`/`srv`，与 `rh56f1_interfaces` 字段一致（寄存器/帧相同），但作为**独立接口包**与其他机械手架构对齐；触觉使用 version2（压阻式）的 `TouchData2`。 |
| **rh56dfx_interfaces** | RH56DFX Serial-CAN 灵巧手专用 `msg`/`srv`，服务集与 RH5DG2/RH56F1 对齐（`Setangle`/`Setforce`/`Setspeed`/`Setid`/`Setbaudrate`/`Setclearerror`/`Setactionseqindex`/`Geterror`/`Getstatus`/`Gettemp` 等已支持；`Setmode`/`Setpause`/`Setstop`/`Setresetpara`/`Setgestureforceclb`/`Setactionlibraryindex` 在当前 CAN 协议中暂未定义，调用返回 `not_supported`；另含 DFX 特有 `Setsave`）。电流话题 `SetCurrent1`/`GetCurrentAct1` 已映射至 CAN 寄存器 `currentSet`（1020 `CURRENT_LIMIT`）与 `currentAct`（1594 `CURRENT`）；`touchAct` 在当前机型无触觉硬件，话题保留但不发布数据。 |
| **eg5cd1_interfaces** | **因时 EG-5CD1** 电动夹爪 RS485：`GripperState`、`SetInt32`、`TriggerForHand`、`SetInt32Value`、`GetScalarForHand`；**组合服务** `ForceModeGrasp` / `ForceModeOpen` / `TouchModeGrasp` / `TouchModeOpen`（仅 `hand_id`+`speed`+`force`，内部按文档顺序经 `ioWriteSequence` 在设备 `DeviceWorker` 上**原子串行**写寄存器，见下）。 |
| **eg2_4c2_interfaces** | **因时 EG2-4C2** 电动夹爪 Serial-CAN（USB-CAN 转串口模块，详见 `docs/4C2夹爪CAN转Serial通信规则.md`）：`GripperState`、`SetInt32`、`TriggerForHand`、`SetInt32Value`、`GetScalarForHand`；**组合服务** `ForceModeGrasp` / `ForceModeOpen`（4C2 手册只定义 `catchMode` 0/1 两档位置/力控，无触控模式，故不提供 `TouchModeGrasp/Open`）。与 `eg5cd1_interfaces` 服务签名一致，**架构上与其他机械手夹爪对齐**，可通过 `eg2_4c2_composite_service_prefix` 改前缀（默认 `/gripper`）。 |

在 **`device_protocol_config.yaml`** 中设置 **`protocol.type`**（如 **`RH524J1_485`**、**`RH5DG2_485`**、**`RH56F1_485`**、**`RH56H1_485`** / **`RH56H1_canfd`**、**`RH56DFX_serial_can`**、**`EG5CD1`**、**`EG2_4C2_serial_can`** 等），启动时自动推导 **`interfaces_profile`**（**`RH524J1`** / `RH5DG2` / `RH56F1` / **`RH56H1`** / **`RH56DFX`** / **`EG5CD1`** / **`EG2_4C2`**）并创建对应适配器。

**RH56H1** 与 **RH56F1** 帧格式相同，但接口包独立（`rh56h1_interfaces`）；差别主要是触觉 **version2 压阻**（`TouchData2`）。百分比与 raw 话题/服务、换算表见 [docs/RH56H1_ROS2_API.md](docs/RH56H1_ROS2_API.md)。

## 机型说明

完整话题/服务与自检写在各机型 `docs/` 里。这里只给启动入口。**按自己手上的机型**选对应 `*_example.yaml`，同时传 `device_config` 和 `controller_config`。

### RH56H1

- **协议**：`RH56H1_485` / `RH56H1_canfd`；接口包 `rh56h1_interfaces`
- **示例**：`device_protocol_rh56h1_example.yaml`（485）、`device_protocol_rh56h1_canfd_example.yaml`（CAN-FD）、`ros2_controller_rh56h1_example.yaml`

```bash
# CAN-FD（常见）
ros2 launch inspire_control_ros2 inspire_control_single_device.launch.py \
  device_name:=hand_left \
  device_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/device_protocol_rh56h1_canfd_example.yaml \
  controller_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/ros2_controller_rh56h1_example.yaml
```

百分比接口、raw 命令、触觉 `TouchData2` 见 [docs/RH56H1_ROS2_API.md](docs/RH56H1_ROS2_API.md)。

### RH56F1

6 关节，电容触觉 version1。帧与 H1 相同，**没有百分比接口**。

- **协议**：`RH56F1_485` / `RH56F1_canfd`；接口包 `rh56f1_interfaces`
- **示例**：`device_protocol_rh56f1_example.yaml`、`ros2_controller_rh56f1_example.yaml`

```bash
ros2 launch inspire_control_ros2 inspire_control_single_device.launch.py \
  device_name:=hand_left \
  device_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/device_protocol_rh56f1_example.yaml \
  controller_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/ros2_controller_rh56f1_example.yaml
```

话题/服务、触觉 `TouchData1` 与自检见 [docs/RH56F1_ROS2_API.md](docs/RH56F1_ROS2_API.md)；485 帧见 [docs/RH56F1_485协议格式说明.md](docs/RH56F1_485协议格式说明.md)。

### RH56DFX

6 关节 Serial-CAN（USB-CAN 转串口）。角度 0～1000。无触觉硬件。

- **协议**：`RH56DFX_serial_can`；接口包 `rh56dfx_interfaces`
- **示例**：`device_protocol_rh56dfx_example.yaml`、`ros2_controller_rh56dfx_example.yaml`

```bash
ros2 launch inspire_control_ros2 inspire_control_single_device.launch.py \
  device_name:=hand_left \
  device_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/device_protocol_rh56dfx_example.yaml \
  controller_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/ros2_controller_rh56dfx_example.yaml
```

话题/服务与自检见 [docs/RH56DFX_ROS2_API.md](docs/RH56DFX_ROS2_API.md)；通信规则见 [docs/RH56DFX_Serial_CAN协议说明.md](docs/RH56DFX_Serial_CAN协议说明.md)。

### RH5DG2

13 关节灵巧手，RS485（也有 `RH5DG2_canfd`）。

- **协议**：`RH5DG2_485` / `RH5DG2_canfd`；接口包 `rh5dg2_interfaces`
- **示例**：`device_protocol_rh5dg2_example.yaml`、`ros2_controller_rh5dg2_example.yaml`

```bash
ros2 launch inspire_control_ros2 inspire_control_single_device.launch.py \
  device_name:=hand_left \
  device_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/device_protocol_rh5dg2_example.yaml \
  controller_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/ros2_controller_rh5dg2_example.yaml
```

13 路数组、话题/服务与自检见 [docs/RH5DG2_ROS2_API.md](docs/RH5DG2_ROS2_API.md)；485 帧见 [docs/RH5DG2_485协议格式说明.md](docs/RH5DG2_485协议格式说明.md)。

### EG-5CD1

电动夹爪 RS485。**`protocol.type` 必须写 `EG5CD1`**（不是 `EG5CD1_485`）。触觉块 20 字节，接近觉 uint32。

- **示例**：`device_protocol_eg5cd1_example.yaml`、`ros2_controller_eg5cd1_example.yaml`
- 组合服务：`force_mode_grasp` / `open`、`touch_mode_grasp` / `open`（前缀默认 `/gripper`）

```bash
ros2 launch inspire_control_ros2 inspire_control_single_device.launch.py \
  device_name:=hand_left \
  device_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/device_protocol_eg5cd1_example.yaml \
  controller_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/ros2_controller_eg5cd1_example.yaml
```

话题、组合服务与触觉见 [docs/EG5CD1_ROS2_API.md](docs/EG5CD1_ROS2_API.md)；485 帧见 [docs/EG5CD1协议格式说明.md](docs/EG5CD1协议格式说明.md)。

### EG2-4C2

电动夹爪 Serial-CAN（USB-CAN 转串口）。手册只有 `catchMode` 0/1，**不做触控组合**。

- **协议**：`EG2_4C2_serial_can`；接口包 `eg2_4c2_interfaces`
- **示例**：`device_protocol_eg2_4c2_example.yaml`、`ros2_controller_eg2_4c2_example.yaml`

```bash
ros2 launch inspire_control_ros2 inspire_control_single_device.launch.py \
  device_name:=hand_left \
  device_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/device_protocol_eg2_4c2_example.yaml \
  controller_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/ros2_controller_eg2_4c2_example.yaml
```

组合服务、运动自检见 [docs/EG2_4C2_ROS2_API.md](docs/EG2_4C2_ROS2_API.md)；通信规则见 [docs/4C2夹爪CAN转Serial通信规则.md](docs/4C2夹爪CAN转Serial通信规则.md)。

### RH524J1

因时 065 腱绳手，**24** 自由度，RS485，帧同 RH5DG2。无触觉。

- **协议**：`RH524J1_485`；接口包 `rh524j1_interfaces`
- **示例**：`device_protocol_rh524j1_example.yaml`、`ros2_controller_rh524j1_example.yaml`

```bash
ros2 launch inspire_control_ros2 inspire_control_single_device.launch.py \
  device_name:=hand_left \
  device_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/device_protocol_rh524j1_example.yaml \
  controller_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/ros2_controller_rh524j1_example.yaml
```

关节顺序、话题/服务与自检见 [docs/RH524J1_ROS2_API.md](docs/RH524J1_ROS2_API.md)。

## 快速开始

> **💡 快速安装**：推荐使用自动化安装脚本一键安装所有依赖
> ```bash
> ./install_dependencies.sh
> ```
> 详细说明见 [依赖安装](#2-依赖安装) 章节

### 1. 环境要求

- **操作系统**：Linux (Ubuntu 22.04+)
- **ROS2**：Humble或更高版本
- **C++标准**：C++17
- **编译器**：GCC 9+ 或 Clang 10+
- **构建工具**：CMake 3.10+

### 2. 依赖安装

推荐一键脚本（系统库、Boost / yaml-cpp / spdlog、串口 `dialout` 组、检查 ROS2）：

```bash
./install_dependencies.sh
```

装完请**重新登录**（或 `newgrp dialout`），否则可能打不开 `/dev/ttyUSB0`。

版本、逐项 apt 命令和验证步骤见 [docs/依赖清单.md](docs/依赖清单.md)。未装 ROS2 Humble 时按该文档「ROS2 依赖」一节安装，再跑脚本。

### 3. 编译项目

#### 一键编译整个工作区（推荐）

仓库根目录即 colcon 工作区，裸库与 ROS 包一起构建，依赖顺序自动解析：

```bash
cd /home/ubuntu/serial_control
source /opt/ros/humble/setup.bash   # 或本机已安装的 ROS2 distro
colcon build
source install/setup.bash
```

仅改节点代码时可只编译节点包：`colcon build --packages-select inspire_control_ros2`；改了裸库或接口包时需带上对应包（或直接全量 `colcon build`）。

#### 仅编译核心库（无 ROS 环境时）

裸库是纯 CMake 包，可脱离 ROS 单独构建（含 `serial_hand_control_node` 示例）：

```bash
cd src/inspire_serial_core
cmake -S . -B build && cmake --build build -j
```

#### 纯 C++ 控制灵巧手（无 ROS）

同一程序 `serial_hand_control_node` 通过 YAML 切换机型，**运行时**指定角度，无需改源码重编译：

| 参数 | 说明 |
|------|------|
| `--config` / `-c` | 设备协议 YAML（如 `config/device_protocol_rh56dfx_example.yaml`） |
| `--angles v1,v2,...` | 固定角度，逗号分隔（RH56DFX/RH56F1/RH56H1=6 个，RH5DG2=13 个） |
| `--angles-file <path>` | 从文件读取一行角度（逗号或空格分隔） |
| `--demo` | 自动开合演示（默认，未指定 `--angles` 时） |
| `--read-only` | 只读 `angleAct`，不写 `angleSet` |

```bash
cd src/inspire_serial_core

# 自动演示（默认）
./build/serial_hand_control_node --config config/device_protocol_rh56dfx_example.yaml

# 握拳：六个关节固定角度（RH56DFX）
./build/serial_hand_control_node -c config/device_protocol_rh56dfx_example.yaml \
  --angles 1000,1000,1000,1000,1200,1800

# 从文件读角度（方便脚本反复调用）
echo "1800,1800,1800,1800,1350,1800" > /tmp/hand_pose.txt
./build/serial_hand_control_node -c config/device_protocol_rh56dfx_example.yaml \
  --angles-file /tmp/hand_pose.txt

# 只读当前角度
./build/serial_hand_control_node -c config/device_protocol_rh56dfx_example.yaml --read-only
```

快捷脚本（机型别名 + 透传额外参数）：

```bash
./scripts/run_cpp_hand.sh rh56dfx --angles 1000,1000,1000,1000,1200,1800
./scripts/run_cpp_hand.sh rh56dfx --read-only
```

> 需要 ROS 话题/服务控制时，请使用 `inspire_control_ros2` 节点；纯 C++ 版适合无 ROS 环境或快速真机验证。

#### 运行单元测试

核心库自带 gtest 单元测试，覆盖：`RingBuffer` 环形缓冲、`DeviceWorker` 串口事务串行化（FIFO 执行、异常传播、并发提交无重叠、关停语义），以及 RH56F1 / RH5DG2 / RH524J1 / EG5CD1 等 485 协议 + RH56DFX_serial_can / EG2_4C2_serial_can 两个 Serial-CAN 协议的命令构建、响应解析、校验和、A5 转义、ExtId 编码等**纯逻辑**。全部用例不依赖真实串口硬件，测试源码位于 `src/inspire_serial_core/tests/`。

- **colcon 工作区方式**（推荐）：

```bash
cd /home/ubuntu/serial_control
source /opt/ros/humble/setup.bash
colcon build --packages-select inspire_serial_core
colcon test --packages-select inspire_serial_core
colcon test-result --all          # 查看测试汇总
```

- **独立 CMake 方式**（无 ROS 环境）：

```bash
cd /home/ubuntu/serial_control/src/inspire_serial_core
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure
```

> 测试默认随核心库一起构建（CMake 选项 `INSPIRE_SERIAL_CORE_BUILD_TESTS=ON`）；若环境未安装 GTest（`libgtest-dev`），构建会自动跳过测试而不影响主库。关闭测试可加 `-DINSPIRE_SERIAL_CORE_BUILD_TESTS=OFF`。

#### 持续集成（CI）

每次向 `master`/`main` 分支 **push** 或发起 **Pull Request** 时，GitHub Actions 会自动执行（见 [`.github/workflows/ci.yml`](.github/workflows/ci.yml)）：

| 步骤 | 内容 |
|------|------|
| `colcon build` | 编译整个工作区（含 RH56DFX、RH56H1、RH524J1、EG5CD1、EG2-4C2 等接口包） |
| `colcon test` | 运行 `inspire_serial_core` 的 gtest 用例，并用 `grep` 确认至少发现 1 个测试（避免“0 tests”被误判为通过） |
| `clang-format` | 校验 C++ 代码格式（规则见根目录 `.clang-format`） |
| `clang-tidy` | 对核心库与驱动包做静态分析（规则见 `.clang-tidy`） |

本地复现 CI 检查（需先 `colcon build --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`）：

```bash
./scripts/check_clang_format.sh
./scripts/run_clang_tidy.sh
```

状态徽章见 README 顶部；详细运行日志在 GitHub 仓库的 **Actions** 页。

### 4. 配置设备

按机型打开对应的 **`device_protocol_*_example.yaml`**，改 `port`、`Hand_ID`。`protocol.type` 必须和机型一致（下例以 `RH56F1_485` 演示，按需换成 `RH56H1_canfd` / `RH5DG2_485` / `RH56DFX_serial_can` / `RH524J1_485` / **`EG5CD1`** / `EG2_4C2_serial_can` 等）：

```yaml
protocol:
  type: RH56F1_485

devices:
  - name: hand_left
    port: /dev/ttyUSB0
    baudrate: 115200
    Hand_ID: 1
```

### 5. 启动节点

#### 单设备模式

按自己的机型同时指定 `device_config` 和 `controller_config`，完整命令见上文「机型说明」。例如 RH56H1 CAN-FD：

```bash
ros2 launch inspire_control_ros2 inspire_control_single_device.launch.py \
  device_name:=hand_left \
  device_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/device_protocol_rh56h1_canfd_example.yaml \
  controller_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/ros2_controller_rh56h1_example.yaml
```

#### 多设备模式

```bash
ros2 launch inspire_control_ros2 inspire_control_multi_device.launch.py
```

### 6. 使用示例

入站话题/服务里的 **`hand_id`** 必须等于该设备 yaml 的 **`Hand_ID`**，否则写寄存器会被拒绝（`accepted: false`）或订阅被忽略。**`hand_id: 0`** 视为未指定，本节点仍接受。

各机型数组长度、话题/服务和自检写在对应 API 文里，不要抄错接口包：

| 机型 | 关节数 | 接口包 | API |
|------|--------|--------|-----|
| RH56F1 | 6 | `rh56f1_interfaces` | [docs/RH56F1_ROS2_API.md](docs/RH56F1_ROS2_API.md) |
| RH56H1 | 6 | `rh56h1_interfaces` | [docs/RH56H1_ROS2_API.md](docs/RH56H1_ROS2_API.md) |
| RH56DFX | 6 | `rh56dfx_interfaces` | [docs/RH56DFX_ROS2_API.md](docs/RH56DFX_ROS2_API.md) |
| RH5DG2 | 13 | `rh5dg2_interfaces` | [docs/RH5DG2_ROS2_API.md](docs/RH5DG2_ROS2_API.md) |
| RH524J1 | 24 | `rh524j1_interfaces` | [docs/RH524J1_ROS2_API.md](docs/RH524J1_ROS2_API.md) |
| EG-5CD1 | 夹爪 1 | `eg5cd1_interfaces` | [docs/EG5CD1_ROS2_API.md](docs/EG5CD1_ROS2_API.md) |
| EG2-4C2 | 夹爪 1 | `eg2_4c2_interfaces` | [docs/EG2_4C2_ROS2_API.md](docs/EG2_4C2_ROS2_API.md) |

查看字段：`ros2 interface show <包名>/<类型>`。

## 文档说明

### 项目架构说明

📖 **[docs/项目架构说明.md](docs/项目架构说明.md)**

包含：
- 系统整体架构图
- 各模块关系和数据流
- 线程模型
- 启动流程
- 扩展点说明

### 模块使用说明

📖 **[docs/模块使用说明.md](docs/模块使用说明.md)**

### 依赖清单

📖 **[docs/依赖清单.md](docs/依赖清单.md)**

包含：
- 完整的依赖项列表
- 版本要求
- 安装命令
- 验证方法
- 常见问题

### 协议格式说明

📖 **[docs/RH56F1_485协议格式说明.md](docs/RH56F1_485协议格式说明.md)**（另见 `docs/RH5DG2_485协议格式说明.md`、`docs/RH56DFX_Serial_CAN协议解析.md`、`docs/夹爪485寄存器规则.md`、`docs/EG5CD1协议格式说明.md`、`docs/4C2夹爪CAN转Serial通信规则.md`）

包含：
- 读写请求格式
- 读写回复格式
- 各字节含义
- 校验和计算
- 完整示例

### 机型 ROS2 API

- [docs/RH56F1_ROS2_API.md](docs/RH56F1_ROS2_API.md)：6 轴 raw、触觉 version1
- [docs/RH56H1_ROS2_API.md](docs/RH56H1_ROS2_API.md)：百分比 / raw / 触觉 version2
- [docs/RH56DFX_ROS2_API.md](docs/RH56DFX_ROS2_API.md)：Serial-CAN、0～1000 角度、无触觉
- [docs/RH5DG2_ROS2_API.md](docs/RH5DG2_ROS2_API.md)：13 轴、话题/服务、自检
- [docs/RH524J1_ROS2_API.md](docs/RH524J1_ROS2_API.md)：24 轴顺序、话题/服务、自检
- [docs/EG5CD1_ROS2_API.md](docs/EG5CD1_ROS2_API.md)：夹爪话题、组合服务、触觉
- [docs/EG2_4C2_ROS2_API.md](docs/EG2_4C2_ROS2_API.md)：组合服务、运动自检（帧格式见 4C2 通信规则）

## 架构要点

分层、线程、`DeviceWorker` 串口串行化、启动流程见 **[docs/项目架构说明.md](docs/项目架构说明.md)**。模块 API 见 [docs/模块使用说明.md](docs/模块使用说明.md)。

要点：

- 话题约 50Hz 定时读；服务按需 set/get；组合动作走 `ioWriteSequence`（步间隔 3ms）
- 每台设备一个工人、一个串口；不要两台手共用同一个 `port`
- 读写错误用 `IoError`，经 `.srv` 的 `message` 回给调用方
- 配置与启动见上文「机型说明」和第 4、5 节；按机型选用 `*_example.yaml`

## 常见问题

### 1. 依赖装不上

先跑 `./install_dependencies.sh`，再按 [docs/依赖清单.md](docs/依赖清单.md) 核对 CMake / Boost / yaml-cpp / spdlog / ROS2 Humble。增量编译后若测试报找不到新协议符号，删掉 `build/inspire_serial_core` 再编一次。

### 2. 串口权限问题

```bash
# 添加用户到dialout组
sudo usermod -a -G dialout $USER

# 重新登录后生效，或立即生效
newgrp dialout

# 验证权限
groups | grep dialout

# 或临时设置权限
sudo chmod 666 /dev/ttyUSB0
```

### 3. 设备未找到

- 检查串口设备：`ls -l /dev/ttyUSB*`
- 检查配置文件中的端口路径
- 检查设备是否已连接
- 检查USB转串口驱动：`lsmod | grep usbserial`

### 4. 通信超时

- 检查波特率配置
- 检查设备ID（Hand_ID）配置
- 检查串口连接
- 查看日志文件排查问题
- 检查串口是否被其他程序占用：`lsof /dev/ttyUSB0`

### 5. ROS2节点未启动

- 检查配置文件路径
- 检查ROS2环境：`source install/setup.bash`
- 检查ROS2包是否编译：`colcon list`
- 查看日志：`ros2 run inspire_control_ros2 inspire_control_node --ros-args --log-level debug`
- 检查节点是否已运行：`ros2 node list`

### 6. 编译错误

#### 找不到ROS2包

```bash
# 确保已source ROS2环境
source /opt/ros/humble/setup.bash

# 检查ROS2包
ros2 pkg list | grep rclcpp
```

#### 链接错误

```bash
# 检查库文件是否存在
ldconfig -p | grep boost
ldconfig -p | grep yaml
ldconfig -p | grep spdlog

# 更新动态链接库缓存
sudo ldconfig
```

#### CMake找不到包

```bash
# 检查pkg-config路径
echo $PKG_CONFIG_PATH

# 如果为空，添加默认路径
export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/local/lib/pkgconfig
```

## 扩展开发

完整步骤、必改文件和自检见 **[docs/新增机型清单.md](docs/新增机型清单.md)**。Git 分支与提交见 **[docs/开发与Git约定.md](docs/开发与Git约定.md)**。给 AI 的通用规则见 **[docs/项目提示词.md](docs/项目提示词.md)**。

摘要：新机型只加 `*_example.yaml`；启动用现有 `inspire_control_single_device.launch.py` 按机型指向示例文件。新协议放在 `include/src/protocol/hand` 或 `gripper`，必须 `REGISTER_PROTOCOL`、工厂显式分支（不要落到默认 RH5DG2）、并补 gtest。README 机型节只留启动短入口，话题表和自检写 `docs/<机型>_ROS2_API.md`。

driver 侧 RH5DG2 / RH56F1 遗留文件名已统一为 `*_example.yaml`（`git mv` 保留历史）。裸库 `src/inspire_serial_core/config/` 里给非 ROS 示例用的短名 `RH5DG2.yaml` / `RH56F1.yaml` 未改，避免搅动 `examples/main.cpp`。后续若再加机型，只新增一对 example，不要再引入 `device_protocol_config_<model>.yaml` 这种旧别名。

---

**文档版本**：v1.6  
**最后更新**：2026-09-11（补齐 F1 / DG2 / DFX 的 ROS2 API；启动按机型自选 `*_example.yaml`）
