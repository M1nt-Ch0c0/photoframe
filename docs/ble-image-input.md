# photoframe 图片事务协议

这一页是相框应用 photoframe 接收图片的协议规格，写给要实现客户端或修改 photoframe 收图代码的开发者。读完你会知道每条命令和 64 字节回执的字节格式、事务的状态规则，以及三种蓝牙承载方式各自的差别。

蓝牙连接、配对、会话鉴权和管理命令由宿主固件负责，见 [蓝牙通信](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/06-bluetooth.md)。那一页的“图片事务”一节从客户端角度介绍了同一协议，包括回执丢失后为什么只能查询、不能重发；本页是以源码为准的完整规格。

## 概览

photoframe 自己负责收图：它保存 PNG 缓冲、计算 SHA-256、维护接收状态，并在屏幕刷新完成后生成完成回执。宿主提供鉴权、事件调度和同步显示接口。

| 源文件 | 内容 |
|---|---|
| `main/image_protocol.h` | 常量：opcode、state、error、5 MiB 上限、60 秒空闲期限 |
| `main/image_transfer.c` | 事务核心：解析命令、维护状态、生成回执。不涉及任何传输细节 |
| `main/ble_image.c` | 三个承载适配器（GATT、CoC、通用消息）、1 秒定时器、PNG 解码和显示 |
| `main/entry.c` | 应用入口：清单、START 首屏、事件分发 |

三种承载方式共用同一个事务核心，命令和回执格式完全相同。

photoframe 用 `APP_MANIFEST_CHANNEL` 声明 ABI 2.3，所以需要支持 ABI 2.3 的宿主。协议本身的版本号是命令里的 `version` 字段，当前为 1；回执以 ASCII `PFI1` 开头。

## 命令格式

所有整数都是小端序。每条命令的结构：

| 偏移 | 字节 | 字段 |
|---:|---:|---|
| 0 | 1 | `version`，固定为 1 |
| 1 | 1 | `opcode` |
| 2 | 16 | `request_id`，客户端生成的非零随机值 |
| 18 | 可变 | `tail`，内容取决于 opcode |

| opcode | 命令 | tail | 命令总长 |
|---:|---|---|---|
| 1 | BEGIN | `png_size:u32`，`sha256:32` | 恰好 54 字节 |
| 2 | CHUNK | `offset:u32`，`data`（至少 1 字节） | 至少 23 字节，上限见[三种承载方式](#三种承载方式) |
| 3 | COMMIT | 空 | 恰好 18 字节 |
| 4 | CANCEL | 空 | 恰好 18 字节 |
| 5 | QUERY | 空 | 恰好 18 字节 |

`request_id` 代表一次推图意图。客户端每推一张图都生成新的 ID，同一张图重新推送也用新 ID。

## 64 字节回执

每条命令处理完都产生一份 64 字节回执：

| 偏移 | 字节 | 字段 |
|---:|---:|---|
| 0 | 4 | ASCII `PFI1` |
| 4 | 1 | `state` |
| 5 | 1 | `opcode`，产生这份回执的命令；初始状态为 0 |
| 6 | 2 | `error`，`u16` |
| 8 | 16 | `request_id` |
| 24 | 4 | PNG 总长度 |
| 28 | 4 | 已接收长度 |
| 32 | 32 | PNG 的 SHA-256 |

`state` 的取值：

| 值 | 名称 | 含义 |
|---:|---|---|
| 0 | idle | 还没有任何事务 |
| 1 | receiving | 正在接收 |
| 2 | processing | 已收齐并通过 SHA 校验，正在解码和刷新屏幕 |
| 3 | complete | 屏幕刷新完成 |
| 4 | failed | 失败，`error` 说明原因 |
| 5 | canceled | 已取消 |
| 6 | missing | 找不到这个 ID 的事务或回执 |
| 7 | rejected | 这一条命令被拒绝；正在进行的事务不受影响 |

`error` 的取值：

| 值 | 名称 | 典型原因 |
|---:|---|---|
| 0 | OK | 成功 |
| 1 | invalid | 命令长度、版本或 opcode 不对；ID 全为零；PNG 长度超出范围 |
| 2 | busy | 已有另一个 ID 或另一个承载通道在接收 |
| 3 | offset | CHUNK 偏移不对或越界；COMMIT 时还没收齐 |
| 4 | conflict | 同 ID 的 BEGIN 长度或摘要不同；重复块内容不一致；对没有接收中事务的 ID 发 CHUNK、COMMIT 或 CANCEL |
| 5 | memory | BEGIN 申请缓冲失败，或解码时内存不足 |
| 6 | SHA mismatch | COMMIT 时整图 SHA-256 与 BEGIN 声明的不一致 |
| 7 | invalid PNG | PNG 结构、尺寸、隔行或颜色不合格，或 PNG 结束后还有多余数据 |
| 8 | display failure | 宿主显示接口返回失败 |
| 9 | timeout | 60 秒没有新的 BEGIN 或 CHUNK |
| 10 | disconnected | 接收中途连接断开 |

**怎样判断图片已经显示。** 只有同时满足下面四条的回执，才说明这张图已经刷到屏幕上：`state` 为 3；`error` 为 0；`request_id`、总长度、SHA-256 与请求一致；已接收长度等于总长度。这份回执在宿主显示接口返回之后才生成，而宿主要等电子纸刷新完成并断电（POWER_OFF）才返回，一次刷新大约 20 秒。

`rejected` 回执只带 `request_id`（命令太短无法解析时为零）、`opcode` 和 `error`，长度和摘要字段为零。`missing` 回执只带查询的 `request_id`。这两种回执都不代表事务结束。

## 事务规则

同一时间只有一个事务在接收。一个事务从 BEGIN 开始，以 complete、failed 或 canceled 结束，结束时的回执叫**终态回执**。

### BEGIN

- PNG 长度必须在 8 字节到 5 MiB（5242880 字节）之间。
- 新 ID：photoframe 按 PNG 长度申请缓冲，回执为 `receiving`，已接收长度为 0。缓冲申请失败时回 `rejected` + memory。
- 另一个 ID 正在接收：回 `rejected` + busy。
- 同一 ID 再次 BEGIN（正在接收，或已有终态回执）：长度和摘要都一致时，返回当前进度或原终态回执；不一致时回 `rejected` + conflict。正在接收的同 ID BEGIN 会重新开始 60 秒空闲计时。

### CHUNK

- 只接受正在接收的那个 ID，其他 ID 回 `rejected` + conflict。
- `offset` 必须小于或等于已接收长度，并且 `offset + 数据长度` 不超过总长度，否则回 `rejected` + offset。
- `offset` 等于已接收长度时追加数据，已接收长度随之增加，SHA-256 增量更新。
- `offset` 小于已接收长度时视为重复块：数据必须整块落在已接收范围内且内容完全一致，这时接受但进度不变；否则回 `rejected` + conflict。
- 被拒绝的块不会改动已接收的字节。

### COMMIT

1. 已有这个 ID 的终态回执时，直接返回它。重复 COMMIT 不会再刷一次屏幕。
2. 没有正在接收的同 ID 事务：回 `rejected` + conflict。
3. 还没收齐：回 `rejected` + offset，事务继续接收。
4. SHA-256 不匹配：事务结束，终态为 failed + SHA mismatch。
5. 校验通过后状态变为 `processing`，并立即写进 GATT 回执特征，客户端在刷新期间能读到这个状态。
6. 完整解码 PNG。图片必须是 800×480、非隔行、每个像素完全不透明，只用精确的黑 `#000000`、白 `#ffffff`、黄 `#ffff00`、红 `#ff0000`、蓝 `#0000ff`、绿 `#00ff00`，并且 PNG 结束后没有多余数据。不合格时终态为 failed + invalid PNG，屏幕不刷新。
7. 调用宿主显示接口。成功时终态为 complete，并把这一帧存进宿主为应用保留的缓存，下次启动时恢复；失败时终态为 failed + display failure。

### CANCEL 和 QUERY

- CANCEL 只作用于正在接收的同 ID 事务，终态为 canceled + OK。
- QUERY 按顺序查找：正在接收的同 ID 事务返回当前进度；否则返回保存的终态回执；都没有就回 `missing`。QUERY 不会延长空闲期限。

### 空闲超时与断线

- photoframe 每秒检查一次。距最近一次 BEGIN 或成功的 CHUNK 满 60 秒时，事务结束，终态为 canceled + timeout，缓冲随即释放。这个计时由应用自己完成，和宿主会话是否断开无关。
- 宿主报告蓝牙连接建立或断开时，正在接收的事务结束，终态为 canceled + disconnected。CoC 或通用消息通道自身断开时，只取消在这条通道上接收的事务。
- COMMIT 进入显示后，断开连接不会打断显示。显示结束后终态回执照常保存，客户端重连后用原 ID 发 QUERY 即可取得。

### 终态回执的保存

photoframe 在内存里保留最近 4 份终态回执，跨连接有效。第 5 个事务结束时淘汰最早的一份。应用停止（被切走、相框重启或出错卸载）时，所有回执都会清空。

### 内存

BEGIN 按 PNG 实际长度申请缓冲。宿主把大于 512 字节的内存申请放进 PSRAM，所以图片缓冲不占内部 RAM。事务结束（完成、失败、取消、超时或断线）时缓冲立即释放。

## 三种承载方式

photoframe 在 START 时同时注册三种通道。同一时间只有一个通道能接收图片：一张图正在某个通道（或某个会话）上接收时，其他通道或会话发来的命令回 `rejected` + busy。客户端怎样连接和鉴权，见 [蓝牙通信](https://github.com/M1nt-Ch0c0/esp32s3/blob/main/docs/06-bluetooth.md)。

| | 通用消息（ABI 2.3） | CoC（ABI 2.2） | GATT 服务（ABI 2.1） |
|---|---|---|---|
| photoframe 的注册参数 | 服务 ID `a4fe2000-1536-4c88-a2ce-32ecdaac0471`，单条消息最多 4096 字节，回复 64 字节 | 接收 SDU 4096 字节，收发各 4 个缓冲槽 | 服务 `a4fe2000-1536-4c88-a2ce-32ecdaac0471`，两个特征 |
| 单条命令上限 | 4096 字节；经宿主的 GATT 管理命令承载时最多 4047 字节，即每块 4025 字节图片 | 协商的应用记录上限；常见为 4058 字节，即每块 4036 字节图片 | 128 字节，即每块 106 字节图片 |
| 哪些命令有回复 | 每条命令一份回执，CHUNK 的回执带累计已接收长度 | 只有 BEGIN、COMMIT、CANCEL、QUERY 回复 | 每次写入后，回执特征更新为最新回执 |

### 通用消息

每收到一条消息，photoframe 处理一条命令，并在同一次交换里回复 64 字节回执，客户端不需要另外读取。

### CoC

CoC（L2CAP 面向连接通道）是一条有序的记录流，比 GATT 快。

- 每个事件最多处理 4 条记录，避免长时间占用宿主的事件队列；剩下的记录在后续事件里处理。
- 成功的 CHUNK 不回复，靠 CoC 的流控限制发送速度。
- 本通道正在接收的事务里有 CHUNK 被拒绝时，事务直接结束，终态为 failed，`error` 是那块被拒绝的原因。客户端在 COMMIT 或 QUERY 时会拿到这份回执。
- 发送队列满时，回执先暂存，等宿主报告可以发送后再发；暂存期间不读取新记录。

### GATT 服务

| 特征 | UUID | 属性 | 最大长度 |
|---|---|---|---:|
| 0 | `a4fe2001-1536-4c88-a2ce-32ecdaac0471` | 写，接收命令 | 128 字节 |
| 1 | `a4fe2002-1536-4c88-a2ce-32ecdaac0471` | 读，最新回执 | 64 字节 |

客户端用宿主的签名写命令（op 14）写特征 0，再用签名读命令（op 13）读特征 1。写入特征 0 以外的特征，或超过 128 字节，回 `rejected` + invalid。每个事件（包括每秒的定时检查）结束时，photoframe 都把最新回执写回特征 1。

## 应用生命周期

| 事件 | photoframe 的处理 |
|---|---|
| START | 清空事务状态；依次注册 GATT 服务、写入初始回执、设置 1 秒定时器、打开 CoC、注册通用消息通道，任一步失败都撤销已注册的部分并报告失败。然后从缓存恢复上一帧，没有缓存时显示 `WAITING FOR IMAGE` 等待页 |
| TIMER | 检查空闲超时，更新 GATT 回执 |
| BLE、COC、CHANNEL | 交给对应的承载适配器 |
| INPUT | 宿主兼容图片通道送来的 PNG。正在接收蓝牙图片时返回 `APP_ERR_INPUT`；否则解码、显示并写缓存 |
| STOP | 关闭三种通道、取消定时器、释放缓冲、清空回执 |

宿主报告 GATT 服务出错时，photoframe 撤销全部通道并返回 `APP_ERR_RUNTIME`。坏命令、坏 PNG 这类协议层错误只体现在回执里，事件本身正常完成，应用不会因此被卸载。

## 测试

原生测试 `tests/test_ble_image.c` 在电脑上编译真实的 `entry.c`、`ble_image.c`、`image_transfer.c` 和 libpng 解码器，用模拟的宿主服务覆盖分块、重复块、SHA 校验、多通道竞争、CoC 背压、断线与超时清理、重复 COMMIT 和显示失败。运行方法见 [README](../README.md#测试)。
