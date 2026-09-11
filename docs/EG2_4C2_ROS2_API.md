# EG2-4C2 夹爪 ROS2 API 说明

本文描述 `inspire_control_ros2` 在 **EG2-4C2**（`protocol.type: EG2_4C2_serial_can`）下的话题、组合服务与运动自检。USB-CAN 转串口的帧格式、ExtId、Modbus 地址见 [4C2夹爪CAN转Serial通信规则.md](4C2夹爪CAN转Serial通信规则.md)。

启动与短节入口见仓库 [README.md](../README.md)。请用本机型 `*_example.yaml`。手册与厂商 demo 冲突时以跑通的 demo 为准。

---

因时 EG2-4C2 电动夹爪通过 **USB-CAN 转串口模块** 与主机通信（详见 `docs/4C2夹爪CAN转Serial通信规则.md` 与 `gripper_demo_can_serial.py`），架构与 RH56DFX_serial_can（同样是 Serial-CAN 灵巧手）和 EG5CD1（同样是单自由度夹爪）对齐：

- **协议实现**：`EG2_4C2_serial_can_Protocol`（`REGISTER_PROTOCOL("EG2_4C2_serial_can", …)`）。
  - 串口封包 21 字节：`AA AA | ExtId(4B 小端) | Data[8] | Meta[4] | Checksum | 55 55`，与 RH56DFX_serial_can 完全一致。
  - 29 位 ExtId 编码（手册第 4 章，与 demo `build_ext_id` 对齐）：
    `ExtId = (op_type & 0x3) << 26 | (reg_addr & 0xFFF) << 14 | (hand_id & 0x3FFF)`，
    `op_type`：00=读、01=写、02=定位、03=随动（实现只用读/写）。
    寄存器地址用 **Modbus 地址**（USB-CAN 串口专用），与手册第五节左列一致。
  - 写帧 Data[8] 不足部分用 `0xFF` 填充；写帧 Meta[0] **固定填 `0x08`**（与 `gripper_demo_can_serial.py:build_write_frame` 第 123 行行为一致：无论写 1/2/3/4 个寄存器，Meta[0] 均为 8）。
  - 读帧 Meta[0] 固定填 `0x01`（与 demo `READ_META = (0x01, 0x00, 0x01, 0x00)` 对齐；手册此处描述与 demo 不一致，**以 demo 实际跑通行为为准**）。
  - 校验和：`sum(ExtId..Meta) & 0xFF`（手册 §3.1 + demo 第 100 行一致）。
  - 应答解析：固定偏移 `frame[6..14]` 为 CAN 数据区，`0xA5` 转义规则同 RH56DFX_serial_can。
- **寄存器**（与 `gripper_demo_can_serial.py:REG` + 状态寄存器一致）：

  | 名称 | Modbus 地址 | 用途 | 默认读取长度 |
  |------|------------|------|-------------|
  | `save` / `defaultPar` | 1001 / 1002 | 保存 / 恢复默认 | 2 |
  | `id` / `baud` | 1003 / 1004 | 设备 ID / 波特率索引 | 2 |
  | `catchMode` | 1005 | 0=一次夹取，1=持续夹取 | 2 |
  | `stop` / `clearError` | 1006 / 1007 | 急停 / 清除故障 | 2 |
  | `openLenSet` / `speedSet` / `forceSet` | 1010 / 1011 / 1012 | 开口度 / 速度 / 力度 | 2 |
  | `maxOpenLen` / `minOpenLen` | 1016 / 1017 | 最大 / 最小开口 | 2 |
  | `forceAct` / `openLenAct` / `currentAct` / `temp` | 1060 / 1061 / 1062 / 1063 | 状态（只读） | 2 |
  | `errorCode` / `status` | 1064 / 1065 | 故障位 / 状态码（只读） | 2 |
  | `gripperStatusBlock` | 1060 | 一次读 1060–1065 共 12 字节（`forceAct / openLenAct / currentAct / temp / errorCode / status`），跨 8+4 两帧 | 12 |

- **示例配置**（随包安装到 `share/inspire_control_ros2/config`）：
  - `device_protocol_eg2_4c2_example.yaml`：`protocol.type: EG2_4C2_serial_can`，串口默认 `/dev/ttyUSB0` @ 115200。
  - `ros2_controller_eg2_4c2_example.yaml`：话题名与适配器约定一致——`gripper_state` / `open_len_set` / `speed_set` / `force_set` / `catch_mode_set`；服务含 `clear_error` / `save_params` / `restore_default` / `stop` / `set_id` / `set_baud_index` / `set_mode_service` / `set_max_open_len` / `set_min_open_len` / `get_error` / `get_temp` / `get_status`。
  - **夹取/张开组合服务**（节点启动后自动创建，前缀由参数 `eg2_4c2_composite_service_prefix` 控制，默认 `/gripper`）：
    - `{prefix}/force_mode_grasp`：夹取，内部写入持续夹取模式和目标开口度 `0`。
      - `speed`：`10～1000`
      - `force`：`100～1000`，必须填正数；数值越大，夹持力越大。
    - `{prefix}/force_mode_open`：张开，内部写入一次夹取模式和目标开口度 `1000`。
      - `speed`：`10～1000`
      - `force`：`-1000～-100`，必须填负数。负号只是 ROS API 用来表示“张开方向”，驱动实际向设备写入其绝对值。
    - `speedSet`、`forceSet` 单独写入只会修改参数，不会让夹爪运动；写入 `openLenSet` 才会触发运动。
    - 重复发送相同目标时，如果夹爪已经完全张开或闭合，不会再次产生肉眼可见的运动。重复测试应按“张开 → 夹取 → 张开”的顺序进行。
    - 服务返回 `accepted=true`、`message='openLenSet: ok'` 表示设备对寄存器写入作出了有效应答，不等于一定产生了位移；应同时查看 `/gripper/state` 中的 `open_len_act` 和 `status`。
    - 整组寄存器写入经 `ioWriteSequence` 在同一设备 worker 上串行执行，不会与定时状态读取交错。
  - **4C2 特有约束**：`catchMode` 只支持 0/1（无触控模式），故**不提供 `TouchModeGrasp/Open`**；当前机型无触觉硬件，`touchAct` 寄存器未实现，调用返回 `NotSupported`。
- **启动示例**：

```bash
ros2 run inspire_control_ros2 inspire_control_node -- \
  --device-config $(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/device_protocol_eg2_4c2_example.yaml \
  --controller-config $(ros2 pkg prefix inspire_control_ros2)/share/inspire_control_ros2/config/ros2_controller_eg2_4c2_example.yaml
```

### EG2-4C2 夹爪运动自检

当 `protocol.type: EG2_4C2_serial_can` 时，建议严格按照下面的顺序测试。先在一个终端持续查看状态：

```bash
source install/setup.bash

ros2 topic echo /gripper/state eg2_4c2_interfaces/msg/GripperState
```

重点查看：

- `open_len_act`：实际开口度，正常范围约为 `0～1000`；`0` 表示最小开口，`1000` 表示最大开口。
- `status`：`1` 已完全张开、`2` 已完全闭合、`3` 已停止、`4` 正在夹取、`5` 正在张开、`6` 夹取物体后力控停止。
- `error_code`：正常应为 `0`。

在另一个终端执行运动测试：

```bash
source install/setup.bash

# 1. 清除可能存在的故障
ros2 service call /gripper/clear_error \
  eg2_4c2_interfaces/srv/TriggerForHand \
  "{hand_id: 1}"

# 2. 张开：force 必须是 -1000～-100 的负数
ros2 service call /gripper/force_mode_open \
  eg2_4c2_interfaces/srv/ForceModeOpen \
  "{hand_id: 1, speed: 600, force: -400}"

# 3. 等夹爪完全张开后再夹取；force 必须是 100～1000 的正数
ros2 service call /gripper/force_mode_grasp \
  eg2_4c2_interfaces/srv/ForceModeGrasp \
  "{hand_id: 1, speed: 600, force: 300}"

# 4. 再次张开，确认能够往返运动
ros2 service call /gripper/force_mode_open \
  eg2_4c2_interfaces/srv/ForceModeOpen \
  "{hand_id: 1, speed: 600, force: -400}"
```

以下张开命令是错误示例，因为 `force` 使用了正数，驱动会返回 `accepted=false`，不会向夹爪发送运动序列：

```bash
ros2 service call /gripper/force_mode_open \
  eg2_4c2_interfaces/srv/ForceModeOpen \
  "{hand_id: 1, speed: 600, force: 900}"
```

若正确命令返回 `accepted=true`，但夹爪没有肉眼可见的运动：

1. 如果 `open_len_act` 已接近命令目标（张开约 `1000`、闭合约 `0`），说明夹爪已经到位，因此重复发送不会再次运动。
2. 如果 `open_len_act` 没有变化，检查电源、CAN 侧 `500 Kbps`、串口 `/dev/ttyUSB0 @ 115200`、设备 ID 和故障码。
3. 如果 `open_len_act` 超出 `0～1000` 或 `status` 长期为 `0`，说明状态数据异常，不能只凭 `accepted=true` 判断夹爪执行成功，应优先排查 USB-CAN 转串口模块的帧格式及 CAN 寄存器地址配置。

📖 **[docs/4C2夹爪CAN转Serial通信规则.md](docs/4C2夹爪CAN转Serial通信规则.md)** 通信协议详细规则。

