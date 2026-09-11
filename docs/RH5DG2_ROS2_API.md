# RH5DG2 ROS2 API 说明

本文描述 `inspire_control_ros2` 在 **RH5DG2**（`RH5DG2_485` / `RH5DG2_canfd`）下的 13 关节话题/服务与触觉 version1。帧格式见 [RH5DG2_485协议格式说明.md](RH5DG2_485协议格式说明.md)。

启动与短节入口见仓库 [README.md](../README.md)。请用本机型 `*_example.yaml`。

---

**RH5DG2** 是 13 自由度灵巧手，走 **RS485** 或 **CAN-FD**。多值寄存器一次收发 **13** 个 16 位值（26 字节）。ROS 接口使用独立包 **`rh5dg2_interfaces`**。

- **协议**：`RH5DG2_485_Protocol` / `RH5DG2_canfd_Protocol`
- **接口包**：`rh5dg2_interfaces`（`joint_values` 固定 13 个整数）
- **示例配置**（随包安装到 `share/inspire_control_ros2/config`）：
  - `device_protocol_rh5dg2_example.yaml`：`protocol.type: RH5DG2_485`
  - `ros2_controller_rh5dg2_example.yaml`：话题 `/hand_left/angle_set` 等
- **CAN-FD**：协议已注册为 `RH5DG2_canfd`。仓库没有单独的 canfd 示例 yaml，把设备配置里的 `protocol.type` 改成 `RH5DG2_canfd` 即可，控制器 yaml 不用换。
- **触觉**：version1，发布 `TouchData1`（5 指 + 掌心 9 点）。5 指顺序与 13 路关节数组**不是一一对应**。

**启动**（改代码/配置后必须 **重新编译并重启节点**）：

```bash
colcon build --packages-select rh5dg2_interfaces inspire_serial_core inspire_control_ros2
source install/setup.bash

ros2 launch inspire_control_ros2 inspire_control_single_device.launch.py \
  device_name:=hand_left \
  device_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/device_protocol_rh5dg2_example.yaml \
  controller_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/ros2_controller_rh5dg2_example.yaml
```

`hand_id` 必须和配置文件里的 `Hand_ID` 一致，否则话题会被忽略、服务会返回 `accepted: false`。`hand_id: 0` 视为未指定，本节点仍接受。下面命令都假定 `Hand_ID: 1`。

### `joint_values` 长度必须是 13

数组一共 13 个数，下标 `0`～`12`。现场请按手册标定各路含义；示例 yaml 未配置 `joint_names` 时，状态消息里的名字为空串。

仓库 `examples/RH5DG2.cpp` 的起始姿态（可作联调参考，**不是**出厂零位）：

```
[1800, 1800, 1800, 0, 1800, 0, 1900, 1900, 1900, 1900, 1750, 1600, 2080]
```

示例循环常用范围大约 **965～1800**（前几路）。各路量程可能不同，**不要一次性把 13 路都拉到同一极值**。不确定某路时，先 `echo` 实际角度，再原样回写或填与当前值相同的数。

查看字段：

```bash
ros2 interface show rh5dg2_interfaces/msg/SetAngle1
```

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

示例 yaml 左手已挂 `set_angle`。先设速度再设角度更不容易“看起来没动”。

```bash
# --- 话题：写角度 / 读实际角度 ---
ros2 topic pub --once /hand_left/angle_set rh5dg2_interfaces/msg/SetAngle1 \
  "{hand_id: 1, joint_values: [1800,1800,1800,0,1800,0,1900,1900,1900,1900,1750,1600,2080]}"

ros2 topic echo /hand_left/angle_actual

# --- 服务：写角度 ---
ros2 service call /hand_left/set_angle rh5dg2_interfaces/srv/Setangle \
  "{command: '', hand_id: 1, joint_values: [1800,1800,1800,0,1800,0,1900,1900,1900,1900,1750,1600,2080]}"
```

成功时服务返回 `accepted: true`、`message: ok`。

### 力 / 速度 / 电流话题

示例 yaml 里力、速度、电流以**话题**为主（适配器也支持 `Setforce` / `Setspeed` 服务，但示例未挂）。

```bash
ros2 topic pub --once /hand_left/force_set rh5dg2_interfaces/msg/SetForce1 \
  "{hand_id: 1, joint_values: [500,500,500,500,500,500,500,500,500,500,500,500,500]}"

ros2 topic echo /hand_left/force_actual

ros2 topic pub --once /hand_left/speed_set rh5dg2_interfaces/msg/SetSpeed1 \
  "{hand_id: 1, joint_values: [1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000]}"

ros2 topic pub --once /hand_left/current_set rh5dg2_interfaces/msg/SetCurrent1 \
  "{hand_id: 1, joint_values: [800,800,800,800,800,800,800,800,800,800,800,800,800]}"

ros2 topic echo /hand_left/current_actual
```

写电流保护**不会让手运动**，只改保护值。

### 触觉（version1）

`touch_control` 的 `touch_version` 必须是 **1**。消息 `TouchData1`：

- `finger_forces` / `finger_tangentials` / `finger_angles` / `finger_proximity`：5 个数，顺序 `little, ring, middle, index, thumb`
- `palm_data`：9 个数（掌左 1–3、掌中 4–6、掌右 7–9）
- `finger_angles` 无接触时可能为 `65535`

```bash
ros2 topic echo /hand_left/touch_data
```

### 系统管理与只读服务

下面是 `ros2_controller_rh5dg2_example.yaml` 左手已挂的服务。

| 服务 | 含义 | 命令示例 |
|------|------|---------|
| `set_angle` | 写 13 路角度 | `ros2 service call /hand_left/set_angle rh5dg2_interfaces/srv/Setangle "{command: '', hand_id: 1, joint_values: [1800,1800,1800,0,1800,0,1900,1900,1900,1900,1750,1600,2080]}"` |
| `set_id` | 改设备通信 ID（改完后 yaml 的 `Hand_ID` 也要改） | `ros2 service call /hand_left/set_id rh5dg2_interfaces/srv/Setid "{hand_id: 1, device_id: 1}"` |
| `set_baudRate` | 改波特率索引 | `ros2 service call /hand_left/set_baudRate rh5dg2_interfaces/srv/Setbaudrate "{hand_id: 1, baudrate: 0}"` |
| `set_clearError` | 写 1 清故障 | `ros2 service call /hand_left/set_clearError rh5dg2_interfaces/srv/Setclearerror "{hand_id: 1, clear_code: 1}"` |
| `set_resetPara` | 写 1 恢复出厂 | `ros2 service call /hand_left/set_resetPara rh5dg2_interfaces/srv/Setresetpara "{hand_id: 1, confirm: 1}"` |
| `set_gestureForceClb` | 力传感器校准（须空载） | `ros2 service call /hand_left/set_gestureForceClb rh5dg2_interfaces/srv/Setgestureforceclb "{hand_id: 1, calibration_values: [1]}"` |
| `set_defaultSpeed` | 上电默认速度（13 路） | `ros2 service call /hand_left/set_defaultSpeed rh5dg2_interfaces/srv/Setdefaultspeed "{hand_id: 1, joint_values: [1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000]}"` |
| `set_defaultForceSet` | 上电默认力（13 路） | `ros2 service call /hand_left/set_defaultForceSet rh5dg2_interfaces/srv/Setdefaultforceset "{hand_id: 1, joint_values: [500,500,500,500,500,500,500,500,500,500,500,500,500]}"` |
| `set_mode` | 13 路运行模式 | `ros2 service call /hand_left/set_mode rh5dg2_interfaces/srv/Setmode "{command: '', hand_id: 1, joint_values: [0,0,0,0,0,0,0,0,0,0,0,0,0]}"` |
| `set_pause` | 写 1 暂停，写 0 继续 | `ros2 service call /hand_left/set_pause rh5dg2_interfaces/srv/Setpause "{hand_id: 1, pause_flag: 1}"` |
| `set_stop` | 写 1 急停 | `ros2 service call /hand_left/set_stop rh5dg2_interfaces/srv/Setstop "{hand_id: 1, stop_flag: 1}"` |
| `set_actionSeqIndex` | 动作序列号 | `ros2 service call /hand_left/set_actionSeqIndex rh5dg2_interfaces/srv/Setactionseqindex "{hand_id: 1, index: 1}"` |
| `set_actionLibraryIndex` | 动作库号（本机型特有） | `ros2 service call /hand_left/set_actionLibraryIndex rh5dg2_interfaces/srv/Setactionlibraryindex "{hand_id: 1, index: 1}"` |
| `get_errorCode` | 13 路故障码（只读） | `ros2 service call /hand_left/get_errorCode rh5dg2_interfaces/srv/Geterror "{query: '', hand_id: 1}"` |
| `get_status` | 13 路状态（只读） | `ros2 service call /hand_left/get_status rh5dg2_interfaces/srv/Getstatus "{query: '', hand_id: 1}"` |
| `get_temp` | 13 路温度（只读） | `ros2 service call /hand_left/get_temp rh5dg2_interfaces/srv/Gettemp "{query: '', hand_id: 1}"` |

> **协议有、示例 yaml 未挂**：`save`、力/速度设置服务、`posSet`/`posAct`（不建议用来设角度）、`speedAct`。需要时在控制器 yaml 加一行即可。无百分比接口。

### RH5DG2 话题/服务自检

改配置或重新编译后，**先重启节点**，再另开终端：

```bash
source install/setup.bash

ros2 node list
ros2 topic list
ros2 service list | grep hand_left

ros2 service call /hand_left/get_temp rh5dg2_interfaces/srv/Gettemp "{query: '', hand_id: 1}"
ros2 service call /hand_left/get_errorCode rh5dg2_interfaces/srv/Geterror "{query: '', hand_id: 1}"
ros2 service call /hand_left/get_status rh5dg2_interfaces/srv/Getstatus "{query: '', hand_id: 1}"

ros2 topic echo /hand_left/angle_actual
ros2 topic echo /hand_left/touch_data

ros2 topic pub --once /hand_left/speed_set rh5dg2_interfaces/msg/SetSpeed1 \
  "{hand_id: 1, joint_values: [1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000]}"

ros2 topic pub --once /hand_left/angle_set rh5dg2_interfaces/msg/SetAngle1 \
  "{hand_id: 1, joint_values: [1800,1800,1800,0,1800,0,1900,1900,1900,1900,1750,1600,2080]}"

ros2 service call /hand_left/set_angle rh5dg2_interfaces/srv/Setangle \
  "{command: '', hand_id: 1, joint_values: [1800,1800,1800,0,1800,0,1900,1900,1900,1900,1750,1600,2080]}"
```

判定：

- **服务正常**：有 `response:`，且 `message: ok`。
- **通信异常**：有 `response:` 但 `message` 不是 `ok`（ROS 通了，串口/设备没应答好）。
- **服务未就绪**：一直 `waiting for service` → 节点未启动，或仍在跑仓库默认 H1 配置（6 关节包对不上）。
- **话题正常**：发 `angle_set` 后，`angle_actual` 对应关节会变化。
- **话题没反应**：检查 `hand_id` 是否等于 yaml 的 `Hand_ID`；数组必须正好 13 个数。
