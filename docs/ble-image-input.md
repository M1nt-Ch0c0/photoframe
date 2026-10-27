# photoframe BLE 图片输入 v1

当前实现使用 ABI 2.2，功能提交 `e373687` 已推送 main，新增单所有权 L2CAP CoC，保留原 ABI 2.1 GATT 图片协议。
相框 ELF 拥有 PNG 缓冲、SHA-256、收图状态和近期回执；宿主提供鉴权、队列、唯一
App Manager 和同步显示。新 APP 需要配套 ABI 2.2 宿主；分区表和安装包格式不变。
旧宿主图片 opcode 15/16/17 保留，电脑端主路径不使用它们。

## ABI 2.2 CoC 扩展（2026-10-27）

APP 使用 `APP_MANIFEST_COC`，START 注册 4096 字节 SDU 预算及 RX/TX 各四槽的唯一 CoC。
既有 GATT 命令和 64 字节 `PFI1` 回执继续有效；CoC 内使用相同图片命令，不改业务版本号。
宿主无线封套和密钥属于宿主，见兄弟仓 `photopainter-host/docs/abi-2-2-coc.md`。

- BEGIN、COMMIT、CANCEL、QUERY：各返回一条相同的 64 字节业务回执。
- DATA/CHUNK：最大图片部分为协商应用记录上限减 22；典型 4058 字节记录可传 4036 字节图片。
- 成功 CHUNK 不返回逐块回执，依靠 CoC credits 施加背压；错误 CHUNK 的失败结果保留至 COMMIT/QUERY。
- 每次 APP_EVENT_COC 最多处理四条记录；发送队列满时保留待发回执，等待 TX_READY，避免阻塞 Manager。
- GATT 和 CoC 共用同一图片接收状态；接收中的图片绑定入口/session，另一入口不能覆盖它。
- CoC 断线取消尚未提交的接收，保留近期终态回执。COMMIT 已进入显示时，断线不取消显示；重新连接只能 QUERY 原请求。
- 原有整图长度/SHA/PNG 检查、最多 5 MiB、60 秒接收空闲期限和最终 POWER_OFF 成功语义不变。

2026-10-27 已在v20取得一次真实CoC回执：67,509字节发送阶段2.477秒，随后物理刷新与POWER_OFF完成。发送阶段均速26.6 KiB/s，不含全部连接和刷屏时间；不是完整稳定性验收，详细记录见项目Wiki状态页。

## 原 GATT 协议

## 服务与传输

仅运行新版 `photoframe` 时注册服务 `a4fe2000-1536-4c88-a2ce-32ecdaac0471`：

| 索引 | UUID 前缀 | 属性 | 最大值 |
|---|---|---|---:|
| 0 | `a4fe2001` | WRITE，命令 | 128 字节 |
| 1 | `a4fe2002` | READ，当前命令结果 | 64 字节 |

后缀均为 `-1536-4c88-a2ce-32ecdaac0471`。电脑先通过常驻通道双向 HMAC 鉴权，
核对运行应用 ID、服务 UUID、代次和服务版本，再以既有签名 op 14 写命令、op 13 读结果。
直接 GATT 读值没有协议 HMAC，不作为物理成功证据。发现、鉴权和应用 OTA 不依赖此服务。

所有整数是 little endian。每条命令为 `version:u8=1, opcode:u8, request_id:16, tail`。
ID 为电脑生成的非零随机 128 位值。单连接串行执行命令，写完立即签名读取状态。

| opcode | 含义 | tail |
|---:|---|---|
| 1 | BEGIN | `png_size:u32, sha256:32`，总长 54 |
| 2 | CHUNK | `offset:u32, data:1..106`，总长 23..128 |
| 3 | COMMIT | 空，总长 18 |
| 4 | CANCEL | 空，总长 18 |
| 5 | QUERY | 空，总长 18；查相同 ID 的接收状态或完成回执 |

PNG 长度为 8..5242880；最终必须是非隔行 800×480 PNG，每个像素完全不透明，
且只使用精确黑、白、黄、红、蓝、绿。复用原 libpng 解码器，完整解码、验色和检查尾随数据后，
才调用 `app_host_display_v2`。SHA-256 是 APP 内 MIT 实现，按新接纳的数据块增量计算，
不增加 SHA 宿主导入，也不在普通事件中扫描整份 5 MiB 缓冲。

BEGIN 相同 ID、长度和摘要会返回已有接收进度或已有终态；同 ID 不同元数据拒绝。
仅允许一个接收请求，另一 ID 的 BEGIN 返回 busy。新块必须从 received 位置追加；
完全落在已接收范围内、内容相同的重复块可接受。空块、空洞、部分重叠、冲突重复和越界拒绝，
不修改已接收字节。COMMIT 必须收齐且 SHA 匹配。重复 COMMIT 只能返回保留的原终态，不能再次刷新。

## 64 字节结果

| 偏移 | 字节数 | 字段 |
|---:|---:|---|
| 0 | 4 | ASCII `PFI1` |
| 4 | 1 | state |
| 5 | 1 | 本次回复的 opcode，初始为 0 |
| 6 | 2 | error |
| 8 | 16 | request ID |
| 24 | 4 | PNG 总长度 |
| 28 | 4 | 已接收长度 |
| 32 | 32 | PNG SHA-256 |

state：0 idle、1 receiving、2 processing、3 complete、4 failed、5 canceled、
6 missing、7 rejected。只有 **complete + error 0 + ID/长度/摘要完全匹配 + received=total**
证明此请求物理刷新及最终 POWER_OFF 成功。普通签名 WRITE 成功只证明事件执行完成。
rejected 为本命令被拒绝，仍在接收的请求保持原样；其长度和摘要为零，不能当作完成回执。
missing 的 ID 是查询 ID，其余元数据为零。

error：0 OK、1 invalid command、2 busy、3 offset、4 conflict、5 memory、6 SHA mismatch、
7 invalid PNG、8 display failure、9 timeout、10 disconnected。应用协议错误通过结果返回，
BLE 事件本身正常完成，不因坏输入卸载应用。宿主鉴权/代次/接纳错误仍使用宿主既有错误码。

## 生命周期与并发

- BEGIN 用 allowlist 的 `malloc` 申请输入；部署宿主配置 `SPIRAM_USE_MALLOC=y`、
  `SPIRAM_MALLOC_ALWAYSINTERNAL=512`，大分配优先 PSRAM，可失败。5 MiB 的实际归属须用实机堆差验证。
  输入在 COMMIT 解码和同步显示返回前保持有效；完成后释放。逻辑帧和可选缓存使用原有路径。
- 每秒定时检查最近一次有效 BEGIN/CHUNK；空闲达到 60 秒则 canceled/timeout 并释放。
  查询不延长接收租期。CONNECTED 和 DISCONNECTED 都释放未提交接收，以覆盖宿主合并连接边界的情况。
- 同步显示期间断开不会取消已执行的 COMMIT。结果在显示接口返回后写入四项 RAM 环形回执。
  回执跨连接保留，STOP、错误卸载、切换、重启和四项终态淘汰可使其消失。
- STOP 释放输入、清除回执、取消计时并注销服务。宿主取消旧代次事件并在释放 ELF 前销毁服务副本。
  服务发布失败通过 ERROR 清理 APP 资源后返回运行错误。
- 每条 BLE 写入和显示都由唯一 App Manager 串行执行。收图不跨事件占住宿主接纳锁，
  功能键/管理切出仍可执行，随后 STOP 清理；包接收可能临时阻止 APP 事件，APP 超时在下一事件处理。
  定时与网络事件不显示。APP 正接收 BLE 时，HTTP PNG INPUT 返回原有 input 错误（422），不刷新、
  不破坏当前接收；空闲时 HTTP 解码、显示和缓存保持兼容。宿主既有 401/409、安装 A/B 和启动保护未改。

## 客户端恢复规则

Mac/Windows/Linux 新图片发送使用 `NewAppImageSender`，保持单 worker、最新快照、去重和错误节流。
相框未运行或服务不匹配时明确不可用，不切应用、不回退 HTTP、不改用宿主图片协议。
COMMIT 前连接失败可用新随机 ID 整图重传。尝试 COMMIT 前就保存身份；COMMIT 写入或结果读取丢失时，
重连核对原代次和服务版本，只 QUERY 原 ID，不能重发 COMMIT。
切出/重启、missing、不一致回执都使该图片结果不确定，停止该进程对该摘要的自动重试，
即使中间有新快照也保留此决定。状态仅在 RAM，不承诺跨电脑进程重启的永久 exactly-once。

## ABI 审计证据

宿主 `b4709c0` 的 `main/app_ble.c` 复制描述和值，连接边界丢弃旧队列并保留边界事件；
`main/ble_protocol.c` 的 op 13/14 在共享接纳锁内调用；`main/ble_transport.c` 等待
`app_manager_ble_flush()`；`main/app_manager.c` 只在管理任务交付事件；
`main/app_services.c` 的显示接口同步等待硬件并延长看门狗，服务关闭取消 BLE 和 timer；
`main/photoframe_plugin.c` 在 STOP 返回后清理宿主副本再卸载 ELF。
上述已有能力足够，此次未发现必须扩展宿主的缺项。SDK 副本保持 byte-identical。

原生测试命令：`cmake -S tests -B tests/build && cmake --build tests/build && ctest --test-dir tests/build --output-on-failure`。
同套测试另以 ASan/UBSan 编译执行。`test_ble_image.c` 使用真实 APP 入口和 libpng，
验证 SHA 标准向量、百万个 a、分块、重入、HTTP 冲突、断线/超时/STOP/ERROR 释放、
重复 COMMIT、尾随数据和显示失败；原解码器六色、透明、尺寸及隔行回归仍保留。
实际部署与内存证据另记验收记录，不能用原生 mock 显示成功代替实机和人工目测。
