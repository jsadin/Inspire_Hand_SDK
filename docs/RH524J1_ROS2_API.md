# RH524J1 ROS2 API 说明

本文描述 `inspire_control_ros2` 在 **RH524J1**（`protocol.type: RH524J1_485`）下的关节顺序、话题/服务与自检。帧格式与 RH5DG2 相同（`EB 90`），见 [RH5DG2_485协议格式说明.md](RH5DG2_485协议格式说明.md)；寄存器地址来自 `065demo/hand_param.h`。

启动与短节入口见仓库 [README.md](../README.md)。请用本机型 `*_example.yaml`。

---

因时 RH524J1（065 腱绳驱动，24 自由度）走 **RS485**，帧格式与 RH5DG2 相同（请求 `EB 90`），寄存器地址来自 `065demo/hand_param.h`（`angleSet=320`，一次写 24 路）。

- **协议**：`RH524J1_485_Protocol`（`REGISTER_PROTOCOL("RH524J1_485", …)`）
- **接口包**：`rh524j1_interfaces`（`joint_values` 固定 24 个整数）
- **示例配置**（随包安装到 `share/inspire_control_ros2/config`）：
  - `device_protocol_rh524j1_example.yaml`：`protocol.type: RH524J1_485`
  - `ros2_controller_rh524j1_example.yaml`：话题 `/hand_left/angle_set` 等，服务 `/hand_left/set_angle` 等
- **无触觉**、无力控组合服务。当前机型无 `touchAct`。

**启动**（改代码/配置后必须 **重新编译并重启节点**；正在跑的进程不会出现新服务）：

```bash
# 1) 在正在跑 launch 的终端按 Ctrl+C 停掉节点
# 2) 编译并加载
colcon build --packages-select inspire_control_ros2
source install/setup.bash

# 3) 用 RH524J1 示例配置启动
ros2 launch inspire_control_ros2 inspire_control_single_device.launch.py \
  device_name:=hand_left \
  device_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/device_protocol_rh524j1_example.yaml \
  controller_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/ros2_controller_rh524j1_example.yaml
```

请先只编译本机型相关包，避免工作区里其它 ROS1 包拖垮构建：

```bash
colcon build --packages-select rh524j1_interfaces inspire_serial_core inspire_control_ros2
source install/setup.bash
```

> **`get_currentAct` 一直 `waiting for service`**：节点还是旧进程。先 `Ctrl+C`，再执行上面的 launch。重启后 `ros2 service list | grep get_currentAct` 应能看到服务。不等重启时，电流可用话题：`ros2 topic echo /hand_left/current_actual`。

### `joint_values` 按下标 = 电缸 ID − 1

数组一共 24 个数，**第几个位置就对应几号电缸**：下标 `0` = 电缸 ID1，下标 `23` = 电缸 ID24。不要按手指重新排。

规则：

- **弯曲关节（除 ID1 外）：`0` = 张开，数值越大越弯曲。**
- **小指翻折（ID1 / 下标 0）实机范围 `-150 ~ 0`：`0` = 张开，`-150` = 尽量弯曲（负数方向）。**
- **不想动的关节填 `-1`**（保持当前角度）。**不要填 `0`**：`0` 会把该路拉回张开/零位。
- **只改 1 个关节时**：仅 1 路填角度、其余全 `-1`，SDK 会发**短帧**（与实机 `EB 90 … 05 12 40 01 …` 单路写相同）；多路同时改才发整包 24 路。
- 实机拇指指尖（ID20）和掌指（ID22）接线对调，协议已自动交换。你只要按表填写：下标 19 = 指尖、下标 21 = 掌指。

| 电缸 ID | 下标 | 关节名 | 部位 | 角度范围 |
|---------|------|--------|------|----------|
| 1 | 0 | `pinky_fold` | 小指翻折 | **-150 ~ 0**（0 张开，-150 弯曲） |
| 2 | 1 | `pinky_side` | 小指侧摆 | **-200 ~ 200** |
| 3 | 2 | `ring_side` | 无名指侧摆 | **-200 ~ 200** |
| 4 | 3 | `middle_side` | 中指侧摆 | **-200 ~ 200** |
| 5 | 4 | `index_side` | 食指侧摆 | **-200 ~ 200** |
| 6 | 5 | `pinky_mcp` | 小指掌指 | 0 ~ 900 |
| 7 | 6 | `ring_mcp` | 无名指掌指 | 0 ~ 900 |
| 8 | 7 | `middle_mcp` | 中指掌指 | 0 ~ 900 |
| 9 | 8 | `index_mcp` | 食指掌指 | 0 ~ 900 |
| 10 | 9 | `pinky_pip` | 小指指中 | 0 ~ 900 |
| 11 | 10 | `ring_pip` | 无名指指中 | 0 ~ 900 |
| 12 | 11 | `middle_pip` | 中指指中 | 0 ~ 900 |
| 13 | 12 | `index_pip` | 食指指中 | 0 ~ 900 |
| 14 | 13 | `pinky_dip` | 小指指尖 | 0 ~ 900 |
| 15 | 14 | `ring_dip` | 无名指指尖 | 0 ~ 900 |
| 16 | 15 | `middle_dip` | 中指指尖 | 0 ~ 900 |
| 17 | 16 | `index_dip` | 食指指尖 | 0 ~ 900 |
| 18 | 17 | `thumb_rot` | 拇指旋转 | 0 ~ 1000 |
| 19 | 18 | `thumb_side` | 拇指侧摆 | 0 ~ 1000 |
| 20 | 19 | `thumb_dip` | 拇指指尖 | 0 ~ 900 |
| 21 | 20 | `thumb_pip` | 拇指指中 | 0 ~ 900 |
| 22 | 21 | `thumb_mcp` | 拇指掌指 | 0 ~ 900 |
| 23 | 22 | `wrist_1` | 手腕 1 | 0 ~ 1000 |
| 24 | 23 | `wrist_2` | 手腕 2 | 0 ~ 1000 |

示例：小指掌指（ID6）700、中指掌指（ID8）800、食指指中（ID13）900、食指指尖（ID17）900、拇指掌指（ID22）600，其余张开：

```bash
ros2 service call /hand_left/set_angle rh524j1_interfaces/srv/Setangle \
  "{command: '', hand_id: 1, joint_values: [0,0,0,0,0,700,0,800,0,0,0,0,900,0,0,0,900,0,0,0,0,0,600,0]}"
```

只动拇指旋转（ID18 / 下标 17），其它关节保持不动：

```bash
ros2 service call /hand_left/set_angle rh524j1_interfaces/srv/Setangle \
  "{command: '', hand_id: 1, joint_values: [-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,400,-1,-1,-1,-1,-1,-1]}"
```

`hand_id` 必须和配置文件里的 `Hand_ID` 一致，否则话题会被忽略、服务会返回 `accepted: false`。下面所有命令都假定 `Hand_ID: 1`。24 路数组一律 **下标 = 电缸 ID − 1**。

### 话题一览

节点按 `update_rate`（示例 50 Hz）循环读状态并发布。速度只有写入话题，没有实际速度反馈话题。

| 用途 | 写入话题 | 消息类型 | 读取话题 | 消息类型 | 寄存器 |
|------|----------|----------|----------|----------|--------|
| 角度 | `/hand_left/angle_set` | `SetAngle1` | `/hand_left/angle_actual` | `GetAngleAct1` | `angleSet` / `angleAct` |
| 力 | `/hand_left/force_set` | `SetForce1` | `/hand_left/force_actual` | `GetForceAct1` | `forceSet` / `forceAct` |
| 速度 | `/hand_left/speed_set` | `SetSpeed1` | （无） | — | `speedSet` |
| 电流 | `/hand_left/current_set` | `SetCurrent1` | `/hand_left/current_actual` | `GetCurrentAct1` | `currentSet` / `currentAct` |

无触觉话题（当前机型无 `touchAct`）。**日常位置控制只需「速度 + 角度」**；力接口见下节，可忽略。

> **角度 vs 速度单位**：`angleSet`/`angleAct` 单位为 **0.1°**（如 900=90°）。`speedSet` 为控制器内部 **标量档位**，**无 m/s、°/s 等物理单位**；065 实机满速刻度 **16384**（2¹⁴），0=最慢、16384=满速。下文示例取 **8192（约半速）**。

### 角度话题与服务

弯曲关节 **`0` = 张开**，数值越大越弯曲；**`-1` = 该路保持当前角度**（仅 `angleSet` 支持）。先设速度再设角度更不容易“看起来没动”。

```bash
# --- 话题：写角度 / 读实际角度 ---
ros2 topic pub --once /hand_left/angle_set rh524j1_interfaces/msg/SetAngle1 \
  "{hand_id: 1, joint_values: [0,0,0,0,0,700,0,800,0,0,0,0,900,0,0,0,900,0,0,0,0,0,600,600]}"

ros2 topic echo /hand_left/angle_actual

# --- 服务：写角度 / 读实际角度 ---
ros2 service call /hand_left/set_angle rh524j1_interfaces/srv/Setangle \
  "{command: '', hand_id: 1, joint_values: [0,0,0,0,0,700,0,800,0,0,0,0,900,0,0,0,900,0,0,0,0,0,600,600]}"

ros2 service call /hand_left/get_angleAct rh524j1_interfaces/srv/Getangleact \
  "{query: '', hand_id: 1}"
```

成功时服务返回 `accepted: true`、`message: ok`（读服务看 `message: ok` 和 24 路 `joint_values`）。

### 力话题与服务（可选）

本机型**无独立力/触觉传感器**；`forceSet`/`forceAct` 为 24 路电缸内部张力估算，**不做力控时可不发**。若使用：demo 只按 16 位截断，**SDK 不做量程裁剪**；不想改的路可填 `0` 或与当前值相同。

```bash
ros2 topic pub --once /hand_left/force_set rh524j1_interfaces/msg/SetForce1 \
  "{hand_id: 1, joint_values: [500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500]}"

ros2 topic echo /hand_left/force_actual

ros2 service call /hand_left/set_force rh524j1_interfaces/srv/Setforce \
  "{command: '', hand_id: 1, joint_values: [500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500]}"

ros2 service call /hand_left/get_forceAct rh524j1_interfaces/srv/Getforceact \
  "{query: '', hand_id: 1}"
```

### 速度话题与服务

`speedSet` 为 24 路速度标量（**0~16384**，16384=满速）。运动前建议先设速度，再设角度。示例取 **8192（半速）**；更慢可试 4096，更快可试 12288（勿一次拉满）。没有 `speedAct` 反馈话题。

```bash
ros2 topic pub --once /hand_left/speed_set rh524j1_interfaces/msg/SetSpeed1 \
  "{hand_id: 1, joint_values: [8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192]}"

ros2 service call /hand_left/set_speed rh524j1_interfaces/srv/Setspeed \
  "{command: '', hand_id: 1, joint_values: [8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192]}"
```

### 电流话题与服务

`currentSet` 是 24 路电流保护上限，`currentAct` 是实际电流。写电流保护**不会让手运动**，只改保护值。

**读电流有两种方式**（效果相同，下标 = 电缸 ID − 1）：

```bash
# 方式 1：话题（节点起来就能用，推荐）
ros2 topic echo /hand_left/current_actual

# 方式 2：服务（需用新版配置重启节点后才有）
ros2 service call /hand_left/get_currentAct rh524j1_interfaces/srv/Getcurrentact \
  "{query: '', hand_id: 1}"
```

写电流保护上限：

```bash
ros2 topic pub --once /hand_left/current_set rh524j1_interfaces/msg/SetCurrent1 \
  "{hand_id: 1, joint_values: [800,800,800,800,800,800,800,800,800,800,800,800,800,800,800,800,800,800,800,800,800,800,800,800]}"
```

### 温度、故障码、状态（只读服务）

这三项都是 **24 路**，每个电缸一个数，顺序同样是下标 = 电缸 ID − 1。`0` 通常表示正常/无故障。

```bash
# 温度（℃，只读）
ros2 service call /hand_left/get_temp rh524j1_interfaces/srv/Gettemp \
  "{query: '', hand_id: 1}"

# 故障码（只读；非 0 表示该电缸有故障）
ros2 service call /hand_left/get_errorCode rh524j1_interfaces/srv/Geterror \
  "{query: '', hand_id: 1}"

# 运行状态（只读）
ros2 service call /hand_left/get_status rh524j1_interfaces/srv/Getstatus \
  "{query: '', hand_id: 1}"
```

有故障时先清错再继续运动：

```bash
ros2 service call /hand_left/set_clearError rh524j1_interfaces/srv/Setclearerror \
  "{hand_id: 1, clear_code: 1}"
```

### 系统管理服务

| 服务 | 含义 | 命令示例 |
|------|------|---------|
| `set_id` | 改设备通信 ID（改完后 yaml 的 `Hand_ID` 也要改） | `ros2 service call /hand_left/set_id rh524j1_interfaces/srv/Setid "{hand_id: 1, device_id: 1}"` |
| `set_baudRate` | 改设备波特率索引（改完后 yaml 的 `baudrate` 也要改，常见串口 115200） | `ros2 service call /hand_left/set_baudRate rh524j1_interfaces/srv/Setbaudrate "{hand_id: 1, baudrate: 0}"` |
| `set_clearError` | 写 1 清除故障 | `ros2 service call /hand_left/set_clearError rh524j1_interfaces/srv/Setclearerror "{hand_id: 1, clear_code: 1}"` |
| `set_resetPara` | 写 1 恢复出厂参数 | `ros2 service call /hand_left/set_resetPara rh524j1_interfaces/srv/Setresetpara "{hand_id: 1, confirm: 1}"` |
| `set_defaultSpeed` | 上电默认速度（24 路，标量 0~16384） | `ros2 service call /hand_left/set_defaultSpeed rh524j1_interfaces/srv/Setdefaultspeed "{hand_id: 1, joint_values: [8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192]}"` |
| `set_defaultForceSet` | 上电默认力（24 路） | `ros2 service call /hand_left/set_defaultForceSet rh524j1_interfaces/srv/Setdefaultforceset "{hand_id: 1, joint_values: [500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500,500]}"` |
| `set_mode` | 24 路电缸运行模式 | `ros2 service call /hand_left/set_mode rh524j1_interfaces/srv/Setmode "{command: '', hand_id: 1, joint_values: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]}"` |
| `set_pause` | 写 1 暂停，写 0 继续 | `ros2 service call /hand_left/set_pause rh524j1_interfaces/srv/Setpause "{hand_id: 1, pause_flag: 1}"` |
| `set_stop` | 写 1 急停 | `ros2 service call /hand_left/set_stop rh524j1_interfaces/srv/Setstop "{hand_id: 1, stop_flag: 1}"` |
| `set_gestureForceClb` | 力校准 | `ros2 service call /hand_left/set_gestureForceClb rh524j1_interfaces/srv/Setgestureforceclb "{hand_id: 1, calibration_values: [1]}"` |
| `get_errorCode` | 读 24 路故障码 | `ros2 service call /hand_left/get_errorCode rh524j1_interfaces/srv/Geterror "{query: '', hand_id: 1}"` |
| `get_status` | 读 24 路状态 | `ros2 service call /hand_left/get_status rh524j1_interfaces/srv/Getstatus "{query: '', hand_id: 1}"` |
| `get_temp` | 读 24 路温度 | `ros2 service call /hand_left/get_temp rh524j1_interfaces/srv/Gettemp "{query: '', hand_id: 1}"` |
| `get_currentAct` | 读 24 路实际电流 | `ros2 service call /hand_left/get_currentAct rh524j1_interfaces/srv/Getcurrentact "{query: '', hand_id: 1}"` |
| `get_forceAct` | 读 24 路实际力 | `ros2 service call /hand_left/get_forceAct rh524j1_interfaces/srv/Getforceact "{query: '', hand_id: 1}"` |
| `get_angleAct` | 读 24 路实际角度 | `ros2 service call /hand_left/get_angleAct rh524j1_interfaces/srv/Getangleact "{query: '', hand_id: 1}"` |

> **协议有、当前 ROS 未暴露**：保存 Flash（`save`）、电缸位置 `posSet`/`posAct`（不建议用来设角度）、`speedAct`、手势号 `Setgestureno` 等。无触觉、无动作序列/动作库、无百分比接口。

### RH524J1 话题/服务自检

改配置或重新编译后，**先重启节点**，再另开终端：

```bash
source install/setup.bash

# 确认节点和服务已起来
ros2 node list
ros2 topic list
ros2 service list | grep hand_left

# 只读：温度 / 故障码 / 状态 / 电流 / 力 / 角度
ros2 service call /hand_left/get_temp rh524j1_interfaces/srv/Gettemp "{query: '', hand_id: 1}"
ros2 service call /hand_left/get_errorCode rh524j1_interfaces/srv/Geterror "{query: '', hand_id: 1}"
ros2 service call /hand_left/get_status rh524j1_interfaces/srv/Getstatus "{query: '', hand_id: 1}"
ros2 service call /hand_left/get_currentAct rh524j1_interfaces/srv/Getcurrentact "{query: '', hand_id: 1}"
ros2 service call /hand_left/get_forceAct rh524j1_interfaces/srv/Getforceact "{query: '', hand_id: 1}"
ros2 service call /hand_left/get_angleAct rh524j1_interfaces/srv/Getangleact "{query: '', hand_id: 1}"

# 话题：先看实际值，再发一条速度 + 角度
ros2 topic echo /hand_left/angle_actual
ros2 topic echo /hand_left/current_actual
ros2 topic echo /hand_left/force_actual

ros2 topic pub --once /hand_left/speed_set rh524j1_interfaces/msg/SetSpeed1 \
  "{hand_id: 1, joint_values: [8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192,8192]}"

ros2 topic pub --once /hand_left/angle_set rh524j1_interfaces/msg/SetAngle1 \
  "{hand_id: 1, joint_values: [-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,400,-1,-1,-1,-1,-1,-1]}"
```

判定：

- **服务正常**：有 `response:`，且 `message: ok`。
- **通信异常**：有 `response:` 但 `message` 不是 `ok`（ROS 通了，串口/设备没应答好）。
- **服务未就绪**：一直 `waiting for service` → 节点未启动，或未用 RH524J1 示例配置重启。执行 `ros2 service list | grep get_currentAct`；若没有，请 `Ctrl+C` 停节点后，用 `device_protocol_rh524j1_example.yaml` 与 `ros2_controller_rh524j1_example.yaml` 再启动 `inspire_control_single_device.launch.py`。
- **话题正常**：发 `angle_set` 后，`angle_actual` 对应电缸会变化。
- **话题没反应**：检查 `hand_id` 是否等于 yaml 的 `Hand_ID`；其它关节不要填 `0`（会拉回张开），应填 `-1`。
