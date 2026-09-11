# RH56H1 ROS2 API 说明

本文描述 `inspire_control_ros2` 在 **RH56H1**（`RH56H1_485` / `RH56H1_canfd`）下的百分比接口、raw 话题/服务与触觉 version2。帧格式与 RH56F1 相同，见 [RH56F1_485协议格式说明.md](RH56F1_485协议格式说明.md)。

启动与短节入口见仓库 [README.md](../README.md)。

---

**RH56H1** 与 **RH56F1** 寄存器与帧格式相同，支持 **485** 与 **CAN-FD** 两种 `protocol.type`；ROS 接口使用**独立包 `rh56h1_interfaces`**（字段与 `rh56f1_interfaces` 一致，架构上与其他机械手对齐，不再复用 F1 包）。二者唯一区别是**触觉传感器类型**：RH56F1 为 version1（电容式），RH56H1 为 **version2（压阻式）**。解码规则（有符号 int16、temp/errCode 取低字节、触觉 version2 布局与 float 合力等）已逐条与厂商参考 `RH56H1_SDK` 核对一致。

**RH56H1 触觉 version2（压阻式）** 已在 `RH56H1_485_Protocol` 中实现（参考 `RH56H1_SDK`），结构与 RH56F1 完全一致，仅 `readTouchData` / `parseTouchData` 两个触觉函数实现 version2 逻辑：
- **寄存器布局**：每根手指 `tip_end` 指端 `2*2=4` 个 int16、`tip_touch` 指尖 `6*5=30` 个 int16、`force` 合力 `x/y/z` 三个 float32；掌心 `palm` `15*6=90` 个 int16。手指顺序 `pinky/ring/middle/index/thumb`。
- **读取方式**：因数据量约 580 字节超过单帧 485 读取上限，按段多次读取（5 指各 3 段 + 掌心 1 段）后拼装再解析。
- **ROS 发布**：专用消息 **`rh56h1_interfaces/msg/TouchData2`** 承载 version2 完整数据（RH56F1 仍用自身的 `TouchData1`）。控制器配置中将 `touch_control` 的 **`touch_version` 设为 2** 时，`RH56H1InterfaceAdapter` 自动发布 `TouchData2`；可参考 **`ros2_controller_rh56h1_example.yaml`**。
- **CAN-FD**：`RH56H1_canfd_Protocol` 继承自 `RH56H1_485_Protocol`，其 `readTouchData` 的 version2 分支按 CAN-FD 合法字节长度（含 >64 自动拆帧）逐段读取后拼装，复用继承来的 `parseTouchData(version=2)` 解析；485 与 CAN-FD 的 version2 表现一致。

## RH56H1 百分比接口（0.0~100.0）

在 `rh56h1_interfaces` 中新增了**百分比**接口，与现有 raw（寄存器原始值）接口**并列存在、互不影响**。百分比接口内部按 RH56H1 用户手册 V1.2 的量程，将 `0.0~100.0` 线性换算成寄存器原始值后，**复用原本的寄存器读写通路**（`ioWriteRegister`/`ioReadRegister`）；读取时反向把原始值换算成百分比返回。超出 `0~100` 的输入会**自动裁剪**。

**位置百分比基于「角度 `angleSet/angleAct`」换算**（手册 2.5.9 明确不建议用电缸位置 `posSet` 设角度，推荐用 `angleSet`）。因各指角度范围不同，位置采用**逐关节**换算，方向约定 **0%=握紧（最小角度）、100%=张开（最大角度）**；速度/力/电流为 0~量程上限的统一线性换算。

| 项目 | 换算寄存器 | 量程（0% ↔ 100%） | 手册依据 | 话题（`SetPercent1`/`GetPercentAct1`，float32[6]） | 服务（`Setpercent`/`Getpercent`，float32[6]） |
|------|-----------|------------------|------|--------------------|------------------|
| 设置位置 | `angleSet` | 逐关节：四指 870→1690、拇指弯曲 950→1350、拇指旋转 700→1700（0%→100%） | 表35（2.5.10） | `/hand_left/pos_percent_set` | `/hand_left/set_pos_percent` |
| 设置速度 | `speedSet` | 0 – 3000（前 5 指） | 表38（2.5.12） | `/hand_left/speed_percent_set` | `/hand_left/set_speed_percent` |
| 设置力 | `forceSet` | 0 – 900（前 5 指） | 表37（2.5.11） | `/hand_left/force_percent_set` | `/hand_left/set_force_percent` |
| 设置电流 | `currentSet` | 0 – 1500（前 5 指） | 表31（2.5.6，电缸电流保护值） | `/hand_left/current_percent_set` | `/hand_left/set_current_percent` |
| 读取位置 | `angleAct` | 逐关节，同「设置位置」范围 | 表40（2.5.14） | `/hand_left/pos_percent_actual`（发布） | `/hand_left/get_pos_percent` |

> **说明与例外**（依据 RH56H1 手册 V1.2）：
> - **位置为何用角度**：`posSet`(0~2000) 是**电缸位置**（0=张开、2000=握紧），手册 2.5.9 明确"不建议用它设定手指位置角度"；`angleSet` 才是推荐的角度寄存器，单位 0.1°，各指范围为四指 870~1690、拇指弯曲 950~1350、拇指旋转 700~1700，**角度越大越张开**。**四指 angleSet 上限 1690，不是 posSet 的 2000**。因此位置百分比按各指范围逐关节换算，`50%` 表示该指角度范围中点。`angle_set` 超范围输入由驱动自动裁剪到手册合法区间。
> - **速度第 6 指（大拇指旋转）例外**：手册大拇指旋转 `speedSet(1057)` 范围为 **0~20**，而非 0~3000，当前代码对该指仍按 0~3000 换算，故 `speed_percent` 对大拇指旋转**不准确**；请对其速度改用 raw 接口（`speedSet`）直接给 0~20。
> - **力 / 电流第 6 指**：手册对大拇指旋转 `forceSet(1051)`、`currentSet(1021)` 标注 `\`（舵机通道未单独定义量程），百分比换算对该指仅为近似。

- **配置**：用 `device_protocol_rh56h1_canfd_example.yaml`（或 485 的 `device_protocol_rh56h1_example.yaml`）+ `ros2_controller_rh56h1_example.yaml`。先改示例里的 `port`、`Hand_ID`。
- **启动**：

```bash
source install/setup.bash
ros2 launch inspire_control_ros2 inspire_control_single_device.launch.py \
  device_name:=hand_left \
  device_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/device_protocol_rh56h1_canfd_example.yaml \
  controller_config:=$(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/ros2_controller_rh56h1_example.yaml
```

### 关节顺序（`joint_values[6]`）

| 下标 | 关节 | 简称 |
|------|------|------|
| 0 | 小拇指 | pinky |
| 1 | 无名指 | ring |
| 2 | 中指 | middle |
| 3 | 食指 | index |
| 4 | 大拇指弯曲 | thumb_bend |
| 5 | 大拇指旋转 | thumb_rot |

### 百分比话题与服务（0.0 ~ 100.0）

方向约定：**位置** `0%`=握紧、`100%`=张开（最大角度）；**速度/力/电流** `0%`=0、`100%`=量程上限。输入超范围自动裁剪。

**位置百分比**（写入 `angleSet`，读取 `angleAct`；**非**电缸 `posSet`）：

| 百分比 | 四指 angleSet | 拇指弯曲 | 拇指旋转 |
|--------|--------------|---------|---------|
| 0%（握紧） | 870 | 950 | 700 |
| 50%（中间） | 1280 | 1150 | 1200 |
| 100%（张开） | 1690 | 1350 | 1700 |

```bash
# --- 位置百分比：话题 ---
ros2 topic pub --once /hand_left/pos_percent_set rh56h1_interfaces/msg/SetPercent1 \
  "{hand_id: 1, joint_values: [50,50,50,50,50,50]}"

ros2 topic pub --once /hand_left/pos_percent_set rh56h1_interfaces/msg/SetPercent1 \
  "{hand_id: 1, joint_values: [100,100,100,100,100,100]}"
# 上式 100% 等价于 angle_set [1690,1690,1690,1690,1350,1700]（完全张开）

ros2 topic echo /hand_left/pos_percent_actual

# --- 位置百分比：服务 ---
ros2 service call /hand_left/set_pos_percent rh56h1_interfaces/srv/Setpercent \
  "{command: '', hand_id: 1, joint_values: [50,50,50,50,50,50]}"

ros2 service call /hand_left/get_pos_percent rh56h1_interfaces/srv/Getpercent \
  "{query: '', hand_id: 1}"

# --- 速度/力/电流百分比：话题（前5指量程见上表；第6指速度见例外说明）---
ros2 topic pub --once /hand_left/speed_percent_set rh56h1_interfaces/msg/SetPercent1 \
  "{hand_id: 1, joint_values: [50,50,50,50,50,50]}"

ros2 topic pub --once /hand_left/force_percent_set rh56h1_interfaces/msg/SetPercent1 \
  "{hand_id: 1, joint_values: [60,60,60,60,60,60]}"

ros2 topic pub --once /hand_left/current_percent_set rh56h1_interfaces/msg/SetPercent1 \
  "{hand_id: 1, joint_values: [50,50,50,50,50,50]}"

# --- 速度/力/电流百分比：服务 ---
ros2 service call /hand_left/set_speed_percent rh56h1_interfaces/srv/Setpercent \
  "{command: '', hand_id: 1, joint_values: [50,50,50,50,50,50]}"

ros2 service call /hand_left/set_force_percent rh56h1_interfaces/srv/Setpercent \
  "{command: '', hand_id: 1, joint_values: [60,60,60,60,60,60]}"

ros2 service call /hand_left/set_current_percent rh56h1_interfaces/srv/Setpercent \
  "{command: '', hand_id: 1, joint_values: [50,50,50,50,50,50]}"
```

> 速度/力/电流**无**百分比读取话题或服务（仅位置有 `pos_percent_actual` / `get_pos_percent`）。

### 原始值(raw)话题与服务

**角度 `angleSet` / `angleAct`**（推荐；单位 0.1°，角度越大越张开，`-1`=该指不动）：

> **勿与 posSet 混淆**：`posSet`（电缸位置）量程 **0~2000**；`angleSet`（角度）四指最大 **1690**，不是 2000。向 `angle_set` 写入 2000 会被驱动**自动裁剪为 1690**（并打 warn 日志）。

| 下标 | angleSet 合法范围 | 物理角度 | 完全张开 | 完全握紧 |
|------|------------------|---------|---------|---------|
| 0~3 四指 | 870 ~ 1690，-1 | 87° ~ 169° | **1690** | **870** |
| 4 拇指弯曲 | 950 ~ 1350，-1 | 95° ~ 135° | **1350** | **950** |
| 5 拇指旋转 | 700 ~ 1700 | 70° ~ 170° | **1700** | **700** |

```bash
ros2 topic pub --once /hand_left/angle_set rh56h1_interfaces/msg/SetAngle1 \
  "{hand_id: 1, joint_values: [1280,1280,1280,1280,1150,1200]}"

ros2 topic pub --once /hand_left/angle_set rh56h1_interfaces/msg/SetAngle1 \
  "{hand_id: 1, joint_values: [870,870,870,870,950,700]}"

ros2 topic echo /hand_left/angle_actual

ros2 service call /hand_left/set_angle rh56h1_interfaces/srv/Setangle \
  "{command: '', hand_id: 1, joint_values: [1280,1280,1280,1280,1150,1200]}"
```

**力 `forceSet` / `forceAct`**（0~4：0~900 g；第6指手册未定义）：

```bash
ros2 topic pub --once /hand_left/force_set rh56h1_interfaces/msg/SetForce1 \
  "{hand_id: 1, joint_values: [600,600,600,600,600,600]}"

ros2 topic echo /hand_left/force_actual

ros2 service call /hand_left/set_force rh56h1_interfaces/srv/Setforce \
  "{command: '', hand_id: 1, joint_values: [600,600,600,600,600,600]}"
```

**速度 `speedSet`**（0~4：0~3000；第6指：**0~20**）：

```bash
ros2 topic pub --once /hand_left/speed_set rh56h1_interfaces/msg/SetSpeed1 \
  "{hand_id: 1, joint_values: [2000,2000,2000,2000,2000,10]}"

ros2 service call /hand_left/set_speed rh56h1_interfaces/srv/Setspeed \
  "{command: '', hand_id: 1, joint_values: [2000,2000,2000,2000,2000,10]}"
```

**电流 `currentSet` / `currentAct`**（0~4：0~1500 mA；第6指手册未定义）：

```bash
ros2 topic pub --once /hand_left/current_set rh56h1_interfaces/msg/SetCurrent1 \
  "{hand_id: 1, joint_values: [800,800,800,800,800,800]}"

ros2 topic echo /hand_left/current_actual
```

**触觉**（只读，`TouchData2`）：

```bash
ros2 topic echo /hand_left/touch_data
```

### 系统管理与只读服务

| 服务 | 范围/含义 | 命令示例 |
|------|----------|---------|
| `set_id` | ID 1~254 | `ros2 service call /hand_left/set_id rh56h1_interfaces/srv/Setid "{hand_id: 1, device_id: 2}"` |
| `set_baudRate` | 0~3（485: 0=115200,1=57600,2=19200；CANFD 见手册） | `ros2 service call /hand_left/set_baudRate rh56h1_interfaces/srv/Setbaudrate "{hand_id: 1, baudrate: 0}"` |
| `set_clearError` | 写 1 清故障 | `ros2 service call /hand_left/set_clearError rh56h1_interfaces/srv/Setclearerror "{hand_id: 1, clear_code: 1}"` |
| `set_save` | 写 1 保存 Flash | `ros2 service call /hand_left/set_save rh56h1_interfaces/srv/Setsave "{hand_id: 1, save_code: 1}"` |
| `set_resetPara` | 写 1 恢复出厂 | `ros2 service call /hand_left/set_resetPara rh56h1_interfaces/srv/Setresetpara "{hand_id: 1, confirm: 1}"` |
| `set_defaultSpeed` | 0~4: 0~3000；第6指 0~20 | `ros2 service call /hand_left/set_defaultSpeed rh56h1_interfaces/srv/Setdefaultspeed "{hand_id: 1, joint_values: [2000,2000,2000,2000,2000,10]}"` |
| `set_defaultForceSet` | 0~4: 0~900 g | `ros2 service call /hand_left/set_defaultForceSet rh56h1_interfaces/srv/Setdefaultforceset "{hand_id: 1, joint_values: [600,600,600,600,600,600]}"` |
| `set_mode` | 0（速度力保护模式） | `ros2 service call /hand_left/set_mode rh56h1_interfaces/srv/Setmode "{command: '', hand_id: 1, joint_values: [0,0,0,0,0,0]}"` |
| `set_pause` | 写 1 暂停 | `ros2 service call /hand_left/set_pause rh56h1_interfaces/srv/Setpause "{hand_id: 1, pause_flag: 1}"` |
| `set_stop` | 写 1 急停 | `ros2 service call /hand_left/set_stop rh56h1_interfaces/srv/Setstop "{hand_id: 1, stop_flag: 1}"` |
| `set_actionSeqIndex` | 动作序列号 | `ros2 service call /hand_left/set_actionSeqIndex rh56h1_interfaces/srv/Setactionseqindex "{hand_id: 1, index: 1}"` |
| `get_errorCode` | 故障位（只读） | `ros2 service call /hand_left/get_errorCode rh56h1_interfaces/srv/Geterror "{query: '', hand_id: 1}"` |
| `get_status` | 状态码 0~8（只读） | `ros2 service call /hand_left/get_status rh56h1_interfaces/srv/Getstatus "{query: '', hand_id: 1}"` |
| `get_temp` | 0~100 ℃（只读） | `ros2 service call /hand_left/get_temp rh56h1_interfaces/srv/Gettemp "{query: '', hand_id: 1}"` |

> **手册有、SDK 未暴露**：电缸位置 `posSet`/`posAct`（0~2000，不建议用于设角度）、速度实际值 `speedAct` 等，需直接操作寄存器或后续扩展配置。
