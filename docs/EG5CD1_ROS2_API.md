# EG-5CD1 夹爪 ROS2 API 说明

本文档描述 `inspire_control_ros2` 节点在 **`interfaces_profile=EG5CD1`** 下的话题、服务与触觉数据读取方式。485 帧格式与寄存器地址见 [EG5CD1协议格式说明.md](EG5CD1协议格式说明.md)。

---

## 1. 启动

```bash
source /opt/ros/humble/setup.bash
source install/setup.bash

ros2 run inspire_control_ros2 inspire_control_node \
  --device-config src/driver/config/device_protocol_eg5cd1_example.yaml \
  --controller-config src/driver/config/ros2_controller_eg5cd1_example.yaml \
  --device gripper_main
```

| 配置文件 | 作用 |
|----------|------|
| `device_protocol_eg5cd1_example.yaml` | 串口、`protocol.type: EG5CD1`、`Hand_ID` |
| `ros2_controller_eg5cd1_example.yaml` | 话题名、服务名、定时读 `gripperStatusBlock` |

组合服务前缀由 ROS 参数 **`eg5cd1_composite_service_prefix`** 控制，默认 **`/gripper`**。

---

## 2. 话题（发布 / 订阅）

| 话题名 | 类型 | 方向 | 对应寄存器 | 说明 |
|--------|------|------|------------|------|
| `/gripper/state` | `eg5cd1_interfaces/msg/GripperState` | 发布 | `gripperStatusBlock`（1120–1132，14B） | 50Hz 状态：力、开口度、电流、温度、故障、状态码、速度 |
| `/gripper/open_len_set` | `eg5cd1_interfaces/msg/SetInt32` | 订阅 | `openLenSet` | 目标开口度 0–1000 |
| `/gripper/speed_set` | `eg5cd1_interfaces/msg/SetInt32` | 订阅 | `speedSet` | 速度 0–1000 |
| `/gripper/force_set` | `eg5cd1_interfaces/msg/SetInt32` | 订阅 | `forceSet` | 力 0–2000（g） |
| `/gripper/catch_mode_set` | `eg5cd1_interfaces/msg/SetInt32` | 订阅 | `catchModeSet` | 0=位置 / 1=力控 / 2=触控 |
| `/gripper/touch_data` | `eg5cd1_interfaces/msg/GripperTouchSensor` | 发布 | `touchAct`（1200 起 20B） | 左/右指尖触觉，需 `touch_control` + `touch_version: 1` |

**GripperState 字段**（与 1120 起连续读顺序一致）：

```
force_act, open_len_act, current_act, temp_c, error_code, status, speed_act
```

**订阅示例**：

```bash
ros2 topic echo /gripper/state
ros2 topic pub --once /gripper/open_len_set eg5cd1_interfaces/msg/SetInt32 "{hand_id: 1, value: 500}"
```

---

## 3. 服务（单寄存器）

| 服务名 | 类型 | 寄存器 | 说明 |
|--------|------|--------|------|
| `/gripper/clear_error` | `TriggerForHand` | `clearError` | 写 1 清故障（过温除外） |
| `/gripper/save_params` | `TriggerForHand` | `save` | 保存参数到 Flash |
| `/gripper/restore_default` | `TriggerForHand` | `defaultPar` | 恢复出厂参数 |
| `/gripper/stop` | `TriggerForHand` | `stop` | 暂停 |
| `/gripper/touch_close` | `TriggerForHand` | `catchModeClose` | 触控模式夹取触发 |
| `/gripper/touch_open` | `TriggerForHand` | `catchModeOpen` | 触控模式张开触发 |
| `/gripper/set_id` | `SetInt32Value` | `id` | 设置设备 ID |
| `/gripper/set_baud_index` | `SetInt32Value` | `reduRatio` | 波特率索引 0/1/2 |
| `/gripper/set_mode_service` | `SetInt32Value` | `catchModeSet` | 工作模式 0/1/2 |
| `/gripper/get_error` | `GetScalarForHand` | `errorCode` | 读故障码 |
| `/gripper/get_temp` | `GetScalarForHand` | `temp` | 读温度 ℃ |
| `/gripper/get_status` | `GetScalarForHand` | `status` | 读状态码 |

**调用示例**：

```bash
ros2 service call /gripper/get_error eg5cd1_interfaces/srv/GetScalarForHand "{hand_id: 1, query: ''}"
ros2 service call /gripper/clear_error eg5cd1_interfaces/srv/TriggerForHand "{hand_id: 1}"
```

---

## 4. 组合服务（力控 / 触控一键序列）

内部经 `ioWriteSequence` 按文档顺序写 `catchModeSet` → `speedSet` → `forceSet` →（触控）`catchModeClose`/`catchModeOpen`，步骤间隔 3ms。

```bash
# 力控夹取：hand_id=1, speed=600, force=800
ros2 service call /gripper/force_mode_grasp eg5cd1_interfaces/srv/ForceModeGrasp "{hand_id: 1, speed: 600, force: 800}"

# 力控张开：force 为负
ros2 service call /gripper/force_mode_open eg5cd1_interfaces/srv/ForceModeOpen "{hand_id: 1, speed: 600, force: -400}"

# 触控夹取
ros2 service call /gripper/touch_mode_grasp eg5cd1_interfaces/srv/TouchModeGrasp "{hand_id: 1, speed: 600, force: 800}"

# 触控张开（力度仍为正，由寄存器区分）
ros2 service call /gripper/touch_mode_open eg5cd1_interfaces/srv/TouchModeOpen "{hand_id: 1, speed: 600, force: 800}"
```

| 服务 | speed | force 范围 | 说明 |
|------|-------|------------|------|
| `ForceModeGrasp` | 0–1000 | 1–2000 | 力控闭合 |
| `ForceModeOpen` | 0–1000 | -2000–0 | 力控张开 |
| `TouchModeGrasp` | 0–1000 | 0–2000 | 触控夹取 |
| `TouchModeOpen` | 0–1000 | 0–2000 | 触控张开 |

---

## 5. 触觉传感器数据读取

EG-5CD1 指尖触觉寄存器地址 **1200–1218**（法向/切向 int16 + 接近觉 uint32×4B），详见协议文档 §3.4、§9。

### 5.1 寄存器一览

| 建议寄存器名 | 地址 | 长度 | 含义 |
|--------------|------|------|------|
| `normalForceRight` | 1200 | 2B | 法向力（右，int16） |
| `normalForceLeft` | 1202 | 2B | 法向力（左，int16） |
| `tangentialForceDirRight` | 1204 | 2B | 切向力方向（右，int16） |
| `tangentialForceDirLeft` | 1206 | 2B | 切向力方向（左，int16） |
| `proximityRight` | 1208 | **4B** | 接近觉（右，**uint32**） |
| `proximityLeft` | 1212 | **4B** | 接近觉（左，**uint32**） |
| `tangentialForceRight` | 1216 | 2B | 切向力大小（右，int16） |
| `tangentialForceLeft` | 1218 | 2B | 切向力大小（左，int16） |
| `touchAct` | 1200 | **一次读 20 字节**（推荐） |

> **驱动现状**：`ros2_controller_eg5cd1_example.yaml` 已启用 `touch_control`；`/gripper/touch_data` 话题中 `proximity_*` 为 **uint32**。

### 5.2 485 直读（无 ROS，调试用）

自地址 **1200** 读 **20 字节**（见 [EG5CD1协议格式说明.md §9.2](EG5CD1协议格式说明.md)）：

```
主发: EB 90 01 04 00 B0 04 14 [Checksum]
```

解析 20 字节 Data 区：法向/切向为 int16 小端；接近觉为 uint32 小端（偏移 8–11、12–15）。

### 5.3 ROS2：扩展控制器 YAML（按需添加）

在 `ros2_controller_eg5cd1_example.yaml` 的 `services` 中增加读服务（协议层映射寄存器名后生效）：

```yaml
    services:
      # ... 原有服务 ...

      - register_name: "normalForceRight"
        get_service_name: "/gripper/get_touch_normal_force_right"
        is_write_register: false

      - register_name: "normalForceLeft"
        get_service_name: "/gripper/get_touch_normal_force_left"
        is_write_register: false

      - register_name: "proximityRight"
        get_service_name: "/gripper/get_touch_proximity_right"
        is_write_register: false

      - register_name: "proximityLeft"
        get_service_name: "/gripper/get_touch_proximity_left"
        is_write_register: false

      # 其余 tangentialForceDir* / tangentialForce* 同理
```

**定时发布触觉块**（`touch_control` + `touch_version: 1`）：

```yaml
    topics:
      - name: touch_control
        registers:
          read: ["touchAct"]   # 1200 起 20 字节
        state_topic: "/gripper/touch_data"
        touch_version: 1
```

### 5.4 ROS2 读单字段示例（配置服务后）

```bash
# 读右侧法向力（需已在 YAML 注册 normalForceRight）
ros2 service call /gripper/get_touch_normal_force_right \
  eg5cd1_interfaces/srv/GetScalarForHand "{hand_id: 1, query: ''}"

# 读右侧接近觉
ros2 service call /gripper/get_touch_proximity_right \
  eg5cd1_interfaces/srv/GetScalarForHand "{hand_id: 1, query: ''}"
```

### 5.5 调试建议

1. 先确认 `/gripper/state` 正常（串口与 Hand_ID 正确）。
2. 切换 **触控模式**：`catch_mode_set` 写 **2**，或调用 `touch_mode_grasp`。
3. 用 485 抓包或 §9.2 帧直读 1200/20B，观察接近觉、法向力是否随夹取变化。
4. 切向力/方向在物体滑动时变化明显，可用于验证触控增力逻辑。

---

## 6. 相关文档

- [EG5CD1协议格式说明.md](EG5CD1协议格式说明.md) — 帧格式、寄存器表、触觉 §3.4 / §9
- [夹爪485寄存器规则.md](夹爪485寄存器规则.md) — 与协议说明同源寄存器参考
- 示例配置：`src/driver/config/device_protocol_eg5cd1_example.yaml`、`ros2_controller_eg5cd1_example.yaml`
