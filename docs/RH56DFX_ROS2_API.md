# RH56DFX ROS2 API 说明

本文描述 `inspire_control_ros2` 在 **RH56DFX**（`protocol.type: RH56DFX_serial_can`）下的 6 关节话题/服务与自检。USB-CAN 转串口的帧格式、ExtId、寄存器见 [RH56DFX_Serial_CAN协议说明.md](RH56DFX_Serial_CAN协议说明.md) 与 [RH56DFX_Serial_CAN协议解析.md](RH56DFX_Serial_CAN协议解析.md)。

启动与短节入口见仓库 [README.md](../README.md)。请用本机型 `*_example.yaml`。

---

**RH56DFX** 是 6 关节灵巧手，主机侧是 **USB-CAN 转串口**（Serial-CAN），不是 RS485。对上层仍用 `angleSet` / `angleAct` 等驼峰寄存器名。

- **协议**：`RH56DFX_serial_can_Protocol`（`REGISTER_PROTOCOL("RH56DFX_serial_can", …)`）
  - 串口封包 21 字节：`AA AA | ExtId(4B 小端) | Data[8] | Meta[4] | Checksum | 55 55`
  - 6 路寄存器按指拆成多帧发送（每指独立地址）
- **接口包**：`rh56dfx_interfaces`（`joint_values` 固定 6 个整数）
- **示例配置**（随包安装到 `share/inspire_control_ros2/config`）：
  - `device_protocol_rh56dfx_example.yaml`：`protocol.type: RH56DFX_serial_can`，串口默认 `/dev/ttyUSB0` @ 115200
  - `ros2_controller_rh56dfx_example.yaml`：话题 `/hand_left/angle_set` 等；服务名与 RH5DG2 对齐
- **无触觉硬件**：`touchAct` 读返回 `NotSupported`，话题 `/hand_left/touch_data` **保留但不发布数据**。
- **无百分比接口**。

**启动**（改代码/配置后必须 **重新编译并重启节点**）：

```bash
colcon build --packages-select rh56dfx_interfaces inspire_serial_core inspire_control_ros2
source install/setup.bash

ros2 launch inspire_control_ros2 inspire_control_single_device.launch.py \
  device_name:=hand_left \
  device_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/device_protocol_rh56dfx_example.yaml \
  controller_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/ros2_controller_rh56dfx_example.yaml
```

`hand_id` 必须和配置文件里的 `Hand_ID` 一致，否则话题会被忽略、服务会返回 `accepted: false`。`hand_id: 0` 视为未指定，本节点仍接受。下面命令都假定 `Hand_ID: 1`。

### 关节顺序（`joint_values[6]`）

| 下标 | 关节 | `ANGLE_SET` 范围 |
|------|------|------------------|
| 0 | 小拇指 | `-1`，0～1000 |
| 1 | 无名指 | `-1`，0～1000 |
| 2 | 中指 | `-1`，0～1000 |
| 3 | 食指 | `-1`，0～1000 |
| 4 | 大拇指弯曲 | `-1`，0～1000 |
| 5 | 大拇指旋转 | 0～1000（手册未给 `-1`） |

- **`0~1000`**：立刻运动到对应角度（手册物理角：四指约 19°～176.7°，拇指弯曲 -13°～53.6°，拇指旋转 90°～165°）。
- **`-1`**：该指保持当前角度（前 5 指）。
- **不要写 1700**：那是 RH56F1/H1 的角度刻度。DFX 的 `angleSet` 上限是 **1000**。
- **不要用电缸 `posSet`**（0=张开、2000=握紧）来设角度。

### 话题一览

节点按 `update_rate`（示例 50 Hz）循环读状态并发布。速度只有写入话题，没有实际速度反馈话题。

| 用途 | 写入话题 | 消息类型 | 读取话题 | 消息类型 | 寄存器 |
|------|----------|----------|----------|----------|--------|
| 角度 | `/hand_left/angle_set` | `SetAngle1` | `/hand_left/angle_actual` | `GetAngleAct1` | `angleSet` / `angleAct` |
| 力 | `/hand_left/force_set` | `SetForce1` | `/hand_left/force_actual` | `GetForceAct1` | `forceSet` / `forceAct`（0～1000 g） |
| 速度 | `/hand_left/speed_set` | `SetSpeed1` | （无） | — | `speedSet`（0～1000，1000≈空载全行程 800ms） |
| 电流 | `/hand_left/current_set` | `SetCurrent1` | `/hand_left/current_actual` | `GetCurrentAct1` | `currentSet`=1020 `CURRENT_LIMIT`（0～1500 mA）/ `currentAct`=1594 |
| 触觉 | （无） | — | `/hand_left/touch_data` | `TouchData1` | 话题保留，**不发布** |

### 角度话题与服务

先设速度再设角度。中间位可用 `[500,500,500,500,500,500]`。

```bash
# --- 话题：写角度 / 读实际角度 ---
ros2 topic pub --once /hand_left/angle_set rh56dfx_interfaces/msg/SetAngle1 \
  "{hand_id: 1, joint_values: [500,500,500,500,500,500]}"

ros2 topic echo /hand_left/angle_actual

# 只动食指，其余保持
ros2 topic pub --once /hand_left/angle_set rh56dfx_interfaces/msg/SetAngle1 \
  "{hand_id: 1, joint_values: [-1,-1,-1,600,-1,500]}"

# --- 服务：写角度 ---
ros2 service call /hand_left/set_angle rh56dfx_interfaces/srv/Setangle \
  "{command: '', hand_id: 1, joint_values: [500,500,500,500,500,500]}"
```

成功时服务返回 `accepted: true`、`message: ok`。

### 力 / 速度 / 电流

```bash
ros2 topic pub --once /hand_left/force_set rh56dfx_interfaces/msg/SetForce1 \
  "{hand_id: 1, joint_values: [400,400,400,400,400,400]}"

ros2 topic echo /hand_left/force_actual

ros2 service call /hand_left/set_force rh56dfx_interfaces/srv/Setforce \
  "{command: '', hand_id: 1, joint_values: [400,400,400,400,400,400]}"

ros2 topic pub --once /hand_left/speed_set rh56dfx_interfaces/msg/SetSpeed1 \
  "{hand_id: 1, joint_values: [600,600,600,600,600,600]}"

ros2 service call /hand_left/set_speed rh56dfx_interfaces/srv/Setspeed \
  "{command: '', hand_id: 1, joint_values: [600,600,600,600,600,600]}"

ros2 topic pub --once /hand_left/current_set rh56dfx_interfaces/msg/SetCurrent1 \
  "{hand_id: 1, joint_values: [800,800,800,800,800,800]}"

ros2 topic echo /hand_left/current_actual
```

写电流保护**不会让手运动**，只改保护值。超限时该指停止，状态码 5。

### 默认速度 / 默认力（映射说明）

RH56DFX **没有**独立的上电默认速度/力寄存器。示例 yaml 仍提供与 RH5DG2 同名的服务，协议层把它们映射到 **`speedSet` / `forceSet`**（当场生效，不是 Flash 默认值）：

```bash
ros2 service call /hand_left/set_defaultSpeed rh56dfx_interfaces/srv/Setdefaultspeed \
  "{hand_id: 1, joint_values: [600,600,600,600,600,600]}"

ros2 service call /hand_left/set_defaultForceSet rh56dfx_interfaces/srv/Setdefaultforceset \
  "{hand_id: 1, joint_values: [400,400,400,400,400,400]}"
```

要写进 Flash，请再调 `set_save`。

### 系统管理与只读服务

| 服务 | 含义 | 命令示例 |
|------|------|---------|
| `set_angle` / `set_force` / `set_speed` | 写 6 路角度 / 力 / 速度 | 见上节 |
| `set_id` | 改设备通信 ID（改完后 yaml 的 `Hand_ID` 也要改） | `ros2 service call /hand_left/set_id rh56dfx_interfaces/srv/Setid "{hand_id: 1, device_id: 1}"` |
| `set_baudRate` | 改 **CAN/CAN-FD 波特率索引**（地址 4008 `REDU_RATIO`，不是 485 波特率） | `ros2 service call /hand_left/set_baudRate rh56dfx_interfaces/srv/Setbaudrate "{hand_id: 1, baudrate: 0}"` |
| `set_clearError` | 写 1 清故障（过温不能清，等温度回落） | `ros2 service call /hand_left/set_clearError rh56dfx_interfaces/srv/Setclearerror "{hand_id: 1, clear_code: 1}"` |
| `set_save` | 写 1 保存到 Flash（本机型特有） | `ros2 service call /hand_left/set_save rh56dfx_interfaces/srv/Setsave "{hand_id: 1, save_code: 1}"` |
| `set_resetPara` | 写 1 恢复出厂 | `ros2 service call /hand_left/set_resetPara rh56dfx_interfaces/srv/Setresetpara "{hand_id: 1, confirm: 1}"` |
| `set_gestureForceClb` | 力校准，约 6 秒，**须空载** | `ros2 service call /hand_left/set_gestureForceClb rh56dfx_interfaces/srv/Setgestureforceclb "{hand_id: 1, calibration_values: [1]}"` |
| `set_actionSeqIndex` | 动作序列号 | `ros2 service call /hand_left/set_actionSeqIndex rh56dfx_interfaces/srv/Setactionseqindex "{hand_id: 1, index: 1}"` |
| `set_actionLibraryIndex` | 动作库号 | `ros2 service call /hand_left/set_actionLibraryIndex rh56dfx_interfaces/srv/Setactionlibraryindex "{hand_id: 1, index: 1}"` |
| `get_errorCode` | 6 路故障码（只读） | `ros2 service call /hand_left/get_errorCode rh56dfx_interfaces/srv/Geterror "{query: '', hand_id: 1}"` |
| `get_status` | 6 路状态（只读） | `ros2 service call /hand_left/get_status rh56dfx_interfaces/srv/Getstatus "{query: '', hand_id: 1}"` |
| `get_temp` | 6 路温度（只读） | `ros2 service call /hand_left/get_temp rh56dfx_interfaces/srv/Gettemp "{query: '', hand_id: 1}"` |

当前 CAN 协议**未定义** `mode` / `pause` / `stop`。示例 yaml 仍挂了这三个服务，调用会返回 **`not_supported`**（ROS 服务在，设备侧没有对应寄存器）。

> **协议有、ROS 未当常规接口暴露**：电缸 `posSet`/`posAct`（0～2000，不建议用来设角度）。无百分比、无触觉数据。

### RH56DFX 话题/服务自检

先停掉旧节点，再用上面的 example yaml 启动。另开终端：

```bash
source install/setup.bash

# 应只有 1 个 /hand_left 节点
ros2 node list
ros2 service type /hand_left/get_status

# 只读
ros2 service call /hand_left/get_status rh56dfx_interfaces/srv/Getstatus \
  "{query: '', hand_id: 1}"
ros2 service call /hand_left/get_errorCode rh56dfx_interfaces/srv/Geterror \
  "{query: '', hand_id: 1}"
ros2 service call /hand_left/get_temp rh56dfx_interfaces/srv/Gettemp \
  "{query: '', hand_id: 1}"

# 话题：先看实际值，再发一条速度 + 角度（6 个数，0～1000）
ros2 topic echo /hand_left/angle_actual

ros2 topic pub --once /hand_left/speed_set rh56dfx_interfaces/msg/SetSpeed1 \
  "{hand_id: 1, joint_values: [600,600,600,600,600,600]}"

ros2 topic pub --once /hand_left/angle_set rh56dfx_interfaces/msg/SetAngle1 \
  "{hand_id: 1, joint_values: [500,500,500,500,500,500]}"
```

判定：

- **服务正常**：有 `response:`，且 `message: ok`。
- **通信异常**：有 `response:` 但 `message: device_error`（ROS 通了，转接器/设备没应答好）。
- **服务未就绪**：一直 `waiting for service` → 节点未启动，或未用 DFX 示例 yaml。
- **`not_supported`**：调了 `set_mode` / `set_pause` / `set_stop`，属预期。
- **话题正常**：发 `angle_set` 后，`angle_actual` 在后续周期出现可观测变化。
- **话题没反应**：检查 `hand_id` 是否等于 yaml 的 `Hand_ID`；角度不要写成 F1 的 1700。
- **触觉无数据**：正常。本机型没有触觉硬件。
