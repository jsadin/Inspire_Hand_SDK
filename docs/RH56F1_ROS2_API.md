# RH56F1 ROS2 API 说明

本文描述 `inspire_control_ros2` 在 **RH56F1**（`RH56F1_485` / `RH56F1_canfd`）下的 6 关节 raw 话题/服务与触觉 version1。帧格式见 [RH56F1_485协议格式说明.md](RH56F1_485协议格式说明.md)。

启动与短节入口见仓库 [README.md](../README.md)。请用本机型 `*_example.yaml`。需要百分比接口或压阻触觉时，请改用 [RH56H1_ROS2_API.md](RH56H1_ROS2_API.md)。

---

**RH56F1** 与 **RH56H1** 寄存器与帧格式相同，支持 **485** 与 **CAN-FD** 两种 `protocol.type`；ROS 接口使用独立包 **`rh56f1_interfaces`**（`joint_values` 固定 6 个整数）。与 H1 的差别：

- 触觉是 **version1（电容式）**，发布 `TouchData1`（H1 是 version2 / `TouchData2`）。
- **没有百分比接口**（没有 `SetPercent1` / `pos_percent_*`）。位置请直接写 `angleSet`。
- 适配器按寄存器原值收发，**不按 H1 手册自动裁剪**。量程以 F1 手册为准。

- **协议**：`RH56F1_485_Protocol` / `RH56F1_canfd_Protocol`
- **接口包**：`rh56f1_interfaces`
- **示例配置**（随包安装到 `share/inspire_control_ros2/config`）：
  - `device_protocol_rh56f1_example.yaml`：`protocol.type: RH56F1_485`
  - `ros2_controller_rh56f1_example.yaml`：话题 `/hand_left/angle_set` 等
- **CAN-FD**：协议已注册为 `RH56F1_canfd`。仓库没有单独的 canfd 示例 yaml，把设备配置里的 `protocol.type` 改成 `RH56F1_canfd` 即可，控制器 yaml 不用换。

**启动**（改代码/配置后必须 **重新编译并重启节点**）：

```bash
colcon build --packages-select rh56f1_interfaces inspire_serial_core inspire_control_ros2
source install/setup.bash

ros2 launch inspire_control_ros2 inspire_control_single_device.launch.py \
  device_name:=hand_left \
  device_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/device_protocol_rh56f1_example.yaml \
  controller_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/ros2_controller_rh56f1_example.yaml
```

`hand_id` 必须和配置文件里的 `Hand_ID` 一致，否则话题会被忽略、服务会返回 `accepted: false`。`hand_id: 0` 视为未指定，本节点仍接受。下面命令都假定 `Hand_ID: 1`。

### 关节顺序（`joint_values[6]`）

| 下标 | 关节 | 简称 |
|------|------|------|
| 0 | 小拇指 | pinky / little |
| 1 | 无名指 | ring |
| 2 | 中指 | middle |
| 3 | 食指 | index |
| 4 | 大拇指弯曲 | thumb_bend |
| 5 | 大拇指旋转 | thumb_rot |

仓库示例常用张开姿态（来自 `examples/RH56F1.cpp`）：`[1700, 1700, 1700, 1700, 1350, 1700]`。`angleSet` 支持 **`-1` = 该指保持当前角度**。不要把 `posSet`（电缸位置，常见 0~2000）当成角度来写。

### 话题一览

节点按 `update_rate`（示例 50 Hz）循环读状态并发布。速度只有写入话题，没有实际速度反馈话题。

| 用途 | 写入话题 | 消息类型 | 读取话题 | 消息类型 | 寄存器 |
|------|----------|----------|----------|----------|--------|
| 角度 | `/hand_left/angle_set` | `SetAngle1` | `/hand_left/angle_actual` | `GetAngleAct1` | `angleSet` / `angleAct` |
| 力 | `/hand_left/force_set` | `SetForce1` | `/hand_left/force_actual` | `GetForceAct1` | `forceSet` / `forceAct` |
| 速度 | `/hand_left/speed_set` | `SetSpeed1` | （无） | — | `speedSet` |
| 电流 | `/hand_left/current_set` | `SetCurrent1` | `/hand_left/current_actual` | `GetCurrentAct1` | `currentSet` / `currentAct` |
| 触觉 | （无） | — | `/hand_left/touch_data` | `TouchData1` | `touchAct`（`touch_version: 1`） |

### 角度话题与服务

日常位置控制建议先设速度，再写 `angleSet`。示例 yaml 左手已挂 `set_angle`。

```bash
# --- 话题：写角度 / 读实际角度 ---
ros2 topic pub --once /hand_left/angle_set rh56f1_interfaces/msg/SetAngle1 \
  "{hand_id: 1, joint_values: [1700,1700,1700,1700,1350,1700]}"

ros2 topic echo /hand_left/angle_actual

# 只动食指，其余保持
ros2 topic pub --once /hand_left/angle_set rh56f1_interfaces/msg/SetAngle1 \
  "{hand_id: 1, joint_values: [-1,-1,-1,1400,-1,-1]}"

# --- 服务：写角度 ---
ros2 service call /hand_left/set_angle rh56f1_interfaces/srv/Setangle \
  "{command: '', hand_id: 1, joint_values: [1700,1700,1700,1700,1350,1700]}"
```

成功时服务返回 `accepted: true`、`message: ok`。

### 力 / 速度 / 电流话题

示例 yaml 里力、速度、电流以**话题**为主（适配器也支持 `Setforce` / `Setspeed` 服务，但示例未挂，需要可自行加到 `ros2_controller_rh56f1_example.yaml`）。

```bash
ros2 topic pub --once /hand_left/force_set rh56f1_interfaces/msg/SetForce1 \
  "{hand_id: 1, joint_values: [600,600,600,600,600,600]}"

ros2 topic echo /hand_left/force_actual

ros2 topic pub --once /hand_left/speed_set rh56f1_interfaces/msg/SetSpeed1 \
  "{hand_id: 1, joint_values: [1000,1000,1000,1000,1000,1000]}"

ros2 topic pub --once /hand_left/current_set rh56f1_interfaces/msg/SetCurrent1 \
  "{hand_id: 1, joint_values: [800,800,800,800,800,800]}"

ros2 topic echo /hand_left/current_actual
```

写电流保护**不会让手运动**，只改保护值。

### 触觉（version1，电容式）

`touch_control` 的 `touch_version` 必须是 **1**。消息 `TouchData1`：

- `finger_forces` / `finger_tangentials` / `finger_angles` / `finger_proximity`：5 个数，顺序 `little, ring, middle, index, thumb`
- `palm_data`：9 个数（掌左 1–3、掌中 4–6、掌右 7–9）

```bash
ros2 topic echo /hand_left/touch_data
```

version2（压阻）在 F1 上**未适配**，不要把 `touch_version` 改成 2。

### 系统管理与只读服务

下面是 `ros2_controller_rh56f1_example.yaml` 左手已挂的服务。

| 服务 | 含义 | 命令示例 |
|------|------|---------|
| `set_angle` | 写 6 路角度 | `ros2 service call /hand_left/set_angle rh56f1_interfaces/srv/Setangle "{command: '', hand_id: 1, joint_values: [1700,1700,1700,1700,1350,1700]}"` |
| `set_id` | 改设备通信 ID（改完后 yaml 的 `Hand_ID` 也要改） | `ros2 service call /hand_left/set_id rh56f1_interfaces/srv/Setid "{hand_id: 1, device_id: 1}"` |
| `set_baudRate` | 改波特率索引（改完后 yaml 的 `baudrate` 也要改） | `ros2 service call /hand_left/set_baudRate rh56f1_interfaces/srv/Setbaudrate "{hand_id: 1, baudrate: 0}"` |
| `set_clearError` | 写 1 清故障 | `ros2 service call /hand_left/set_clearError rh56f1_interfaces/srv/Setclearerror "{hand_id: 1, clear_code: 1}"` |
| `set_resetPara` | 写 1 恢复出厂 | `ros2 service call /hand_left/set_resetPara rh56f1_interfaces/srv/Setresetpara "{hand_id: 1, confirm: 1}"` |
| `set_gestureForceClb` | 力传感器校准（须空载） | `ros2 service call /hand_left/set_gestureForceClb rh56f1_interfaces/srv/Setgestureforceclb "{hand_id: 1, calibration_values: [1]}"` |
| `set_defaultSpeed` | 上电默认速度（6 路） | `ros2 service call /hand_left/set_defaultSpeed rh56f1_interfaces/srv/Setdefaultspeed "{hand_id: 1, joint_values: [1000,1000,1000,1000,1000,1000]}"` |
| `set_defaultForceSet` | 上电默认力（6 路） | `ros2 service call /hand_left/set_defaultForceSet rh56f1_interfaces/srv/Setdefaultforceset "{hand_id: 1, joint_values: [600,600,600,600,600,600]}"` |
| `set_mode` | 6 路运行模式 | `ros2 service call /hand_left/set_mode rh56f1_interfaces/srv/Setmode "{command: '', hand_id: 1, joint_values: [0,0,0,0,0,0]}"` |
| `set_pause` | 写 1 暂停，写 0 继续 | `ros2 service call /hand_left/set_pause rh56f1_interfaces/srv/Setpause "{hand_id: 1, pause_flag: 1}"` |
| `set_stop` | 写 1 急停 | `ros2 service call /hand_left/set_stop rh56f1_interfaces/srv/Setstop "{hand_id: 1, stop_flag: 1}"` |
| `set_actionSeqIndex` | 动作序列号 | `ros2 service call /hand_left/set_actionSeqIndex rh56f1_interfaces/srv/Setactionseqindex "{hand_id: 1, index: 1}"` |
| `get_errorCode` | 6 路故障码（只读） | `ros2 service call /hand_left/get_errorCode rh56f1_interfaces/srv/Geterror "{query: '', hand_id: 1}"` |
| `get_status` | 6 路状态（只读） | `ros2 service call /hand_left/get_status rh56f1_interfaces/srv/Getstatus "{query: '', hand_id: 1}"` |
| `get_temp` | 6 路温度（只读） | `ros2 service call /hand_left/get_temp rh56f1_interfaces/srv/Gettemp "{query: '', hand_id: 1}"` |

> **协议有、示例 yaml 未挂**：`save`、力/速度设置服务、`posSet`/`posAct`（不建议用来设角度）、`speedAct`、动作库。需要时在控制器 yaml 加一行即可（适配器已支持 `Setforce` / `Setspeed` / `Setsave`）。无百分比接口。

### RH56F1 话题/服务自检

改配置或重新编译后，**先重启节点**，再另开终端：

```bash
source install/setup.bash

ros2 node list
ros2 topic list
ros2 service list | grep hand_left

ros2 service call /hand_left/get_temp rh56f1_interfaces/srv/Gettemp "{query: '', hand_id: 1}"
ros2 service call /hand_left/get_errorCode rh56f1_interfaces/srv/Geterror "{query: '', hand_id: 1}"
ros2 service call /hand_left/get_status rh56f1_interfaces/srv/Getstatus "{query: '', hand_id: 1}"

ros2 topic echo /hand_left/angle_actual
ros2 topic echo /hand_left/touch_data

ros2 topic pub --once /hand_left/speed_set rh56f1_interfaces/msg/SetSpeed1 \
  "{hand_id: 1, joint_values: [1000,1000,1000,1000,1000,1000]}"

ros2 topic pub --once /hand_left/angle_set rh56f1_interfaces/msg/SetAngle1 \
  "{hand_id: 1, joint_values: [-1,-1,-1,1400,-1,-1]}"
```

判定：

- **服务正常**：有 `response:`，且 `message: ok`。
- **通信异常**：有 `response:` 但 `message` 不是 `ok`（ROS 通了，串口/设备没应答好）。
- **服务未就绪**：一直 `waiting for service` → 节点未启动，或仍在跑仓库默认 H1 配置。
- **话题正常**：发 `angle_set` 后，`angle_actual` 对应关节会变化。
- **话题没反应**：检查 `hand_id` 是否等于 yaml 的 `Hand_ID`；其它关节不要填 `0`（可能把该指拉走），应填 `-1`。
