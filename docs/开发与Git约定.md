# 开发与 Git 约定

小团队采用 **GitHub Flow**：主干是 `master`，功能在分支上做，用 PR 合回去。不另开 release 分支（要发正式版本号时再议）。

规范管理入口：加新产品见 [新增机型清单.md](新增机型清单.md)；架构见 [项目架构说明.md](项目架构说明.md)。

---

## 1. 谁改什么（不要搬家）

| 目录 | 职责 |
|------|------|
| `src/inspire_serial_core/` | 协议、串口、`DeviceWorker`、配置加载、工厂自注册、gtest |
| `src/driver/` | ROS 节点、机型适配器、launch、**权威 yaml** |
| `src/interfaces/<机型>/` | 每机型独立 `msg`/`srv` 包 |
| `docs/` | 协议说明、架构、本约定；厂商 PDF/DOC **不强制入库** |
| `scripts/` + `.github/workflows/` | clang-format / clang-tidy / CI |

`src/inspire_serial_core/config/` 里的 yaml 仅供裸库示例；ROS 启动以 `src/driver/config/` 为准。

---

## 2. 分支

| 前缀 | 用途 | 例子 |
|------|------|------|
| `feat/<主题>` | 新机型、新功能 | `feat/rh56xx-protocol` |
| `fix/<问题>` | 修缺陷 | `fix/eg5cd1-touch-parse` |
| `docs/<主题>` | 只改文档 | `docs/git-workflow` |

从最新 `master` 拉分支，不要直接在 `master` 上日常开发。能打开 GitHub 分支保护更好：必须走 PR，且 CI 绿才能合。

```bash
git fetch origin
git checkout master
git pull origin master
git checkout -b feat/<主题>
```

---

## 3. 提交信息

格式：`类型: 用中文写为什么`（一句即可，不要堆文件清单）。

| 类型 | 何时用 |
|------|--------|
| `feat` | 新能力、新机型 |
| `fix` | 修 bug |
| `docs` | 文档 |
| `test` | 只加/改测试 |
| `refactor` | 行为不变的结构调整 |
| `ci` | CI 或检查脚本 |

示例：

```
feat: 接入 RH524J1 腱绳手 485 协议与独立接口包
fix: EG5CD1 接近觉按 uint32 解析，避免触觉块错位
docs: 补充加机型清单与 Git 约定
```

一次提交只做一件事。新机型建议拆成 2～3 个 PR，避免整包大冲突：

1. 协议 + 单测  
2. 接口包 + 适配器 + 示例 yaml  
3. README / docs  

---

## 4. Pull Request 与 CI

合入 `master` 用 PR。CI（[`.github/workflows/ci.yml`](../.github/workflows/ci.yml)）必须绿：

- `colcon build`
- `colcon test --packages-select inspire_serial_core`（用 `grep` 确认至少发现 1 个测试，**不要写死用例个数**）
- `./scripts/check_clang_format.sh`
- `./scripts/run_clang_tidy.sh`

本地先跑：

```bash
colcon build --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TESTING=ON
colcon test --packages-select inspire_serial_core
./scripts/check_clang_format.sh
# tidy 需 compile_commands.json
./scripts/run_clang_tidy.sh
```

增量编译后若测试报找不到新协议符号，删掉 `build/inspire_serial_core` 再编一次。

---

## 5. 禁止入库

- `build/`、`install/`、`log/`、`.ros_log/`
- 厂商手册大体积 PDF/DOC（协议要点写成 `docs/*.md`）
- 密钥、`.env`、本机串口调试残留
- 不要对 `master` 做 `push --force`

`.gitignore` 已忽略常见构建产物；新增大文件前先确认不必进库。

---

## 6. 配置文件怎么命名

| 用途 | 文件名 | 说明 |
|------|--------|------|
| 机型示例 | `device_protocol_<model>_example.yaml` + `ros2_controller_<model>_example.yaml` | 已统一为此命名；随包安装，启动时用 launch 参数指向它们 |
| 仓库默认 | `device_protocol_config.yaml` + `ros2_controller_config.yaml` | **当前默认是 RH56H1 CAN-FD**，不要拿它当新产品的唯一配置 |

`protocol.type`：一般写成 `<MODEL>_<bus>`（如 `RH524J1_485`、`EG2_4C2_serial_can`）。**已冻结例外**：EG-5CD1 必须写 `EG5CD1`，不是 `EG5CD1_485`。

一份 device yaml 只有一个 `protocol.type`。左右手同型号时用不同 `port` 和 `Hand_ID`。两台手不要共用同一个 `port`（当前按端口建串口和工人，同口会互相覆盖）。

启动继续用现有 `inspire_control_single_device.launch.py`，不要为每个机型再做一个 launch。
