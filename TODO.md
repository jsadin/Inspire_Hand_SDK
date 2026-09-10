# 待办与重构清单（TODO）

> 本文件记录项目的待办事项与重构计划，按优先级分级（P0 最高）。
> 完成一项请把 `[ ]` 改为 `[x]` 并在「已完成」区追加一行说明。
> 架构见 `docs/项目架构说明.md`；加机型见 `docs/新增机型清单.md`；Git 见 `docs/开发与Git约定.md`。

最近更新：2026-09-10

---

## 已完成 ✅

### P0 — 卫生 / 立即收益
- [x] 提交 `.gitignore`，并对历史产物执行 `git rm --cached`
- [x] 移出 30MB 的 `Inspire_Hand_Serial_Driver.zip`（不进源码库）
- [x] 统一配置文件命名（去除中文括号），明确唯一权威配置目录

### P1 — 结构 / 解耦
- [x] 裸库抽成独立 CMake target `inspire_serial_core`，ROS 包改用 `find_package` 依赖，去掉 `../../../` 相对路径
- [x] 引入统一错误类型 `IoError`，贯穿 `Protocol → IRegisterIoBackend → 适配器 → ROS Service 响应`
- [x] 消除双层 `src` 嵌套，重构为 colcon 标准布局（`src/` 下平级 `inspire_serial_core` / `driver` / `interfaces`）
- [x] 全部 58 个 `.srv` 响应段新增 `string message` 字段，错误信息透传给 ROS Service 调用方
- [x] 文档集中到 `docs/`，更新 README 与各说明文档

### P2 — 并发 / 性能（重要）
- [x] **串口并发模型重构**：每设备引入 `DeviceWorker`（请求队列 + 单工作线程），所有读写/组合序列均经 worker **串行化**；`RegisterController` 双回调组 + `MultiThreadedExecutor`；定时读 `read_in_flight_` 背压；删除 `ioPauseTimer`/`pauseTimer`。
- [x] **EG5CD1 组合服务**：`ioWriteSequence` 在 worker 上原子执行（`kCompositeStepMs=3ms`），不再用经验长暂停。

### P3 — 质量 / 功能
- [x] 补单元测试（gtest）：`RingBuffer`、`DeviceWorker`、各协议组帧/解析（含 RH56DFX / EG5CD1 / RH524J1 / EG2-4C2）。`colcon test --packages-select inspire_serial_core` 当前约 **79** 例全绿。代码在 `src/inspire_serial_core/tests/`。
- [x] **接入 CI**：GitHub Actions 执行 `colcon build` + `colcon test` + `clang-format` + `clang-tidy`。测试数量不要写死，用 `grep` 确认至少发现 1 个测试。
- [x] RH56H1 独立接口包 + 触觉 version2（压阻式）协议解析与 `TouchData2` 发布。
- [x] 接入 RH524J1（24 关节腱绳 485）与 EG2-4C2（Serial-CAN 夹爪）。
- [x] **规范文档**：`docs/开发与Git约定.md`、`docs/新增机型清单.md`；README 加入口。

---

## 待办 TODO

### P3 — 质量 / 功能

- [ ] **补 EG-5CD1 的 CANFD 通道**
  - 现状：夹爪目前仅有 485（`protocol.type: EG5CD1`）；核里已有 `RH56F1_canfd` / `RH56H1_canfd` / `RH5DG2_canfd`，缺 `EG5CD1_canfd`。
  - 步骤按 `docs/新增机型清单.md`。

---

## 优先级建议

1. 新机型按 `docs/新增机型清单.md` 拆 PR，勿整包一次合入。
2. 其余功能（如 EG5CD1 CANFD）按需推进。
